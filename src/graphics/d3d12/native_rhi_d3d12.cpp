// D3D12 implementation of the native-render RHI. A thin passthrough onto the
// D3D12 command processor's deferred command list, barrier queue and
// submission counters; the descriptor-field choices here reproduce exactly
// what the scene renderer set when it talked to D3D12 directly, so migrating
// the scene onto the RHI is behavior-preserving on this backend.

#include <rex/graphics/d3d12/native_rhi_d3d12.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

#include <d3dcompiler.h>

#include <rex/graphics/d3d12/command_processor.h>
#include <rex/logging.h>
#include <rex/ui/d3d12/d3d12_provider.h>
#include <rex/ui/d3d12/d3d12_util.h>

namespace rex::graphics::d3d12 {
namespace {

using nrhi::Backend;
using nrhi::Format;
using nrhi::HeapKind;
using nrhi::ResourceState;
using nrhi::TextureKind;

DXGI_FORMAT ToDxgi(Format format) {
  switch (format) {
    case Format::kR8G8B8A8_UNORM:
      return DXGI_FORMAT_R8G8B8A8_UNORM;
    case Format::kR8G8B8A8_UINT:
      return DXGI_FORMAT_R8G8B8A8_UINT;
    case Format::kR8_UNORM:
      return DXGI_FORMAT_R8_UNORM;
    case Format::kR8G8_UNORM:
      return DXGI_FORMAT_R8G8_UNORM;
    case Format::kB5G6R5_UNORM:
      return DXGI_FORMAT_B5G6R5_UNORM;
    case Format::kR16G16_UNORM:
      return DXGI_FORMAT_R16G16_UNORM;
    case Format::kR10G10B10A2_UNORM:
      return DXGI_FORMAT_R10G10B10A2_UNORM;
    case Format::kR32_FLOAT:
      return DXGI_FORMAT_R32_FLOAT;
    case Format::kR32G32_FLOAT:
      return DXGI_FORMAT_R32G32_FLOAT;
    case Format::kR32G32B32_FLOAT:
      return DXGI_FORMAT_R32G32B32_FLOAT;
    case Format::kR32G32B32A32_FLOAT:
      return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case Format::kD32_FLOAT:
      return DXGI_FORMAT_D32_FLOAT;
    case Format::kBC1_UNORM:
      return DXGI_FORMAT_BC1_UNORM;
    case Format::kBC2_UNORM:
      return DXGI_FORMAT_BC2_UNORM;
    case Format::kBC3_UNORM:
      return DXGI_FORMAT_BC3_UNORM;
    case Format::kBC4_UNORM:
      return DXGI_FORMAT_BC4_UNORM;
    case Format::kBC5_UNORM:
      return DXGI_FORMAT_BC5_UNORM;
    default:
      return DXGI_FORMAT_UNKNOWN;
  }
}

Format FromDxgi(DXGI_FORMAT format) {
  switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
      return Format::kR8G8B8A8_UNORM;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
      return Format::kR10G10B10A2_UNORM;
    default:
      return Format::kUnknown;
  }
}

D3D12_RESOURCE_STATES ToStates(ResourceState state,
                               D3D12_RESOURCE_STATES guest_output_state) {
  switch (state) {
    case ResourceState::kRenderTarget:
      return D3D12_RESOURCE_STATE_RENDER_TARGET;
    case ResourceState::kDepthWrite:
      return D3D12_RESOURCE_STATE_DEPTH_WRITE;
    case ResourceState::kPixelShaderResource:
      return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    case ResourceState::kCopySource:
      return D3D12_RESOURCE_STATE_COPY_SOURCE;
    case ResourceState::kCopyDest:
      return D3D12_RESOURCE_STATE_COPY_DEST;
    case ResourceState::kGenericRead:
      return D3D12_RESOURCE_STATE_GENERIC_READ;
    case ResourceState::kGuestOutput:
      return guest_output_state;
    case ResourceState::kCommon:
    default:
      return D3D12_RESOURCE_STATE_COMMON;
  }
}

UINT8 ToShaderComponentMapping(const nrhi::Swizzle swizzle) {
  switch (swizzle) {
    case nrhi::Swizzle::kX:
      return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0;
    case nrhi::Swizzle::kY:
      return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1;
    case nrhi::Swizzle::kZ:
      return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2;
    case nrhi::Swizzle::kW:
      return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3;
    case nrhi::Swizzle::kZero:
      return D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_0;
    case nrhi::Swizzle::kOne:
    default:
      return D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1;
  }
}

D3D12_BLEND ToBlend(nrhi::BlendFactor factor) {
  switch (factor) {
    case nrhi::BlendFactor::kZero:
      return D3D12_BLEND_ZERO;
    case nrhi::BlendFactor::kSrcAlpha:
      return D3D12_BLEND_SRC_ALPHA;
    case nrhi::BlendFactor::kInvSrcAlpha:
      return D3D12_BLEND_INV_SRC_ALPHA;
    case nrhi::BlendFactor::kOne:
    default:
      return D3D12_BLEND_ONE;
  }
}

D3D12_BLEND_OP ToBlendOp(nrhi::BlendOp op) {
  return op == nrhi::BlendOp::kMin ? D3D12_BLEND_OP_MIN : D3D12_BLEND_OP_ADD;
}

D3D12_COMPARISON_FUNC ToCompare(nrhi::CompareFunc func) {
  switch (func) {
    case nrhi::CompareFunc::kLess:
      return D3D12_COMPARISON_FUNC_LESS;
    case nrhi::CompareFunc::kLessEqual:
      return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case nrhi::CompareFunc::kAlways:
    default:
      return D3D12_COMPARISON_FUNC_ALWAYS;
  }
}

class NrDeviceD3D12;

class NrBufferD3D12 : public nrhi::Buffer {
 public:
  uint64_t size() const override { return size_; }

  ID3D12Resource* resource = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS gpu_va = 0;
  void* mapping = nullptr;
  uint64_t size_ = 0;
  HeapKind heap = HeapKind::kDefault;
};

class NrTextureD3D12 : public nrhi::Texture {
 public:
  uint32_t width() const override { return desc.width; }
  uint32_t height() const override { return desc.height; }
  Format format() const override { return desc.format; }

  ID3D12Resource* resource = nullptr;
  nrhi::TextureDesc desc;
  DXGI_FORMAT dxgi_format = DXGI_FORMAT_UNKNOWN;
  // RTV/DSV slots in the device's non-shader-visible heaps, ~0u when absent.
  uint32_t rtv_slot = ~0u;
  uint32_t dsv_slot = ~0u;
  // Guest-output wrappers don't own their resource's lifetime semantics the
  // same way (they hold a ref, but are never destroyed via DestroyDeferred).
  bool is_guest_output = false;
};

class NrTextureViewD3D12 : public nrhi::TextureView {
 public:
  // Slot in the device's non-shader-visible staging CBV_SRV_UAV heap; the
  // SRV descriptor is copied from here into shader-visible binding slots.
  uint32_t staging_slot = ~0u;
  NrTextureD3D12* texture = nullptr;
};

class NrBindingLayoutD3D12 : public nrhi::BindingLayout {
 public:
  ID3D12RootSignature* root_signature = nullptr;
};

class NrShaderD3D12 : public nrhi::Shader {
 public:
  ID3DBlob* blob = nullptr;
};

class NrPipelineD3D12 : public nrhi::Pipeline {
 public:
  ID3D12PipelineState* pso = nullptr;
};

// A simple slot allocator over a fixed-size descriptor heap with
// submission-keyed retirement (mirrors the scene renderer's original
// srv_next/srv_free/retired_srv_slots machinery).
struct SlotAllocator {
  uint32_t capacity = 0;
  uint32_t next = 0;
  std::vector<uint32_t> free_list;
  // (first_slot, count, submission)
  std::vector<std::pair<std::pair<uint32_t, uint32_t>, uint64_t>> retired;

  bool Alloc(uint32_t count, uint32_t* out) {
    if (count == 1 && !free_list.empty()) {
      *out = free_list.back();
      free_list.pop_back();
      return true;
    }
    if (next + count <= capacity) {
      *out = next;
      next += count;
      return true;
    }
    return false;
  }
  void Retire(uint32_t first, uint32_t count, uint64_t submission) {
    retired.emplace_back(std::make_pair(first, count), submission);
  }
  void Drain(uint64_t completed) {
    std::erase_if(retired, [&](const auto& entry) {
      if (entry.second < completed) {
        for (uint32_t i = 0; i < entry.first.second; ++i) {
          free_list.push_back(entry.first.first + i);
        }
        return true;
      }
      return false;
    });
  }
};

class NrCmdD3D12 : public nrhi::Cmd {
 public:
  void SetBindingLayout(nrhi::BindingLayout* layout) override;
  void SetPipeline(nrhi::Pipeline* pipeline) override;
  void SetRootConstants(uint32_t param, uint32_t count, const void* values,
                        uint32_t dest_offset_in_values) override;
  void SetConstantBuffer(uint32_t param, nrhi::Buffer* buffer,
                         uint64_t offset) override;
  void SetBufferSrv(uint32_t param, nrhi::Buffer* buffer,
                    uint64_t offset) override;
  void SetTexture(uint32_t param, nrhi::TextureView* view) override;
  void SetTexturePair(uint32_t param, nrhi::TextureView* first,
                      nrhi::TextureView* second) override;
  void SetTextures(uint32_t param, nrhi::TextureView* const* views,
                   uint32_t count) override;
  void SetRenderTargets(nrhi::Texture* color, nrhi::Texture* depth) override;
  void ClearRenderTarget(nrhi::Texture* color, const float color4[4]) override;
  void ClearDepth(nrhi::Texture* depth, float value) override;
  void SetViewport(const nrhi::Viewport& viewport) override;
  void SetScissor(const nrhi::Rect& rect) override;
  void SetVertexBuffer(nrhi::Buffer* buffer, uint64_t offset,
                       uint32_t size_bytes, uint32_t stride) override;
  void SetIndexBuffer(nrhi::Buffer* buffer, uint64_t offset,
                      uint32_t size_bytes) override;
  void SetPrimitiveTopology(nrhi::PrimitiveTopology topology) override;
  void Draw(uint32_t vertex_count, uint32_t start_vertex) override;
  void DrawIndexed(uint32_t index_count, uint32_t start_index,
                   int32_t base_vertex) override;
  void CopyBufferToTexture(nrhi::Texture* dst, uint32_t mip,
                           uint32_t array_slice, nrhi::Buffer* src,
                           uint64_t src_offset, uint32_t row_pitch,
                           uint32_t width, uint32_t height,
                           uint32_t depth) override;
  void CopyTextureToBuffer(nrhi::Buffer* dst, uint64_t dst_offset,
                           uint32_t row_pitch, nrhi::Texture* src,
                           uint32_t mip, uint32_t width,
                           uint32_t height) override;
  void Barrier(nrhi::Texture* texture, ResourceState before,
               ResourceState after) override;
  void FlushBarriers() override;

  NrDeviceD3D12* device = nullptr;
};

class NrDeviceD3D12 : public nrhi::Device {
 public:
  explicit NrDeviceD3D12(D3D12CommandProcessor* cp) : cp_(cp) {
    const ui::d3d12::D3D12Provider& provider = cp_->GetD3D12Provider();
    device_ = provider.GetDevice();
    cmd_.device = this;

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.NumDescriptors = kStagingViews;
    device_->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&staging_heap_));
    heap_desc.NumDescriptors = kShaderVisibleViews;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    device_->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&srv_heap_));
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.NumDescriptors = kRtvSlots;
    device_->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&rtv_heap_));
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    heap_desc.NumDescriptors = kDsvSlots;
    device_->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&dsv_heap_));

    view_size_ =
        device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    rtv_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    dsv_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    staging_slots_.capacity = kStagingViews;
    srv_slots_.capacity = kShaderVisibleViews;
    rtv_slots_.capacity = kRtvSlots;
    dsv_slots_.capacity = kDsvSlots;
  }

  ~NrDeviceD3D12() override {
    // Callers guarantee GPU idle; release everything immediately.
    DrainRetired(~0ull);
    for (auto& entry : guest_outputs_) {
      entry.second->resource->Release();
      delete entry.second;
    }
    if (staging_heap_) staging_heap_->Release();
    if (srv_heap_) srv_heap_->Release();
    if (rtv_heap_) rtv_heap_->Release();
    if (dsv_heap_) dsv_heap_->Release();
  }

  Backend backend() const override { return Backend::kD3D12; }

  nrhi::Buffer* CreateBuffer(const nrhi::BufferDesc& desc) override {
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = desc.size;
    rd.Height = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    const D3D12_HEAP_PROPERTIES* heap = &ui::d3d12::util::kHeapPropertiesDefault;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    if (desc.heap == HeapKind::kUpload) {
      heap = &ui::d3d12::util::kHeapPropertiesUpload;
      state = D3D12_RESOURCE_STATE_GENERIC_READ;
    } else if (desc.heap == HeapKind::kReadback) {
      heap = &ui::d3d12::util::kHeapPropertiesReadback;
      state = D3D12_RESOURCE_STATE_COPY_DEST;
    }
    ID3D12Resource* resource = nullptr;
    if (FAILED(device_->CreateCommittedResource(
            heap, cp_->GetD3D12Provider().GetHeapFlagCreateNotZeroed(), &rd, state, nullptr,
            IID_PPV_ARGS(&resource)))) {
      REXLOG_ERROR("nrhi-d3d12: buffer creation failed ({} bytes)", desc.size);
      return nullptr;
    }
    auto* buffer = new NrBufferD3D12();
    buffer->resource = resource;
    buffer->gpu_va = resource->GetGPUVirtualAddress();
    buffer->size_ = desc.size;
    buffer->heap = desc.heap;
    return buffer;
  }

  nrhi::Texture* CreateTexture(const nrhi::TextureDesc& desc) override {
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = desc.kind == TextureKind::k3D ? D3D12_RESOURCE_DIMENSION_TEXTURE3D
                                                 : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = desc.width;
    rd.Height = desc.height;
    rd.DepthOrArraySize =
        UINT16(desc.kind == TextureKind::kCube ? 6 : desc.kind == TextureKind::k3D ? desc.depth : 1);
    rd.MipLevels = UINT16(desc.mip_levels);
    rd.Format = ToDxgi(desc.format);
    rd.SampleDesc.Count = desc.sample_count;
    rd.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    D3D12_CLEAR_VALUE clear{};
    const D3D12_CLEAR_VALUE* clear_ptr = nullptr;
    if (desc.usage & nrhi::kTextureUsageRenderTarget) {
      rd.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
      clear.Format = rd.Format;
      std::memcpy(clear.Color, desc.clear_color, sizeof(clear.Color));
      clear_ptr = &clear;
    }
    if (desc.usage & nrhi::kTextureUsageDepthStencil) {
      // The depth resource is created typeless and viewed as D32_FLOAT /
      // R32_FLOAT; a typed depth SRV read the clear value on some drivers
      // (the photo-editor DoF smear); preserve the proven arrangement.
      rd.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
      if (desc.format == Format::kD32_FLOAT) {
        rd.Format = DXGI_FORMAT_R32_TYPELESS;
      }
      clear.Format = DXGI_FORMAT_D32_FLOAT;
      clear.DepthStencil.Depth = desc.clear_depth;
      clear_ptr = &clear;
    }
    ID3D12Resource* resource = nullptr;
    if (FAILED(device_->CreateCommittedResource(
            &ui::d3d12::util::kHeapPropertiesDefault,
            cp_->GetD3D12Provider().GetHeapFlagCreateNotZeroed(), &rd,
            ToStates(desc.initial_state, guest_output_state_), clear_ptr,
            IID_PPV_ARGS(&resource)))) {
      REXLOG_ERROR("nrhi-d3d12: texture creation failed ({}x{} fmt {})", desc.width,
                   desc.height, uint32_t(desc.format));
      return nullptr;
    }
    auto* texture = new NrTextureD3D12();
    texture->resource = resource;
    texture->desc = desc;
    texture->dxgi_format = ToDxgi(desc.format);
    if (desc.usage & nrhi::kTextureUsageRenderTarget) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (rtv_slots_.Alloc(1, &texture->rtv_slot)) {
        device_->CreateRenderTargetView(resource, nullptr, RtvHandle(texture->rtv_slot));
      }
    }
    if (desc.usage & nrhi::kTextureUsageDepthStencil) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (dsv_slots_.Alloc(1, &texture->dsv_slot)) {
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
        dsv.Format = DXGI_FORMAT_D32_FLOAT;
        dsv.ViewDimension = desc.sample_count > 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS
                                                  : D3D12_DSV_DIMENSION_TEXTURE2D;
        device_->CreateDepthStencilView(resource, &dsv, DsvHandle(texture->dsv_slot));
      }
    }
    return texture;
  }

  void* Map(nrhi::Buffer* buffer) override {
    auto* b = static_cast<NrBufferD3D12*>(buffer);
    if (b->mapping == nullptr) {
      const D3D12_RANGE no_read{};
      // Readback buffers are read through mapped memory; upload buffers are
      // write-only (pass an empty read range).
      b->resource->Map(0, b->heap == HeapKind::kReadback ? nullptr : &no_read, &b->mapping);
    }
    return b->mapping;
  }

  void Unmap(nrhi::Buffer* buffer) override {
    auto* b = static_cast<NrBufferD3D12*>(buffer);
    if (b->mapping != nullptr) {
      b->resource->Unmap(0, nullptr);
      b->mapping = nullptr;
    }
  }

  void InvalidateForRead(nrhi::Buffer*, uint64_t, uint64_t) override {}

  void DestroyDeferred(nrhi::Buffer* buffer) override {
    if (buffer == nullptr) return;
    auto* b = static_cast<NrBufferD3D12*>(buffer);
    std::lock_guard<std::mutex> lock(mutex_);
    retired_.emplace_back(RetiredObject{b->resource, nullptr, cp_->GetCurrentSubmission()});
    delete b;
  }

  void DestroyDeferred(nrhi::Texture* texture) override {
    if (texture == nullptr) return;
    auto* t = static_cast<NrTextureD3D12*>(texture);
    std::lock_guard<std::mutex> lock(mutex_);
    const uint64_t submission = cp_->GetCurrentSubmission();
    if (t->rtv_slot != ~0u) rtv_slots_.Retire(t->rtv_slot, 1, submission);
    if (t->dsv_slot != ~0u) dsv_slots_.Retire(t->dsv_slot, 1, submission);
    retired_.emplace_back(RetiredObject{t->resource, nullptr, submission});
    delete t;
  }

  void DestroyDeferred(nrhi::TextureView* view) override {
    if (view == nullptr) return;
    auto* v = static_cast<NrTextureViewD3D12*>(view);
    std::lock_guard<std::mutex> lock(mutex_);
    const uint64_t submission = cp_->GetCurrentSubmission();
    staging_slots_.Retire(v->staging_slot, 1, submission);
    // Retire every shader-visible binding this view participates in.
    std::erase_if(bindings_, [&](auto& entry) {
      for (uint32_t i = 0; i < entry.first.count; ++i) {
        if (entry.first.views[i] == v) {
          srv_slots_.Retire(entry.second.first_slot, entry.second.count, submission);
          return true;
        }
      }
      return false;
    });
    delete v;
  }

  void DestroyDeferred(nrhi::Pipeline* pipeline) override {
    if (pipeline == nullptr) return;
    auto* p = static_cast<NrPipelineD3D12*>(pipeline);
    std::lock_guard<std::mutex> lock(mutex_);
    retired_.emplace_back(RetiredObject{nullptr, p->pso, cp_->GetCurrentSubmission()});
    delete p;
  }

  void DestroyDeferred(nrhi::Shader* shader) override {
    if (shader == nullptr) return;
    auto* s = static_cast<NrShaderD3D12*>(shader);
    if (s->blob) s->blob->Release();
    delete s;
  }

  nrhi::TextureView* CreateTextureView(nrhi::Texture* texture,
                                       const nrhi::TextureViewDesc& desc) override {
    auto* t = static_cast<NrTextureD3D12*>(texture);
    uint32_t slot;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!staging_slots_.Alloc(1, &slot)) {
        REXLOG_ERROR("nrhi-d3d12: staging view heap exhausted");
        return nullptr;
      }
    }
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = desc.format != Format::kUnknown ? ToDxgi(desc.format)
                 : t->desc.format == Format::kD32_FLOAT ? DXGI_FORMAT_R32_FLOAT
                                                        : t->dxgi_format;
    srv.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
        ToShaderComponentMapping(desc.swizzle[0]), ToShaderComponentMapping(desc.swizzle[1]),
        ToShaderComponentMapping(desc.swizzle[2]), ToShaderComponentMapping(desc.swizzle[3]));
    const uint32_t mips = desc.mip_levels == ~0u
                              ? (t->desc.mip_levels - desc.base_mip)
                              : desc.mip_levels;
    switch (desc.dimension) {
      case nrhi::ViewDimension::k2DMS:
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
        break;
      case nrhi::ViewDimension::kCube:
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srv.TextureCube.MostDetailedMip = desc.base_mip;
        srv.TextureCube.MipLevels = mips;
        break;
      case nrhi::ViewDimension::k3D:
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        srv.Texture3D.MostDetailedMip = desc.base_mip;
        srv.Texture3D.MipLevels = mips;
        break;
      case nrhi::ViewDimension::k2D:
      default:
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Texture2D.MostDetailedMip = desc.base_mip;
        srv.Texture2D.MipLevels = mips;
        break;
    }
    device_->CreateShaderResourceView(t->resource, &srv, StagingHandle(slot));
    auto* view = new NrTextureViewD3D12();
    view->staging_slot = slot;
    view->texture = t;
    return view;
  }

  nrhi::BindingLayout* CreateBindingLayout(const nrhi::BindingLayoutDesc& desc) override {
    D3D12_ROOT_PARAMETER params[nrhi::kMaxBindingParams] = {};
    D3D12_DESCRIPTOR_RANGE ranges[nrhi::kMaxBindingParams] = {};
    for (uint32_t i = 0; i < desc.param_count; ++i) {
      const nrhi::BindingParamDesc& p = desc.params[i];
      D3D12_SHADER_VISIBILITY visibility =
          p.visibility == nrhi::Visibility::kVertex  ? D3D12_SHADER_VISIBILITY_VERTEX
          : p.visibility == nrhi::Visibility::kPixel ? D3D12_SHADER_VISIBILITY_PIXEL
                                                     : D3D12_SHADER_VISIBILITY_ALL;
      params[i].ShaderVisibility = visibility;
      switch (p.kind) {
        case nrhi::BindingParamKind::kConstants:
          params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
          params[i].Constants.ShaderRegister = p.shader_register;
          params[i].Constants.Num32BitValues = p.count;
          break;
        case nrhi::BindingParamKind::kConstantBuffer:
          params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
          params[i].Descriptor.ShaderRegister = p.shader_register;
          break;
        case nrhi::BindingParamKind::kBufferSrv:
          params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
          params[i].Descriptor.ShaderRegister = p.shader_register;
          break;
        case nrhi::BindingParamKind::kTextureTable:
          ranges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
          ranges[i].NumDescriptors = p.count;
          ranges[i].BaseShaderRegister = p.shader_register;
          params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
          params[i].DescriptorTable.NumDescriptorRanges = 1;
          params[i].DescriptorTable.pDescriptorRanges = &ranges[i];
          break;
      }
    }
    D3D12_STATIC_SAMPLER_DESC samplers[nrhi::kMaxStaticSamplers] = {};
    for (uint32_t i = 0; i < desc.static_sampler_count; ++i) {
      const nrhi::StaticSamplerDesc& s = desc.static_samplers[i];
      samplers[i].Filter = s.filter == nrhi::Filter::kAnisotropic ? D3D12_FILTER_ANISOTROPIC
                           : s.filter == nrhi::Filter::kLinear
                               ? D3D12_FILTER_MIN_MAG_MIP_LINEAR
                               : D3D12_FILTER_MIN_MAG_MIP_POINT;
      samplers[i].MaxAnisotropy = s.max_anisotropy;
      D3D12_TEXTURE_ADDRESS_MODE mode = s.address == nrhi::AddressMode::kWrap
                                            ? D3D12_TEXTURE_ADDRESS_MODE_WRAP
                                            : D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
      samplers[i].AddressU = mode;
      samplers[i].AddressV = mode;
      samplers[i].AddressW = mode;
      samplers[i].MaxLOD = D3D12_FLOAT32_MAX;
      samplers[i].ShaderRegister = s.shader_register;
      samplers[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    }
    D3D12_ROOT_SIGNATURE_DESC rs{};
    rs.NumParameters = desc.param_count;
    rs.pParameters = params;
    rs.NumStaticSamplers = desc.static_sampler_count;
    rs.pStaticSamplers = samplers;
    rs.Flags = desc.allow_input_layout ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
                                       : D3D12_ROOT_SIGNATURE_FLAG_NONE;
    ID3D12RootSignature* root_signature =
        ui::d3d12::util::CreateRootSignature(cp_->GetD3D12Provider(), rs);
    if (root_signature == nullptr) {
      REXLOG_ERROR("nrhi-d3d12: root signature creation failed");
      return nullptr;
    }
    auto* layout = new NrBindingLayoutD3D12();
    layout->root_signature = root_signature;
    return layout;
  }

  nrhi::Shader* CreateShader(const nrhi::ShaderDesc& desc) override {
    D3D_SHADER_MACRO macros[9] = {};
    uint32_t macro_count = 0;
    if (desc.macros != nullptr) {
      while (desc.macros[macro_count].name != nullptr && macro_count < 8) {
        macros[macro_count].Name = desc.macros[macro_count].name;
        macros[macro_count].Definition = desc.macros[macro_count].value;
        ++macro_count;
      }
    }
    ID3DBlob* blob = nullptr;
    ID3DBlob* errors = nullptr;
    const char* target = desc.stage == nrhi::ShaderStage::kVertex ? "vs_5_0" : "ps_5_0";
    HRESULT hr = D3DCompile(desc.hlsl_source, std::strlen(desc.hlsl_source),
                            desc.name != nullptr ? desc.name : "nrhi_shader",
                            macro_count != 0 ? macros : nullptr, nullptr, desc.entry_point,
                            target, 0, 0, &blob, &errors);
    if (FAILED(hr)) {
      REXLOG_ERROR("nrhi-d3d12: shader compile failed ({} {}): {}",
                   desc.name != nullptr ? desc.name : "?", desc.entry_point,
                   errors != nullptr ? static_cast<const char*>(errors->GetBufferPointer())
                                     : "no error blob");
      if (errors) errors->Release();
      return nullptr;
    }
    if (errors) errors->Release();
    auto* shader = new NrShaderD3D12();
    shader->blob = blob;
    return shader;
  }

  nrhi::Pipeline* CreateGraphicsPipeline(const nrhi::GraphicsPipelineDesc& desc) override {
    auto* layout = static_cast<NrBindingLayoutD3D12*>(desc.layout);
    auto* vs = static_cast<NrShaderD3D12*>(desc.vs);
    auto* ps = static_cast<NrShaderD3D12*>(desc.ps);
    if (layout == nullptr || vs == nullptr || ps == nullptr) return nullptr;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};
    pd.pRootSignature = layout->root_signature;
    pd.VS = {vs->blob->GetBufferPointer(), vs->blob->GetBufferSize()};
    pd.PS = {ps->blob->GetBufferPointer(), ps->blob->GetBufferSize()};
    pd.BlendState.RenderTarget[0].BlendEnable = desc.blend.enable;
    pd.BlendState.RenderTarget[0].SrcBlend = ToBlend(desc.blend.src);
    pd.BlendState.RenderTarget[0].DestBlend = ToBlend(desc.blend.dst);
    pd.BlendState.RenderTarget[0].BlendOp = ToBlendOp(desc.blend.op);
    pd.BlendState.RenderTarget[0].SrcBlendAlpha = ToBlend(desc.blend.src_alpha);
    pd.BlendState.RenderTarget[0].DestBlendAlpha = ToBlend(desc.blend.dst_alpha);
    pd.BlendState.RenderTarget[0].BlendOpAlpha = ToBlendOp(desc.blend.op_alpha);
    pd.BlendState.RenderTarget[0].RenderTargetWriteMask = desc.blend.write_mask;
    pd.SampleMask = UINT_MAX;
    pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pd.RasterizerState.CullMode = desc.cull == nrhi::CullMode::kFront ? D3D12_CULL_MODE_FRONT
                                  : desc.cull == nrhi::CullMode::kBack ? D3D12_CULL_MODE_BACK
                                                                       : D3D12_CULL_MODE_NONE;
    pd.RasterizerState.DepthClipEnable = desc.depth_clip;
    pd.DepthStencilState.DepthEnable = desc.depth.test_enable;
    pd.DepthStencilState.DepthWriteMask = desc.depth.write_enable
                                              ? D3D12_DEPTH_WRITE_MASK_ALL
                                              : D3D12_DEPTH_WRITE_MASK_ZERO;
    pd.DepthStencilState.DepthFunc = ToCompare(desc.depth.func);
    D3D12_INPUT_ELEMENT_DESC elements[16] = {};
    if (desc.input_elements != nullptr && desc.input_element_count != 0 &&
        desc.input_element_count <= 16) {
      for (uint32_t i = 0; i < desc.input_element_count; ++i) {
        const nrhi::InputElementDesc& e = desc.input_elements[i];
        elements[i].SemanticName = e.semantic_name;
        elements[i].SemanticIndex = e.semantic_index;
        elements[i].Format = ToDxgi(e.format);
        elements[i].AlignedByteOffset = e.byte_offset;
        elements[i].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
      }
      pd.InputLayout.pInputElementDescs = elements;
      pd.InputLayout.NumElements = desc.input_element_count;
    }
    pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    if (desc.rtv_format != Format::kUnknown) {
      pd.NumRenderTargets = 1;
      pd.RTVFormats[0] = ToDxgi(desc.rtv_format);
    }
    if (desc.dsv_format != Format::kUnknown) {
      pd.DSVFormat = ToDxgi(desc.dsv_format);
    }
    pd.SampleDesc.Count = desc.sample_count;
    ID3D12PipelineState* pso = nullptr;
    if (FAILED(device_->CreateGraphicsPipelineState(&pd, IID_PPV_ARGS(&pso)))) {
      REXLOG_ERROR("nrhi-d3d12: graphics pipeline creation failed");
      return nullptr;
    }
    auto* pipeline = new NrPipelineD3D12();
    pipeline->pso = pso;
    return pipeline;
  }

  uint32_t GetSupportedSampleCount(Format format, uint32_t desired) override {
    DXGI_FORMAT dxgi = ToDxgi(format);
    uint32_t count = desired;
    while (count > 1) {
      D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS levels{};
      levels.Format = dxgi;
      levels.SampleCount = count;
      if (SUCCEEDED(device_->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
                                                 &levels, sizeof(levels))) &&
          levels.NumQualityLevels > 0) {
        break;
      }
      count >>= 1;
    }
    return std::max(count, 1u);
  }

  uint64_t CurrentSubmission() const override { return cp_->GetCurrentSubmission(); }
  uint64_t CompletedSubmission() const override { return cp_->GetCompletedSubmission(); }

  // --- frame handling (called from the command processor) ---

  nrhi::Cmd* BeginFrame(ID3D12Resource* guest_output_resource, DXGI_FORMAT guest_output_format,
                        D3D12_RESOURCE_STATES guest_output_internal_state, uint32_t width,
                        uint32_t height, nrhi::Texture** guest_output_out) {
    guest_output_state_ = guest_output_internal_state;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      DrainRetired(cp_->GetCompletedSubmission());
    }
    NrTextureD3D12*& wrapper = guest_outputs_[guest_output_resource];
    if (wrapper != nullptr &&
        (wrapper->desc.width != width || wrapper->desc.height != height)) {
      // Same resource pointer, different geometry: a recreated image reusing
      // the address. Retire the stale wrapper's views and slots.
      DestroyGuestOutputWrapper(wrapper);
      wrapper = nullptr;
    }
    if (wrapper == nullptr) {
      wrapper = new NrTextureD3D12();
      guest_output_resource->AddRef();
      wrapper->resource = guest_output_resource;
      wrapper->desc.width = width;
      wrapper->desc.height = height;
      wrapper->desc.format = FromDxgi(guest_output_format);
      wrapper->desc.usage = nrhi::kTextureUsageRenderTarget;
      wrapper->dxgi_format = guest_output_format;
      wrapper->is_guest_output = true;
      std::lock_guard<std::mutex> lock(mutex_);
      if (rtv_slots_.Alloc(1, &wrapper->rtv_slot)) {
        device_->CreateRenderTargetView(guest_output_resource, nullptr,
                                        RtvHandle(wrapper->rtv_slot));
      }
      // The presenter keeps a small guest-output mailbox; evict wrappers of
      // resources it has replaced.
      if (guest_outputs_.size() > 6) {
        for (auto it = guest_outputs_.begin(); it != guest_outputs_.end();) {
          if (it->second != wrapper && it->first != guest_output_resource) {
            DestroyGuestOutputWrapperLocked(it->second);
            it = guest_outputs_.erase(it);
          } else {
            ++it;
          }
        }
      }
    }
    *guest_output_out = wrapper;
    return &cmd_;
  }

  // --- internals shared with NrCmdD3D12 ---

  D3D12_CPU_DESCRIPTOR_HANDLE StagingHandle(uint32_t slot) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        staging_heap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += size_t(slot) * view_size_;
    return handle;
  }
  D3D12_CPU_DESCRIPTOR_HANDLE SrvCpuHandle(uint32_t slot) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = srv_heap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += size_t(slot) * view_size_;
    return handle;
  }
  D3D12_GPU_DESCRIPTOR_HANDLE SrvGpuHandle(uint32_t slot) const {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = srv_heap_->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += size_t(slot) * view_size_;
    return handle;
  }
  D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle(uint32_t slot) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += size_t(slot) * rtv_size_;
    return handle;
  }
  D3D12_CPU_DESCRIPTOR_HANDLE DsvHandle(uint32_t slot) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = dsv_heap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += size_t(slot) * dsv_size_;
    return handle;
  }

  // Shader-visible binding for an ordered view tuple: consecutive slots
  // holding copies of the staging descriptors, cached and retired when a
  // participating view is destroyed. Render thread only.
  bool GetBinding(NrTextureViewD3D12* const* views, uint32_t count,
                  D3D12_GPU_DESCRIPTOR_HANDLE* gpu_out) {
    if (count == 0 || count > nrhi::kMaxTextureTableSize || views[0] == nullptr) {
      return false;
    }
    BindingKey key{};
    for (uint32_t i = 0; i < count; ++i) {
      key.views[i] = views[i];
    }
    key.count = count;
    auto it = bindings_.find(key);
    if (it == bindings_.end()) {
      uint32_t first_slot;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!srv_slots_.Alloc(count, &first_slot)) {
          REXLOG_ERROR("nrhi-d3d12: shader-visible view heap exhausted");
          return false;
        }
      }
      for (uint32_t i = 0; i < count; ++i) {
        if (views[i] == nullptr) continue;
        device_->CopyDescriptorsSimple(1, SrvCpuHandle(first_slot + i),
                                       StagingHandle(views[i]->staging_slot),
                                       D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
      }
      it = bindings_.emplace(key, Binding{first_slot, count}).first;
    }
    *gpu_out = SrvGpuHandle(it->second.first_slot);
    return true;
  }

  D3D12CommandProcessor* cp() { return cp_; }
  ID3D12DescriptorHeap* srv_heap() { return srv_heap_; }
  D3D12_RESOURCE_STATES guest_output_state() const { return guest_output_state_; }

 private:
  static constexpr uint32_t kStagingViews = 16384;
  static constexpr uint32_t kShaderVisibleViews = 32768;
  static constexpr uint32_t kRtvSlots = 64;
  static constexpr uint32_t kDsvSlots = 8;

  struct RetiredObject {
    ID3D12Resource* resource;
    ID3D12PipelineState* pso;
    uint64_t submission;
  };
  struct BindingKey {
    NrTextureViewD3D12* views[nrhi::kMaxTextureTableSize];
    uint32_t count;
    bool operator<(const BindingKey& other) const {
      if (count != other.count) return count < other.count;
      for (uint32_t i = 0; i < count; ++i) {
        if (views[i] != other.views[i]) return views[i] < other.views[i];
      }
      return false;
    }
  };
  struct Binding {
    uint32_t first_slot;
    uint32_t count;
  };

  void DrainRetired(uint64_t completed) {
    std::erase_if(retired_, [&](const RetiredObject& r) {
      if (r.submission < completed) {
        if (r.resource) r.resource->Release();
        if (r.pso) r.pso->Release();
        return true;
      }
      return false;
    });
    staging_slots_.Drain(completed);
    srv_slots_.Drain(completed);
    rtv_slots_.Drain(completed);
    dsv_slots_.Drain(completed);
  }

  void DestroyGuestOutputWrapperLocked(NrTextureD3D12* wrapper) {
    const uint64_t submission = cp_->GetCurrentSubmission();
    if (wrapper->rtv_slot != ~0u) rtv_slots_.Retire(wrapper->rtv_slot, 1, submission);
    retired_.emplace_back(RetiredObject{wrapper->resource, nullptr, submission});
    delete wrapper;
  }
  void DestroyGuestOutputWrapper(NrTextureD3D12* wrapper) {
    std::lock_guard<std::mutex> lock(mutex_);
    DestroyGuestOutputWrapperLocked(wrapper);
  }

  D3D12CommandProcessor* cp_;
  ID3D12Device* device_ = nullptr;
  NrCmdD3D12 cmd_;

  ID3D12DescriptorHeap* staging_heap_ = nullptr;
  ID3D12DescriptorHeap* srv_heap_ = nullptr;
  ID3D12DescriptorHeap* rtv_heap_ = nullptr;
  ID3D12DescriptorHeap* dsv_heap_ = nullptr;
  uint32_t view_size_ = 0;
  uint32_t rtv_size_ = 0;
  uint32_t dsv_size_ = 0;

  // mutex_ guards the slot allocators and retired_ (creation and deferred
  // destruction are thread-safe); bindings_ is render-thread-only.
  std::mutex mutex_;
  SlotAllocator staging_slots_;
  SlotAllocator srv_slots_;
  SlotAllocator rtv_slots_;
  SlotAllocator dsv_slots_;
  std::vector<RetiredObject> retired_;
  std::map<BindingKey, Binding> bindings_;
  std::map<ID3D12Resource*, NrTextureD3D12*> guest_outputs_;
  D3D12_RESOURCE_STATES guest_output_state_ = D3D12_RESOURCE_STATE_COMMON;
};

void NrCmdD3D12::SetBindingLayout(nrhi::BindingLayout* layout) {
  auto* l = static_cast<NrBindingLayoutD3D12*>(layout);
  DeferredCommandList& list = device->cp()->GetDeferredCommandList();
  list.SetDescriptorHeaps(device->srv_heap(), nullptr);
  list.D3DSetGraphicsRootSignature(l->root_signature);
}

void NrCmdD3D12::SetPipeline(nrhi::Pipeline* pipeline) {
  device->cp()->GetDeferredCommandList().D3DSetPipelineState(
      static_cast<NrPipelineD3D12*>(pipeline)->pso);
}

void NrCmdD3D12::SetRootConstants(uint32_t param, uint32_t count, const void* values,
                                  uint32_t dest_offset_in_values) {
  device->cp()->GetDeferredCommandList().D3DSetGraphicsRoot32BitConstants(
      param, count, values, dest_offset_in_values);
}

void NrCmdD3D12::SetConstantBuffer(uint32_t param, nrhi::Buffer* buffer, uint64_t offset) {
  device->cp()->GetDeferredCommandList().D3DSetGraphicsRootConstantBufferView(
      param, static_cast<NrBufferD3D12*>(buffer)->gpu_va + offset);
}

void NrCmdD3D12::SetBufferSrv(uint32_t param, nrhi::Buffer* buffer, uint64_t offset) {
  device->cp()->GetDeferredCommandList().D3DSetGraphicsRootShaderResourceView(
      param, static_cast<NrBufferD3D12*>(buffer)->gpu_va + offset);
}

void NrCmdD3D12::SetTexture(uint32_t param, nrhi::TextureView* view) {
  NrTextureViewD3D12* views[1] = {static_cast<NrTextureViewD3D12*>(view)};
  D3D12_GPU_DESCRIPTOR_HANDLE handle;
  if (device->GetBinding(views, 1, &handle)) {
    device->cp()->GetDeferredCommandList().D3DSetGraphicsRootDescriptorTable(param, handle);
  }
}

void NrCmdD3D12::SetTexturePair(uint32_t param, nrhi::TextureView* first,
                                nrhi::TextureView* second) {
  NrTextureViewD3D12* views[2] = {static_cast<NrTextureViewD3D12*>(first),
                                  static_cast<NrTextureViewD3D12*>(second)};
  D3D12_GPU_DESCRIPTOR_HANDLE handle;
  if (device->GetBinding(views, 2, &handle)) {
    device->cp()->GetDeferredCommandList().D3DSetGraphicsRootDescriptorTable(param, handle);
  }
}

void NrCmdD3D12::SetTextures(uint32_t param, nrhi::TextureView* const* views, uint32_t count) {
  NrTextureViewD3D12* typed[nrhi::kMaxTextureTableSize] = {};
  if (count > nrhi::kMaxTextureTableSize) return;
  for (uint32_t i = 0; i < count; ++i) {
    typed[i] = static_cast<NrTextureViewD3D12*>(views[i]);
  }
  D3D12_GPU_DESCRIPTOR_HANDLE handle;
  if (device->GetBinding(typed, count, &handle)) {
    device->cp()->GetDeferredCommandList().D3DSetGraphicsRootDescriptorTable(param, handle);
  }
}

void NrCmdD3D12::SetRenderTargets(nrhi::Texture* color, nrhi::Texture* depth) {
  D3D12_CPU_DESCRIPTOR_HANDLE rtv;
  D3D12_CPU_DESCRIPTOR_HANDLE dsv;
  const D3D12_CPU_DESCRIPTOR_HANDLE* rtv_ptr = nullptr;
  const D3D12_CPU_DESCRIPTOR_HANDLE* dsv_ptr = nullptr;
  UINT num_rtvs = 0;
  if (color != nullptr) {
    rtv = device->RtvHandle(static_cast<NrTextureD3D12*>(color)->rtv_slot);
    rtv_ptr = &rtv;
    num_rtvs = 1;
  }
  if (depth != nullptr) {
    dsv = device->DsvHandle(static_cast<NrTextureD3D12*>(depth)->dsv_slot);
    dsv_ptr = &dsv;
  }
  device->cp()->GetDeferredCommandList().D3DOMSetRenderTargets(num_rtvs, rtv_ptr, FALSE,
                                                               dsv_ptr);
}

void NrCmdD3D12::ClearRenderTarget(nrhi::Texture* color, const float color4[4]) {
  device->cp()->GetDeferredCommandList().D3DClearRenderTargetView(
      device->RtvHandle(static_cast<NrTextureD3D12*>(color)->rtv_slot), color4, 0, nullptr);
}

void NrCmdD3D12::ClearDepth(nrhi::Texture* depth, float value) {
  device->cp()->GetDeferredCommandList().D3DClearDepthStencilView(
      device->DsvHandle(static_cast<NrTextureD3D12*>(depth)->dsv_slot),
      D3D12_CLEAR_FLAG_DEPTH, value, 0, 0, nullptr);
}

void NrCmdD3D12::SetViewport(const nrhi::Viewport& viewport) {
  D3D12_VIEWPORT vp;
  vp.TopLeftX = viewport.x;
  vp.TopLeftY = viewport.y;
  vp.Width = viewport.width;
  vp.Height = viewport.height;
  vp.MinDepth = viewport.min_depth;
  vp.MaxDepth = viewport.max_depth;
  device->cp()->GetDeferredCommandList().RSSetViewport(vp);
}

void NrCmdD3D12::SetScissor(const nrhi::Rect& rect) {
  D3D12_RECT r;
  r.left = rect.left;
  r.top = rect.top;
  r.right = rect.right;
  r.bottom = rect.bottom;
  device->cp()->GetDeferredCommandList().RSSetScissorRect(r);
}

void NrCmdD3D12::SetVertexBuffer(nrhi::Buffer* buffer, uint64_t offset, uint32_t size_bytes,
                                 uint32_t stride) {
  D3D12_VERTEX_BUFFER_VIEW view;
  view.BufferLocation = static_cast<NrBufferD3D12*>(buffer)->gpu_va + offset;
  view.SizeInBytes = size_bytes;
  view.StrideInBytes = stride;
  device->cp()->GetDeferredCommandList().D3DIASetVertexBuffers(0, 1, &view);
}

void NrCmdD3D12::SetIndexBuffer(nrhi::Buffer* buffer, uint64_t offset, uint32_t size_bytes) {
  D3D12_INDEX_BUFFER_VIEW view;
  view.BufferLocation = static_cast<NrBufferD3D12*>(buffer)->gpu_va + offset;
  view.SizeInBytes = size_bytes;
  view.Format = DXGI_FORMAT_R16_UINT;
  device->cp()->GetDeferredCommandList().D3DIASetIndexBuffer(&view);
}

void NrCmdD3D12::SetPrimitiveTopology(nrhi::PrimitiveTopology topology) {
  device->cp()->GetDeferredCommandList().D3DIASetPrimitiveTopology(
      topology == nrhi::PrimitiveTopology::kTriangleStrip ? D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
                                                          : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void NrCmdD3D12::Draw(uint32_t vertex_count, uint32_t start_vertex) {
  device->cp()->GetDeferredCommandList().D3DDrawInstanced(vertex_count, 1, start_vertex, 0);
}

void NrCmdD3D12::DrawIndexed(uint32_t index_count, uint32_t start_index, int32_t base_vertex) {
  device->cp()->GetDeferredCommandList().D3DDrawIndexedInstanced(index_count, 1, start_index,
                                                                 base_vertex, 0);
}

void NrCmdD3D12::CopyBufferToTexture(nrhi::Texture* dst, uint32_t mip, uint32_t array_slice,
                                     nrhi::Buffer* src, uint64_t src_offset, uint32_t row_pitch,
                                     uint32_t width, uint32_t height, uint32_t depth) {
  auto* t = static_cast<NrTextureD3D12*>(dst);
  D3D12_TEXTURE_COPY_LOCATION dst_loc{};
  dst_loc.pResource = t->resource;
  dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dst_loc.SubresourceIndex = mip + array_slice * t->desc.mip_levels;
  D3D12_TEXTURE_COPY_LOCATION src_loc{};
  src_loc.pResource = static_cast<NrBufferD3D12*>(src)->resource;
  src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  src_loc.PlacedFootprint.Offset = src_offset;
  src_loc.PlacedFootprint.Footprint.Format = t->dxgi_format;
  src_loc.PlacedFootprint.Footprint.Width = width;
  src_loc.PlacedFootprint.Footprint.Height = height;
  src_loc.PlacedFootprint.Footprint.Depth = depth;
  src_loc.PlacedFootprint.Footprint.RowPitch = row_pitch;
  device->cp()->GetDeferredCommandList().D3DCopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc,
                                                              nullptr);
}

void NrCmdD3D12::CopyTextureToBuffer(nrhi::Buffer* dst, uint64_t dst_offset, uint32_t row_pitch,
                                     nrhi::Texture* src, uint32_t mip, uint32_t width,
                                     uint32_t height) {
  auto* t = static_cast<NrTextureD3D12*>(src);
  D3D12_TEXTURE_COPY_LOCATION src_loc{};
  src_loc.pResource = t->resource;
  src_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  src_loc.SubresourceIndex = mip;
  D3D12_TEXTURE_COPY_LOCATION dst_loc{};
  dst_loc.pResource = static_cast<NrBufferD3D12*>(dst)->resource;
  dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  dst_loc.PlacedFootprint.Offset = dst_offset;
  dst_loc.PlacedFootprint.Footprint.Format = t->dxgi_format;
  dst_loc.PlacedFootprint.Footprint.Width = width;
  dst_loc.PlacedFootprint.Footprint.Height = height;
  dst_loc.PlacedFootprint.Footprint.Depth = 1;
  dst_loc.PlacedFootprint.Footprint.RowPitch = row_pitch;
  device->cp()->GetDeferredCommandList().D3DCopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc,
                                                              nullptr);
}

void NrCmdD3D12::Barrier(nrhi::Texture* texture, ResourceState before, ResourceState after) {
  auto* t = static_cast<NrTextureD3D12*>(texture);
  device->cp()->PushTransitionBarrier(t->resource,
                                      ToStates(before, device->guest_output_state()),
                                      ToStates(after, device->guest_output_state()));
}

void NrCmdD3D12::FlushBarriers() { device->cp()->SubmitBarriers(); }

}  // namespace

nrhi::Device* CreateNativeRhiDevice(D3D12CommandProcessor* command_processor) {
  return new NrDeviceD3D12(command_processor);
}

void DestroyNativeRhiDevice(nrhi::Device* device) {
  delete static_cast<NrDeviceD3D12*>(device);
}

nrhi::Cmd* NativeRhiBeginFrame(nrhi::Device* device, ID3D12Resource* guest_output_resource,
                               DXGI_FORMAT guest_output_format,
                               D3D12_RESOURCE_STATES guest_output_internal_state, uint32_t width,
                               uint32_t height, nrhi::Texture** guest_output_out) {
  return static_cast<NrDeviceD3D12*>(device)->BeginFrame(
      guest_output_resource, guest_output_format, guest_output_internal_state, width, height,
      guest_output_out);
}

}  // namespace rex::graphics::d3d12

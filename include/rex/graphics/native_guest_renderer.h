#pragma once

#include <cstdint>

#include <rex/graphics/native_rhi.h>

namespace rex::graphics {

enum class NativeGuestOutputBackend : uint32_t {
  kUnknown = 0,
  kD3D12,
  kVulkan,
};

// Per-frame context handed to the registered native guest-output renderer.
// Backend-agnostic: all device access and command recording goes through the
// native-render RHI (rex/graphics/native_rhi.h); the command processor owns
// the nrhi::Device and the per-frame nrhi::Cmd.
struct NativeGuestOutputRenderContext {
  NativeGuestOutputBackend backend = NativeGuestOutputBackend::kUnknown;
  uint32_t guest_output_width = 0;
  uint32_t guest_output_height = 0;
  uint32_t display_width = 0;
  uint32_t display_height = 0;

  // Device-level RHI: resource/pipeline creation, submission counters.
  // Stable across frames for the lifetime of the graphics system.
  nrhi::Device* device = nullptr;
  // Frame-scoped command recording into the command processor's deferred
  // command list. Only valid during the callback.
  nrhi::Cmd* cmd = nullptr;
  // The presenter's guest output image, wrapped as an RHI texture. In
  // nrhi::ResourceState::kGuestOutput at entry (or kCommon the very first
  // frame it exists); must be returned to kGuestOutput before the callback
  // returns true. The pointer is stable while the underlying image is (it
  // changes on output resize).
  nrhi::Texture* guest_output = nullptr;
};

using NativeGuestOutputRenderer = bool (*)(const NativeGuestOutputRenderContext& context,
                                           void* user_data);

void SetNativeGuestOutputRenderer(NativeGuestOutputRenderer renderer, void* user_data);
bool TryRenderNativeGuestOutput(const NativeGuestOutputRenderContext& context);
// Whether a renderer is registered at all; command processors skip RHI
// setup entirely when none is (non-Skate titles / renderer disabled).
bool HasNativeGuestOutputRenderer();

// True while the registered renderer actually replaced the last presented
// frame (false when it yields to the emulated output).
bool IsNativeGuestOutputActive();
// native_render_suppress_emulated_draws && IsNativeGuestOutputActive():
// command processors skip emulated draw/resolve execution (memexport draws,
// fences, queries and PM4 parsing still run).
bool ShouldSuppressEmulatedDraws();

// While the native guest-output renderer is active: should the emulated pass
// currently targeting `surface_pitch`-wide surfaces be suppressed? Driven by
// the native_render_suppress_mode cvar; shared by both command processors.
// Draw and resolve suppression must agree: executed passes need their
// resolves, suppressed passes leave garbage EDRAM that must never be copied
// out.
bool ShouldSuppressPassAtPitch(uint32_t surface_pitch);

// Within a suppression-EXEMPT pass: should a draw with no pixel shader
// (depth/stencil-only: shadow casters, z-prepasses) be skipped anyway?
// Driven by native_render_suppress_exempt_depth_only; their output feeds
// only suppressed scene passes (the native renderer shadows itself).
bool ShouldSuppressExemptDepthOnlyDraws();

}  // namespace rex::graphics

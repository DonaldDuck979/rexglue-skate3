#include <rex/graphics/native_guest_renderer.h>

#include <atomic>

namespace rex::graphics {
namespace {

std::atomic<NativeGuestOutputRenderer> g_renderer{nullptr};
std::atomic<void*> g_renderer_user_data{nullptr};

}  // namespace

void SetNativeGuestOutputRenderer(NativeGuestOutputRenderer renderer, void* user_data) {
  g_renderer_user_data.store(user_data, std::memory_order_release);
  g_renderer.store(renderer, std::memory_order_release);
}

bool TryRenderNativeGuestOutput(const NativeGuestOutputRenderContext& context) {
  NativeGuestOutputRenderer renderer = g_renderer.load(std::memory_order_acquire);
  if (renderer == nullptr) {
    return false;
  }
  void* user_data = g_renderer_user_data.load(std::memory_order_acquire);
  return renderer(context, user_data);
}

}  // namespace rex::graphics

#include <rex/graphics/native_guest_renderer.h>

#include <atomic>

#include <rex/cvar.h>

// Defined here (shared TU) rather than in a backend's command_processor.cpp
// so both D3D12 and Vulkan see the same definition.
REXCVAR_DEFINE_BOOL(native_render_suppress_emulated_draws, false, "GPU",
                    "While the registered native guest-output renderer is actively "
                    "replacing frames, skip emulated draw and resolve execution in the "
                    "command processor (PM4 parsing, fences, queries and memexport draws "
                    "still run). Menus/pause yield to the emulated path and execute "
                    "normally.")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

namespace rex::graphics {
namespace {

std::atomic<NativeGuestOutputRenderer> g_renderer{nullptr};
std::atomic<void*> g_renderer_user_data{nullptr};
// True while the registered renderer actually replaced the last presented
// frame (false when it yields: menus, early-outs, no renderer).
std::atomic<bool> g_native_output_active{false};

}  // namespace

void SetNativeGuestOutputRenderer(NativeGuestOutputRenderer renderer, void* user_data) {
  g_renderer_user_data.store(user_data, std::memory_order_release);
  g_renderer.store(renderer, std::memory_order_release);
}

bool TryRenderNativeGuestOutput(const NativeGuestOutputRenderContext& context) {
  NativeGuestOutputRenderer renderer = g_renderer.load(std::memory_order_acquire);
  if (renderer == nullptr) {
    g_native_output_active.store(false, std::memory_order_relaxed);
    return false;
  }
  void* user_data = g_renderer_user_data.load(std::memory_order_acquire);
  const bool rendered = renderer(context, user_data);
  g_native_output_active.store(rendered, std::memory_order_relaxed);
  return rendered;
}

bool IsNativeGuestOutputActive() {
  return g_native_output_active.load(std::memory_order_relaxed);
}

bool ShouldSuppressEmulatedDraws() {
  return REXCVAR_GET(native_render_suppress_emulated_draws) &&
         g_native_output_active.load(std::memory_order_relaxed);
}

}  // namespace rex::graphics

/**
 * @file        rex/ui/graphics_device_list.h
 *
 * @brief       Registry of the graphics devices the active backend enumerated.
 */
#pragma once

#include <string>
#include <vector>

namespace rex::ui {

// Devices the active graphics backend enumerated during initialization, in
// cvar index order: element i is selected by setting the backend's device
// cvar to i, and -1 selects automatically. Published by the provider so UI
// like the settings overlay can offer a device picker without linking against
// a specific backend.
struct GraphicsDeviceList {
  // Init-only int32 cvar that selects a device by index ("d3d12_adapter" or
  // "vulkan_device"); empty when no backend has published a list.
  std::string cvar_name;
  std::vector<std::string> device_names;
};

void SetGraphicsDeviceList(GraphicsDeviceList list);
GraphicsDeviceList GetGraphicsDeviceList();

}  // namespace rex::ui

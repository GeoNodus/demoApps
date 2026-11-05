#pragma once

// Vulkan 1.0 renderer: vulkan-1.dll is loaded at run time (VK_NO_PROTOTYPES), the Khronos headers are in
// third_party/vulkan, the shaders are prebuilt SPIR-V (shaders/shaders_spv.h). Neither the Vulkan SDK nor vulkan-1.lib
// is needed
#include "demo_app.h"

#include <memory>

namespace demo {

std::unique_ptr<IRenderer> createVulkanRenderer();

} // namespace demo

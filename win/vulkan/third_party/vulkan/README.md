Vulkan headers 1.3.236 from the Khronos Vulkan-Headers repository (https://github.com/KhronosGroup/Vulkan-Headers),
SPDX-License-Identifier: Apache-2.0. Only `vk_platform.h`, `vulkan_core.h` and `vulkan_win32.h` are needed: the demo
loads `vulkan-1.dll` at run time (`VK_NO_PROTOTYPES`), so neither the Vulkan SDK nor `vulkan-1.lib` is required.

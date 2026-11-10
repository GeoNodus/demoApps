// GeoNodus demo (Vulkan): shows a GeoNodus graph (.gng, gnSaveGraph) and plays its animation.
// Command line: geonodus_vulkan [graph.gng] [--play]
#include "vk_renderer.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    auto renderer = demo::createVulkanRenderer();
    return demo::runDemo(*renderer, L"GeoNodus Vulkan demo");
}

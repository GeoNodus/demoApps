#include "vk_renderer.h"

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_win32.h>

#include "shaders/shaders_spv.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace demo {

namespace {

// --- Vulkan functions: global, instance and device -------------------------------------------------------------
#define DEMO_VK_GLOBAL(X)                                                                                              \
    X(vkCreateInstance)                                                                                                \
    X(vkEnumerateInstanceExtensionProperties)

#define DEMO_VK_INSTANCE(X)                                                                                            \
    X(vkDestroyInstance)                                                                                               \
    X(vkEnumeratePhysicalDevices)                                                                                      \
    X(vkGetPhysicalDeviceProperties)                                                                                   \
    X(vkGetPhysicalDeviceFeatures)                                                                                     \
    X(vkGetPhysicalDeviceQueueFamilyProperties)                                                                        \
    X(vkGetPhysicalDeviceMemoryProperties)                                                                             \
    X(vkGetPhysicalDeviceFormatProperties)                                                                             \
    X(vkEnumerateDeviceExtensionProperties)                                                                            \
    X(vkCreateDevice)                                                                                                  \
    X(vkGetDeviceProcAddr)                                                                                             \
    X(vkCreateWin32SurfaceKHR)                                                                                         \
    X(vkDestroySurfaceKHR)                                                                                             \
    X(vkGetPhysicalDeviceSurfaceSupportKHR)                                                                            \
    X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)                                                                       \
    X(vkGetPhysicalDeviceSurfaceFormatsKHR)

#define DEMO_VK_DEVICE(X)                                                                                              \
    X(vkDestroyDevice)                                                                                                 \
    X(vkGetDeviceQueue)                                                                                                \
    X(vkDeviceWaitIdle)                                                                                                \
    X(vkCreateSwapchainKHR)                                                                                            \
    X(vkDestroySwapchainKHR)                                                                                           \
    X(vkGetSwapchainImagesKHR)                                                                                         \
    X(vkAcquireNextImageKHR)                                                                                           \
    X(vkQueuePresentKHR)                                                                                               \
    X(vkQueueSubmit)                                                                                                   \
    X(vkCreateImage)                                                                                                   \
    X(vkDestroyImage)                                                                                                  \
    X(vkCreateImageView)                                                                                               \
    X(vkDestroyImageView)                                                                                              \
    X(vkGetImageMemoryRequirements)                                                                                    \
    X(vkBindImageMemory)                                                                                               \
    X(vkCreateBuffer)                                                                                                  \
    X(vkDestroyBuffer)                                                                                                 \
    X(vkGetBufferMemoryRequirements)                                                                                   \
    X(vkBindBufferMemory)                                                                                              \
    X(vkAllocateMemory)                                                                                                \
    X(vkFreeMemory)                                                                                                    \
    X(vkMapMemory)                                                                                                     \
    X(vkUnmapMemory)                                                                                                   \
    X(vkCreateRenderPass)                                                                                              \
    X(vkDestroyRenderPass)                                                                                             \
    X(vkCreateFramebuffer)                                                                                             \
    X(vkDestroyFramebuffer)                                                                                            \
    X(vkCreateShaderModule)                                                                                            \
    X(vkDestroyShaderModule)                                                                                           \
    X(vkCreatePipelineLayout)                                                                                          \
    X(vkDestroyPipelineLayout)                                                                                         \
    X(vkCreateGraphicsPipelines)                                                                                       \
    X(vkDestroyPipeline)                                                                                               \
    X(vkCreateCommandPool)                                                                                             \
    X(vkDestroyCommandPool)                                                                                            \
    X(vkAllocateCommandBuffers)                                                                                        \
    X(vkResetCommandBuffer)                                                                                            \
    X(vkBeginCommandBuffer)                                                                                            \
    X(vkEndCommandBuffer)                                                                                              \
    X(vkCmdBeginRenderPass)                                                                                            \
    X(vkCmdEndRenderPass)                                                                                              \
    X(vkCmdBindPipeline)                                                                                               \
    X(vkCmdBindVertexBuffers)                                                                                          \
    X(vkCmdBindIndexBuffer)                                                                                            \
    X(vkCmdDraw)                                                                                                       \
    X(vkCmdDrawIndexed)                                                                                                \
    X(vkCmdPushConstants)                                                                                              \
    X(vkCmdSetViewport)                                                                                                \
    X(vkCmdSetScissor)                                                                                                 \
    X(vkCreateSemaphore)                                                                                               \
    X(vkDestroySemaphore)                                                                                              \
    X(vkCreateFence)                                                                                                   \
    X(vkDestroyFence)                                                                                                  \
    X(vkWaitForFences)                                                                                                 \
    X(vkResetFences)

struct VkFunctions
{
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
#define DEMO_VK_DECLARE(name) PFN_##name name = nullptr;
    DEMO_VK_GLOBAL(DEMO_VK_DECLARE)
    DEMO_VK_INSTANCE(DEMO_VK_DECLARE)
    DEMO_VK_DEVICE(DEMO_VK_DECLARE)
#undef DEMO_VK_DECLARE

    bool loadGlobal()
    {
        bool ok = true;
#define DEMO_VK_LOAD(name)                                                                                             \
    name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(nullptr, #name));                                        \
    ok = ok && name;
        DEMO_VK_GLOBAL(DEMO_VK_LOAD)
#undef DEMO_VK_LOAD
        return ok;
    }

    bool loadInstance(VkInstance instance)
    {
        bool ok = true;
#define DEMO_VK_LOAD(name)                                                                                             \
    name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(instance, #name));                                       \
    ok = ok && name;
        DEMO_VK_INSTANCE(DEMO_VK_LOAD)
#undef DEMO_VK_LOAD
        return ok;
    }

    bool loadDevice(VkDevice device)
    {
        bool ok = true;
#define DEMO_VK_LOAD(name)                                                                                             \
    name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name));                                          \
    ok = ok && name;
        DEMO_VK_DEVICE(DEMO_VK_LOAD)
#undef DEMO_VK_LOAD
        return ok;
    }
};

const int FRAMES_IN_FLIGHT = 2;

// vkCreateDevice under SEH: some drivers crash inside it instead of returning an error (Dozen — the Microsoft
// Vulkan-on-D3D12 driver — on the Parallels D3D12 adapter), then the next device is tried. A separate function without
// C++ objects, as __try requires
VkResult createDeviceGuarded(PFN_vkCreateDevice fn, VkPhysicalDevice physical, const VkDeviceCreateInfo* info,
    VkDevice* device, DWORD* exception)
{
    __try {
        return fn(physical, info, nullptr, device);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *exception = GetExceptionCode();
        *device = VK_NULL_HANDLE;
        return VK_ERROR_INITIALIZATION_FAILED;
    }
}
const float POINT_SIZE = 4.0f;

struct Image
{
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

struct Buffer
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void* mapped = nullptr;
    VkDeviceSize capacity = 0;
};

class VulkanRenderer : public IRenderer
{
public:
    bool init(HWND hwnd, std::string& error) override;
    void shutdown() override;
    void resize(int width, int height) override;
    void setGeometry(const SceneGeometry& geometry) override;
    void render(const ViewParams& view) override;
    std::wstring description() const override { return m_description; }

private:
    bool createInstance(std::string& error);
    // Picks a device and creates the logical device: candidates in order of preference, a failing one is skipped
    bool pickDevice(std::string& error);
    bool configureDevice(std::string& error);
    bool createDevice(std::string& error);
    bool createRenderPass(std::string& error);
    bool createPipelines(std::string& error);
    bool createSwapchain();
    void destroySwapchain();
    bool recreateSwapchain();
    bool createImage(uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect,
        Image& out);
    void destroyImage(Image& img);
    bool ensureBuffer(Buffer& buf, VkDeviceSize size, VkBufferUsageFlags usage);
    void destroyBuffer(Buffer& buf);
    int memoryType(uint32_t bits, VkMemoryPropertyFlags flags) const;
    VkShaderModule shaderModule(const uint32_t* code, size_t size);
    VkPipeline makePipeline(VkShaderModule vs, VkShaderModule fs, VkPrimitiveTopology topology, bool vertexInput,
        bool depth, bool depthBias);

    HWND m_hwnd = nullptr;
    HMODULE m_lib = nullptr;
    VkFunctions vk;
    std::wstring m_description = L"Vulkan";

    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties m_mem_props{};
    uint32_t m_queue_family = 0;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    VkSampleCountFlagBits m_samples = VK_SAMPLE_COUNT_1_BIT;
    VkFormat m_depth_format = VK_FORMAT_D32_SFLOAT;
    VkSurfaceFormatKHR m_surface_format{};
    float m_point_size = 1.0f;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkExtent2D m_extent{0, 0};
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_views;
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkSemaphore> m_render_done; // per swapchain image
    Image m_color; // MSAA target, resolved into the swapchain image
    Image m_depth;

    VkRenderPass m_render_pass = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_tri_pipeline = VK_NULL_HANDLE;
    VkPipeline m_line_pipeline = VK_NULL_HANDLE;
    VkPipeline m_point_pipeline = VK_NULL_HANDLE;
    VkPipeline m_background_pipeline = VK_NULL_HANDLE;

    VkCommandPool m_pool = VK_NULL_HANDLE;
    VkCommandBuffer m_cmd[FRAMES_IN_FLIGHT]{};
    VkSemaphore m_acquired[FRAMES_IN_FLIGHT]{};
    VkFence m_fences[FRAMES_IN_FLIGHT]{};
    int m_frame = 0;

    Buffer m_vertices;
    Buffer m_indices;
    uint32_t m_tri_count = 0, m_line_count = 0, m_point_count = 0; // indices
    std::vector<DrawBatch> m_batches;

    int m_width = 1, m_height = 1;
    bool m_need_recreate = false;
};

bool VulkanRenderer::createInstance(std::string& error)
{
    m_lib = LoadLibraryW(L"vulkan-1.dll");
    if (!m_lib) {
        error = "Vulkan loader (vulkan-1.dll) is not installed";
        return false;
    }
    vk.vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        GetProcAddress(m_lib, "vkGetInstanceProcAddr"));
    if (!vk.vkGetInstanceProcAddr || !vk.loadGlobal()) {
        error = "vulkan-1.dll has no vkGetInstanceProcAddr";
        return false;
    }

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "GeoNodus Vulkan demo";
    app.apiVersion = VK_API_VERSION_1_0;
    const char* extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = 2;
    ci.ppEnabledExtensionNames = extensions;
    const VkResult r = vk.vkCreateInstance(&ci, nullptr, &m_instance);
    if (r != VK_SUCCESS) {
        error = r == VK_ERROR_INCOMPATIBLE_DRIVER ? "No Vulkan driver is installed (VK_ERROR_INCOMPATIBLE_DRIVER)"
                                                  : "vkCreateInstance failed";
        return false;
    }
    if (!vk.loadInstance(m_instance)) {
        error = "Cannot load Vulkan instance functions";
        return false;
    }

    VkWin32SurfaceCreateInfoKHR si{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    si.hinstance = GetModuleHandleW(nullptr);
    si.hwnd = m_hwnd;
    if (vk.vkCreateWin32SurfaceKHR(m_instance, &si, nullptr, &m_surface) != VK_SUCCESS) {
        error = "vkCreateWin32SurfaceKHR failed";
        return false;
    }
    return true;
}

bool VulkanRenderer::pickDevice(std::string& error)
{
    uint32_t count = 0;
    vk.vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vk.vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    // Devices with graphics and present in one queue family and the swapchain extension; discrete GPUs first, then
    // integrated, then the rest (virtual, CPU)
    struct Candidate
    {
        VkPhysicalDevice device;
        uint32_t family;
        int score;
    };
    std::vector<Candidate> candidates;
    for (VkPhysicalDevice dev : devices) {
        uint32_t extCount = 0;
        vk.vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vk.vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, exts.data());
        const bool swapchain = std::any_of(exts.begin(), exts.end(), [](const VkExtensionProperties& e) {
            return std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
        });
        if (!swapchain)
            continue;

        uint32_t qCount = 0;
        vk.vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(qCount);
        vk.vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, families.data());
        for (uint32_t i = 0; i < qCount; ++i) {
            VkBool32 present = VK_FALSE;
            vk.vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, m_surface, &present);
            if (!(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) || !present)
                continue;
            VkPhysicalDeviceProperties props;
            vk.vkGetPhysicalDeviceProperties(dev, &props);
            const int score = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU     ? 3
                              : props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2
                                                                                           : 1;
            candidates.push_back({dev, i, score});
            break;
        }
    }
    if (candidates.empty()) {
        error = count ? "No Vulkan device can present to the window" : "No Vulkan devices found";
        return false;
    }
    std::stable_sort(candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) { return a.score > b.score; });

    std::string failures;
    for (const Candidate& c : candidates) {
        m_physical = c.device;
        m_queue_family = c.family;
        std::string reason;
        if (configureDevice(reason) && createDevice(reason))
            return true;
        VkPhysicalDeviceProperties props;
        vk.vkGetPhysicalDeviceProperties(c.device, &props);
        failures += std::string("\n") + props.deviceName + ": " + reason;
    }
    m_physical = VK_NULL_HANDLE;
    error = "No Vulkan device could be created:" + failures;
    return false;
}

bool VulkanRenderer::configureDevice(std::string& error)
{
    vk.vkGetPhysicalDeviceMemoryProperties(m_physical, &m_mem_props);
    VkPhysicalDeviceProperties props;
    vk.vkGetPhysicalDeviceProperties(m_physical, &props);

    // MSAA — up to 4x, as supported by both color and depth
    const VkSampleCountFlags counts = props.limits.framebufferColorSampleCounts
                                      & props.limits.framebufferDepthSampleCounts;
    m_samples = counts & VK_SAMPLE_COUNT_4_BIT   ? VK_SAMPLE_COUNT_4_BIT
                : counts & VK_SAMPLE_COUNT_2_BIT ? VK_SAMPLE_COUNT_2_BIT
                                                 : VK_SAMPLE_COUNT_1_BIT;

    for (VkFormat f : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM}) {
        VkFormatProperties fp;
        vk.vkGetPhysicalDeviceFormatProperties(m_physical, f, &fp);
        if (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            m_depth_format = f;
            break;
        }
    }

    // Frame format: 8-bit UNORM (as in the OpenGL version — colors are written as is, without sRGB conversion)
    uint32_t fmtCount = 0;
    vk.vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical, m_surface, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vk.vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical, m_surface, &fmtCount, formats.data());
    if (formats.empty()) {
        error = "The surface has no formats";
        return false;
    }
    m_surface_format = formats[0];
    if (fmtCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
        m_surface_format = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    for (const VkSurfaceFormatKHR& f : formats)
        if ((f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM)
            && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            m_surface_format = f;
            break;
        }

    wchar_t name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
    MultiByteToWideChar(CP_UTF8, 0, props.deviceName, -1, name, VK_MAX_PHYSICAL_DEVICE_NAME_SIZE);
    m_description = L"Vulkan " + std::to_wstring(VK_VERSION_MAJOR(props.apiVersion)) + L"."
                    + std::to_wstring(VK_VERSION_MINOR(props.apiVersion)) + L", " + name + L", MSAA "
                    + std::to_wstring(static_cast<int>(m_samples)) + L"x";
    return true;
}

bool VulkanRenderer::createDevice(std::string& error)
{
    VkPhysicalDeviceFeatures supported;
    vk.vkGetPhysicalDeviceFeatures(m_physical, &supported);
    VkPhysicalDeviceFeatures enabled{};
    // Points larger than a pixel — only with largePoints
    enabled.largePoints = supported.largePoints;
    if (supported.largePoints) {
        VkPhysicalDeviceProperties props;
        vk.vkGetPhysicalDeviceProperties(m_physical, &props);
        m_point_size = (std::min) (POINT_SIZE, props.limits.pointSizeRange[1]);
    }

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qi.queueFamilyIndex = m_queue_family;
    qi.queueCount = 1;
    qi.pQueuePriorities = &priority;
    const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    ci.queueCreateInfoCount = 1;
    ci.pQueueCreateInfos = &qi;
    ci.enabledExtensionCount = 1;
    ci.ppEnabledExtensionNames = extensions;
    ci.pEnabledFeatures = &enabled;
    DWORD exception = 0;
    const VkResult r = createDeviceGuarded(vk.vkCreateDevice, m_physical, &ci, &m_device, &exception);
    if (r != VK_SUCCESS || !vk.loadDevice(m_device)) {
        char text[96];
        if (exception)
            snprintf(text, sizeof(text), "the driver crashed in vkCreateDevice (exception 0x%08lX)", exception);
        else
            snprintf(text, sizeof(text), "vkCreateDevice failed (%d)", static_cast<int>(r));
        error = text;
        m_device = VK_NULL_HANDLE;
        return false;
    }
    vk.vkGetDeviceQueue(m_device, m_queue_family, 0, &m_queue);

    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pi.queueFamilyIndex = m_queue_family;
    vk.vkCreateCommandPool(m_device, &pi, nullptr, &m_pool);
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = m_pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = FRAMES_IN_FLIGHT;
    vk.vkAllocateCommandBuffers(m_device, &ai, m_cmd);
    for (int i = 0; i < FRAMES_IN_FLIGHT; ++i) {
        VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        vk.vkCreateSemaphore(m_device, &sci, nullptr, &m_acquired[i]);
        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vk.vkCreateFence(m_device, &fci, nullptr, &m_fences[i]);
    }
    return true;
}

bool VulkanRenderer::createRenderPass(std::string& error)
{
    const bool msaa = m_samples != VK_SAMPLE_COUNT_1_BIT;
    VkAttachmentDescription att[3]{};
    // 0 — color: the MSAA target or the swapchain image itself
    att[0].format = m_surface_format.format;
    att[0].samples = m_samples;
    att[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att[0].storeOp = msaa ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
    att[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att[0].finalLayout = msaa ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    // 1 — depth
    att[1].format = m_depth_format;
    att[1].samples = m_samples;
    att[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    // 2 — the swapchain image MSAA resolves into
    att[2].format = m_surface_format.format;
    att[2].samples = VK_SAMPLE_COUNT_1_BIT;
    att[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att[2].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkAttachmentReference resolveRef{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &colorRef;
    sub.pResolveAttachments = msaa ? &resolveRef : nullptr;
    sub.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    ci.attachmentCount = msaa ? 3 : 2;
    ci.pAttachments = att;
    ci.subpassCount = 1;
    ci.pSubpasses = &sub;
    ci.dependencyCount = 1;
    ci.pDependencies = &dep;
    if (vk.vkCreateRenderPass(m_device, &ci, nullptr, &m_render_pass) != VK_SUCCESS) {
        error = "vkCreateRenderPass failed";
        return false;
    }
    return true;
}

VkShaderModule VulkanRenderer::shaderModule(const uint32_t* code, size_t size)
{
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = size;
    ci.pCode = code;
    VkShaderModule m = VK_NULL_HANDLE;
    vk.vkCreateShaderModule(m_device, &ci, nullptr, &m);
    return m;
}

VkPipeline VulkanRenderer::makePipeline(VkShaderModule vs, VkShaderModule fs, VkPrimitiveTopology topology,
    bool vertexInput, bool depth, bool depthBias)
{
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[3] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, p))},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, n))},
        {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, c))},
    };
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    if (vertexInput) {
        vi.vertexBindingDescriptionCount = 1;
        vi.pVertexBindingDescriptions = &binding;
        vi.vertexAttributeDescriptionCount = 3;
        vi.pVertexAttributeDescriptions = attrs;
    }
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = topology;
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE; // two-sided polygons, as in the Houdini viewport
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rs.lineWidth = 1.0f;
    // The fill is pushed back so that lines and points on the surface do not flicker (glPolygonOffset in the OpenGL
    // version)
    rs.depthBiasEnable = depthBias ? VK_TRUE : VK_FALSE;
    rs.depthBiasConstantFactor = 1.0f;
    rs.depthBiasSlopeFactor = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = m_samples;
    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = depth ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = depth ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
                           | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &blend;
    const VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dy.dynamicStateCount = 2;
    dy.pDynamicStates = dyn;

    VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    ci.stageCount = 2;
    ci.pStages = stages;
    ci.pVertexInputState = &vi;
    ci.pInputAssemblyState = &ia;
    ci.pViewportState = &vp;
    ci.pRasterizationState = &rs;
    ci.pMultisampleState = &ms;
    ci.pDepthStencilState = &ds;
    ci.pColorBlendState = &cb;
    ci.pDynamicState = &dy;
    ci.layout = m_layout;
    ci.renderPass = m_render_pass;
    VkPipeline p = VK_NULL_HANDLE;
    vk.vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &ci, nullptr, &p);
    return p;
}

bool VulkanRenderer::createPipelines(std::string& error)
{
    VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ShaderConstants)};
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    li.pushConstantRangeCount = 1;
    li.pPushConstantRanges = &range;
    vk.vkCreatePipelineLayout(m_device, &li, nullptr, &m_layout);

    VkShaderModule meshVs = shaderModule(mesh_vert, sizeof(mesh_vert));
    VkShaderModule meshFs = shaderModule(mesh_frag, sizeof(mesh_frag));
    VkShaderModule bgVs = shaderModule(background_vert, sizeof(background_vert));
    VkShaderModule bgFs = shaderModule(background_frag, sizeof(background_frag));
    m_tri_pipeline = makePipeline(meshVs, meshFs, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, true, true, true);
    m_line_pipeline = makePipeline(meshVs, meshFs, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, true, true, false);
    m_point_pipeline = makePipeline(meshVs, meshFs, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, true, true, false);
    m_background_pipeline = makePipeline(bgVs, bgFs, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, false, false, false);
    for (VkShaderModule m : {meshVs, meshFs, bgVs, bgFs})
        vk.vkDestroyShaderModule(m_device, m, nullptr);
    if (!m_tri_pipeline || !m_line_pipeline || !m_point_pipeline || !m_background_pipeline) {
        error = "vkCreateGraphicsPipelines failed";
        return false;
    }
    return true;
}

int VulkanRenderer::memoryType(uint32_t bits, VkMemoryPropertyFlags flags) const
{
    for (uint32_t i = 0; i < m_mem_props.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (m_mem_props.memoryTypes[i].propertyFlags & flags) == flags)
            return static_cast<int>(i);
    return -1;
}

bool VulkanRenderer::createImage(uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags usage,
    VkImageAspectFlags aspect, Image& out)
{
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = format;
    ci.extent = {w, h, 1};
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = m_samples;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = usage;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vk.vkCreateImage(m_device, &ci, nullptr, &out.image) != VK_SUCCESS)
        return false;
    VkMemoryRequirements req;
    vk.vkGetImageMemoryRequirements(m_device, out.image, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    const int type = memoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    ai.memoryTypeIndex = static_cast<uint32_t>(type >= 0 ? type : memoryType(req.memoryTypeBits, 0));
    if (vk.vkAllocateMemory(m_device, &ai, nullptr, &out.memory) != VK_SUCCESS)
        return false;
    vk.vkBindImageMemory(m_device, out.image, out.memory, 0);
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = out.image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = format;
    vi.subresourceRange = {aspect, 0, 1, 0, 1};
    return vk.vkCreateImageView(m_device, &vi, nullptr, &out.view) == VK_SUCCESS;
}

void VulkanRenderer::destroyImage(Image& img)
{
    if (img.view)
        vk.vkDestroyImageView(m_device, img.view, nullptr);
    if (img.image)
        vk.vkDestroyImage(m_device, img.image, nullptr);
    if (img.memory)
        vk.vkFreeMemory(m_device, img.memory, nullptr);
    img = {};
}

bool VulkanRenderer::createSwapchain()
{
    VkSurfaceCapabilitiesKHR caps;
    vk.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical, m_surface, &caps);
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == 0xFFFFFFFFu) {
        extent.width = std::clamp(static_cast<uint32_t>(m_width), caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(m_height), caps.minImageExtent.height,
            caps.maxImageExtent.height);
    }
    if (extent.width == 0 || extent.height == 0)
        return false; // the window is minimized

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0)
        imageCount = (std::min) (imageCount, caps.maxImageCount);
    VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    for (VkCompositeAlphaFlagBitsKHR a : {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
             VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR})
        if (caps.supportedCompositeAlpha & a) {
            alpha = a;
            break;
        }

    VkSwapchainKHR old = m_swapchain;
    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface = m_surface;
    ci.minImageCount = imageCount;
    ci.imageFormat = m_surface_format.format;
    ci.imageColorSpace = m_surface_format.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = alpha;
    ci.presentMode = VK_PRESENT_MODE_FIFO_KHR; // vsync, always available
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = old;
    const VkResult r = vk.vkCreateSwapchainKHR(m_device, &ci, nullptr, &m_swapchain);
    if (old)
        vk.vkDestroySwapchainKHR(m_device, old, nullptr);
    if (r != VK_SUCCESS) {
        m_swapchain = VK_NULL_HANDLE;
        return false;
    }
    m_extent = extent;

    uint32_t count = 0;
    vk.vkGetSwapchainImagesKHR(m_device, m_swapchain, &count, nullptr);
    m_images.resize(count);
    vk.vkGetSwapchainImagesKHR(m_device, m_swapchain, &count, m_images.data());

    const bool msaa = m_samples != VK_SAMPLE_COUNT_1_BIT;
    if (msaa
        && !createImage(extent.width, extent.height, m_surface_format.format,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
            m_color))
        return false;
    if (!createImage(extent.width, extent.height, m_depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT, m_depth))
        return false;

    for (VkImage img : m_images) {
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = img;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = m_surface_format.format;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageView view = VK_NULL_HANDLE;
        vk.vkCreateImageView(m_device, &vi, nullptr, &view);
        m_views.push_back(view);

        const VkImageView attachments[3] = {msaa ? m_color.view : view, m_depth.view, view};
        VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fi.renderPass = m_render_pass;
        fi.attachmentCount = msaa ? 3 : 2;
        fi.pAttachments = attachments;
        fi.width = extent.width;
        fi.height = extent.height;
        fi.layers = 1;
        VkFramebuffer fb = VK_NULL_HANDLE;
        vk.vkCreateFramebuffer(m_device, &fi, nullptr, &fb);
        m_framebuffers.push_back(fb);

        VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkSemaphore s = VK_NULL_HANDLE;
        vk.vkCreateSemaphore(m_device, &sci, nullptr, &s);
        m_render_done.push_back(s);
    }
    return true;
}

void VulkanRenderer::destroySwapchain()
{
    for (VkFramebuffer fb : m_framebuffers)
        vk.vkDestroyFramebuffer(m_device, fb, nullptr);
    for (VkImageView v : m_views)
        vk.vkDestroyImageView(m_device, v, nullptr);
    for (VkSemaphore s : m_render_done)
        vk.vkDestroySemaphore(m_device, s, nullptr);
    m_framebuffers.clear();
    m_views.clear();
    m_render_done.clear();
    m_images.clear();
    destroyImage(m_color);
    destroyImage(m_depth);
    // The swapchain itself stays: it is passed to the new one as oldSwapchain
}

bool VulkanRenderer::recreateSwapchain()
{
    vk.vkDeviceWaitIdle(m_device);
    destroySwapchain();
    m_need_recreate = false;
    return createSwapchain();
}

bool VulkanRenderer::init(HWND hwnd, std::string& error)
{
    m_hwnd = hwnd;
    RECT rc;
    GetClientRect(hwnd, &rc);
    m_width = (std::max) (1L, rc.right);
    m_height = (std::max) (1L, rc.bottom);
    if (!createInstance(error) || !pickDevice(error) || !createRenderPass(error) || !createPipelines(error))
        return false;
    createSwapchain(); // the window may still have zero size — then on the first frame
    return true;
}

void VulkanRenderer::shutdown()
{
    if (m_device) {
        vk.vkDeviceWaitIdle(m_device);
        destroySwapchain();
        if (m_swapchain)
            vk.vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        destroyBuffer(m_vertices);
        destroyBuffer(m_indices);
        for (VkPipeline p : {m_tri_pipeline, m_line_pipeline, m_point_pipeline, m_background_pipeline})
            if (p)
                vk.vkDestroyPipeline(m_device, p, nullptr);
        if (m_layout)
            vk.vkDestroyPipelineLayout(m_device, m_layout, nullptr);
        if (m_render_pass)
            vk.vkDestroyRenderPass(m_device, m_render_pass, nullptr);
        for (int i = 0; i < FRAMES_IN_FLIGHT; ++i) {
            if (m_acquired[i])
                vk.vkDestroySemaphore(m_device, m_acquired[i], nullptr);
            if (m_fences[i])
                vk.vkDestroyFence(m_device, m_fences[i], nullptr);
        }
        if (m_pool)
            vk.vkDestroyCommandPool(m_device, m_pool, nullptr);
        vk.vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    if (m_instance) {
        if (m_surface)
            vk.vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        vk.vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
    if (m_lib)
        FreeLibrary(m_lib);
    m_lib = nullptr;
}

void VulkanRenderer::resize(int width, int height)
{
    m_width = (std::max) (1, width);
    m_height = (std::max) (1, height);
    m_need_recreate = true;
}

bool VulkanRenderer::ensureBuffer(Buffer& buf, VkDeviceSize size, VkBufferUsageFlags usage)
{
    if (size <= buf.capacity && buf.buffer)
        return true;
    destroyBuffer(buf);
    // Headroom, so that the buffer is not recreated every time the geometry changes from frame to frame
    const VkDeviceSize capacity = (std::max<VkDeviceSize>) (size + size / 4, 64 * 1024);
    VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ci.size = capacity;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vk.vkCreateBuffer(m_device, &ci, nullptr, &buf.buffer) != VK_SUCCESS)
        return false;
    VkMemoryRequirements req;
    vk.vkGetBufferMemoryRequirements(m_device, buf.buffer, &req);
    const int type = memoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = static_cast<uint32_t>(type);
    if (type < 0 || vk.vkAllocateMemory(m_device, &ai, nullptr, &buf.memory) != VK_SUCCESS) {
        destroyBuffer(buf);
        return false;
    }
    vk.vkBindBufferMemory(m_device, buf.buffer, buf.memory, 0);
    vk.vkMapMemory(m_device, buf.memory, 0, VK_WHOLE_SIZE, 0, &buf.mapped);
    buf.capacity = capacity;
    return true;
}

void VulkanRenderer::destroyBuffer(Buffer& buf)
{
    if (buf.mapped)
        vk.vkUnmapMemory(m_device, buf.memory);
    if (buf.buffer)
        vk.vkDestroyBuffer(m_device, buf.buffer, nullptr);
    if (buf.memory)
        vk.vkFreeMemory(m_device, buf.memory, nullptr);
    buf = {};
}

void VulkanRenderer::setGeometry(const SceneGeometry& g)
{
    if (!m_device)
        return;
    // The GPU reads the buffers in frames in flight — wait for them first
    vk.vkDeviceWaitIdle(m_device);
    m_tri_count = m_line_count = m_point_count = 0;
    m_batches.clear();
    const VkDeviceSize vbytes = g.vertices.size() * sizeof(Vertex);
    const VkDeviceSize ibytes = (g.triangles.size() + g.lines.size() + g.points.size()) * sizeof(uint32_t);
    if (!vbytes || !ibytes || !ensureBuffer(m_vertices, vbytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
        || !ensureBuffer(m_indices, ibytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT))
        return;
    std::memcpy(m_vertices.mapped, g.vertices.data(), vbytes);
    // Indices one after another: triangles, lines, points
    auto* dst = static_cast<uint32_t*>(m_indices.mapped);
    dst = std::copy(g.triangles.begin(), g.triangles.end(), dst);
    dst = std::copy(g.lines.begin(), g.lines.end(), dst);
    std::copy(g.points.begin(), g.points.end(), dst);
    m_tri_count = static_cast<uint32_t>(g.triangles.size());
    m_line_count = static_cast<uint32_t>(g.lines.size());
    m_point_count = static_cast<uint32_t>(g.points.size());
    m_batches = g.batches;
}

void VulkanRenderer::render(const ViewParams& view)
{
    if (!m_device)
        return;
    if ((m_need_recreate || !m_swapchain || m_framebuffers.empty()) && !recreateSwapchain())
        return; // minimized

    vk.vkWaitForFences(m_device, 1, &m_fences[m_frame], VK_TRUE, UINT64_MAX);
    uint32_t imageIndex = 0;
    VkResult r = vk.vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_acquired[m_frame], VK_NULL_HANDLE,
        &imageIndex);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR)
        return;
    vk.vkResetFences(m_device, 1, &m_fences[m_frame]);

    VkCommandBuffer cmd = m_cmd[m_frame];
    vk.vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk.vkBeginCommandBuffer(cmd, &bi);

    VkClearValue clear[3]{};
    clear[1].depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp.renderPass = m_render_pass;
    rp.framebuffer = m_framebuffers[imageIndex];
    rp.renderArea = {{0, 0}, m_extent};
    rp.clearValueCount = m_samples != VK_SAMPLE_COUNT_1_BIT ? 3 : 2;
    rp.pClearValues = clear;
    vk.vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{0, 0, float(m_extent.width), float(m_extent.height), 0.0f, 1.0f};
    VkRect2D scissor{{0, 0}, m_extent};
    vk.vkCmdSetViewport(cmd, 0, 1, &viewport);
    vk.vkCmdSetScissor(cmd, 0, 1, &scissor);

    vk.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_background_pipeline);
    vk.vkCmdDraw(cmd, 3, 1, 0, 0);

    if (m_tri_count + m_line_count + m_point_count > 0) {
        const Mat4 proj = perspective(view.fovy, float(m_extent.width) / float(m_extent.height), view.zNear, view.zFar,
            true);
        const VkShaderStageFlags stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        const VkDeviceSize offset = 0;
        vk.vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertices.buffer, &offset);
        vk.vkCmdBindIndexBuffer(cmd, m_indices.buffer, 0, VK_INDEX_TYPE_UINT32);
        // Per object: its own matrix (push constants); indices in the buffer — triangles, lines, points
        for (const DrawBatch& b : m_batches) {
            ShaderConstants c = shaderConstants(proj, view.view, b.model, m_point_size);
            if (b.triangleCount) {
                c.params[1] = 1.0f;
                vk.vkCmdPushConstants(cmd, m_layout, stages, 0, sizeof(c), &c);
                vk.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_tri_pipeline);
                vk.vkCmdDrawIndexed(cmd, b.triangleCount, 1, b.firstTriangle, 0, 0);
            }
            if (!b.lineCount && !b.pointCount)
                continue;
            c.params[1] = 0.0f;
            vk.vkCmdPushConstants(cmd, m_layout, stages, 0, sizeof(c), &c);
            if (b.lineCount) {
                vk.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_line_pipeline);
                vk.vkCmdDrawIndexed(cmd, b.lineCount, 1, m_tri_count + b.firstLine, 0, 0);
            }
            if (b.pointCount) {
                vk.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_point_pipeline);
                vk.vkCmdDrawIndexed(cmd, b.pointCount, 1, m_tri_count + m_line_count + b.firstPoint, 0, 0);
            }
        }
    }

    vk.vkCmdEndRenderPass(cmd);
    vk.vkEndCommandBuffer(cmd);

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &m_acquired[m_frame];
    si.pWaitDstStageMask = &waitStage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &m_render_done[imageIndex];
    vk.vkQueueSubmit(m_queue, 1, &si, m_fences[m_frame]);

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &m_render_done[imageIndex];
    pi.swapchainCount = 1;
    pi.pSwapchains = &m_swapchain;
    pi.pImageIndices = &imageIndex;
    r = vk.vkQueuePresentKHR(m_queue, &pi);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
        m_need_recreate = true;
    m_frame = (m_frame + 1) % FRAMES_IN_FLIGHT;
}

} // namespace

std::unique_ptr<IRenderer> createVulkanRenderer()
{
    return std::make_unique<VulkanRenderer>();
}

} // namespace demo

#include "app.hpp"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#include <spdlog/spdlog.h>
#include <optional>
#include <set>
#include <algorithm>
#include <ranges>

const std::vector<const char*> App::s_validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
const std::vector<const char*> App::s_deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

namespace {
    bool CheckValidationLayerSupport()
    {
        uint32_t layerCount;

        if (vk::enumerateInstanceLayerProperties(&layerCount, nullptr) == vk::Result::eSuccess)
        {
            std::vector<vk::LayerProperties> availableLayers(layerCount);
            if (vk::enumerateInstanceLayerProperties(&layerCount, availableLayers.data()) == vk::Result::eSuccess)
            {
                for (const char* layerName : App::s_validationLayers) {
                    bool layerFound = false;

                    for (const auto& layerProperties : availableLayers) {
                        if (strcmp(layerName, layerProperties.layerName) == 0) {
                            layerFound = true;
                            break;
                        }
                    }

                    if (!layerFound) {
                        return false;
                    }
                }

                return true;
            }
        }

        return false;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData)
    {
        switch(messageSeverity)
        {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            spdlog::warn(pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            spdlog::info(pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            spdlog::error(pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            spdlog::debug(pCallbackData->pMessage);
            break;
        default:
            spdlog::critical(pCallbackData->pMessage);
        }

        return VK_FALSE;
    }

    void window_error_callback(int code, const char* description)
    {
        spdlog::error("GLFW: {}", description);
    }

    vk::DebugUtilsMessengerCreateInfoEXT MessengerCreateInfo()
    {
        vk::DebugUtilsMessengerCreateInfoEXT result;
        result.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose);
        result.setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                              vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                              vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
        result.setPfnUserCallback(debugCallback);
        return result;
    }

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool IsComplete()
        {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    QueueFamilyIndices FindQueueFamilies(const vk::PhysicalDevice& device,
                                         const vk::SurfaceKHR& surface)
    {
        QueueFamilyIndices indices;

        auto queueFamilies = device.getQueueFamilyProperties();

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
                indices.graphicsFamily = i;
            }

            vk::Bool32 presentSupport = device.getSurfaceSupportKHR(i, surface);
            if (presentSupport)
            {
                indices.presentFamily = i;
            }

            if (indices.IsComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    SwapChainSupportDetails QuerySwapChainSupport(const vk::PhysicalDevice& device,
                                                  const vk::SurfaceKHR& surface) {
        SwapChainSupportDetails result;

        result.capabilities = device.getSurfaceCapabilitiesKHR(surface);
        result.formats = device.getSurfaceFormatsKHR(surface);
        result.presentModes = device.getSurfacePresentModesKHR(surface);

        return result;
    }

}

void App::InitWindow()
{
    if (!glfwInit())
    {
        spdlog::error("GLFW initialization failed");
        throw std::runtime_error("GLFW init failed");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    glfwSetErrorCallback(window_error_callback);

    m_window = glfwCreateWindow(m_width, m_height, "Vulkan", nullptr, nullptr);

}

std::vector<const char*> App::GetRequiredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> result(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (m_validationLayers)
    {
        result.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return result;
}

void App::CreateInstance()
{
    if (m_validationLayers && !CheckValidationLayerSupport())
    {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    vk::ApplicationInfo appInfo("Viewer", 1, "No Engine", 1, VK_API_VERSION_1_2);
    vk::InstanceCreateInfo createInfo;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = GetRequiredExtensions();
    createInfo.enabledExtensionCount = extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();

    vk::DebugUtilsMessengerCreateInfoEXT messengerInfo;
    if (m_validationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(s_validationLayers.size());
        createInfo.ppEnabledLayerNames = s_validationLayers.data();

        messengerInfo = MessengerCreateInfo();
        createInfo.pNext = &messengerInfo;
    } else {
        createInfo.enabledLayerCount = 0;
    }

    m_instance = vk::createInstance(createInfo);

    m_dispatcher = vk::DispatchLoaderDynamic(m_instance, vkGetInstanceProcAddr);
    spdlog::info("Create vulkan instance successful");
}

void App::SetupDebugMessenger()
{
    if (!m_validationLayers) return;

    auto createInfo = MessengerCreateInfo();

    m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(createInfo, nullptr, m_dispatcher);
}

bool CheckDeviceExtensionSupport(const vk::PhysicalDevice& device) {

    auto availableExtensions = device.enumerateDeviceExtensionProperties();

    std::set<std::string> requiredExtensions(App::s_deviceExtensions.begin(),
                                             App::s_deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

bool App::IsDeviceSuitable(const vk::PhysicalDevice& device)
{
    QueueFamilyIndices indices = FindQueueFamilies(device, m_surface);

    bool extensionsSupported = CheckDeviceExtensionSupport(device);

    bool swapChainOk = false;
    if (extensionsSupported)
    {
        auto details = QuerySwapChainSupport(device, m_surface);
        swapChainOk = !details.formats.empty() && !details.presentModes.empty();
    }

    return indices.IsComplete() && extensionsSupported && swapChainOk;
}

void App::PickPhysicalDevice()
{
    auto devices = m_instance.enumeratePhysicalDevices(m_dispatcher);

    if (devices.size() == 0)
    {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    bool foundSuitable = false;
    for (const auto& device : devices)
    {
        if (IsDeviceSuitable(device))
        {
            m_physicalDevice = device;
            foundSuitable = true;
            break;
        }
    }

    if (!foundSuitable)
    {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
    spdlog::info("Physical device picked");
}

void App::CreateLogicalDevice()
{
    auto indices = FindQueueFamilies(m_physicalDevice, m_surface);

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies
        {indices.graphicsFamily.value(),
         indices.presentFamily.value()};

    for (auto queueFamily : uniqueQueueFamilies)
    {
        vk::DeviceQueueCreateInfo queueCreateInfo;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
        float priority = 1.f;
        queueCreateInfo.pQueuePriorities = &priority;

        queueCreateInfos.push_back(queueCreateInfo);
    }

    vk::PhysicalDeviceFeatures deviceFeatures;

    vk::DeviceCreateInfo createInfo;
    createInfo.queueCreateInfoCount = queueCreateInfos.size();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = s_deviceExtensions.size();
    createInfo.ppEnabledExtensionNames = s_deviceExtensions.data();


    m_device = m_physicalDevice.createDevice(createInfo);

    m_graphicsQueue = m_device.getQueue(indices.graphicsFamily.value(), 0);
    m_presentQueue = m_device.getQueue(indices.presentFamily.value(), 0);
}

void App::CreateSurface()
{
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }

    m_surface = vk::SurfaceKHR(surface);
}

vk::SurfaceFormatKHR App::ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats)
{
    auto it = std::ranges::find_if(formats, [] (const vk::SurfaceFormatKHR& format)
    {
        return format.format == vk::Format::eB8G8R8A8Srgb
            && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });

    return it != formats.end() ? *it : formats[0];
}

vk::Extent2D App::ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window)
{
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        vk::Extent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

vk::PresentModeKHR App::ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& modes)
{
    auto it = std::ranges::find_if(modes, [] (const vk::PresentModeKHR& mode)
    {
        return mode == vk::PresentModeKHR::eImmediate;
    });

    return it != modes.end() ? *it : vk::PresentModeKHR::eFifo;
}

void App::CreateSwapChain()
{
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(m_physicalDevice, m_surface);

    auto surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
    auto presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
    auto extent = ChooseSwapExtent(swapChainSupport.capabilities, m_window);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    QueueFamilyIndices indices = FindQueueFamilies(m_physicalDevice, m_surface);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    m_swapChain = m_device.createSwapchainKHR(createInfo);

    m_swapChainImages = m_device.getSwapchainImagesKHR(m_swapChain);
    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent = extent;
}

void App::CreateImageViews()
{
    m_swapChainImageViews.resize(m_swapChainImages.size());

    for (size_t i = 0; i < m_swapChainImages.size(); i++)
    {
        vk::ImageViewCreateInfo createInfo;
        createInfo.image = m_swapChainImages[i];
        createInfo.viewType = vk::ImageViewType::e2D;
        createInfo.format = m_swapChainImageFormat;

        createInfo.components = vk::ComponentMapping();

        createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        m_swapChainImageViews[i] = m_device.createImageView(createInfo);
    }
}

void App::CreateGraphicsPipeline()
{

}

void App::InitVulkan()
{
    spdlog::info("Initializing Vulkan");
    CreateInstance();
    SetupDebugMessenger();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapChain();
    CreateImageViews();
    CreateGraphicsPipeline();
}

void App::Loop()
{
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
    }
}

void App::Terminate()
{
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

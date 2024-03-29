#include <engine.hpp>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#include <spdlog/spdlog.h>
#include <optional>
#include <set>
#include <algorithm>
#include <ranges>
#include "shader_compiler.hpp"
#include "files.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <chrono>
#include <imgui.h>
#include "bindings/imgui_impl_glfw.h"
#include "bindings/imgui_impl_vulkan.h"
#include <Tracy.hpp>
#include <TracyVulkan.hpp>
#include "initializers.hpp"

const std::vector<const char*> Engine::s_validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
const std::vector<const char*> Engine::s_deviceExtensions = {
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
                for (const char* layerName : Engine::s_validationLayers) {
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

std::vector<const char*> Engine::GetRequiredExtensions()
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

void Engine::CreateInstance()
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

    m_instance = vk::createInstanceUnique(createInfo);

    m_dispatcher = vk::DispatchLoaderDynamic(*m_instance, vkGetInstanceProcAddr);
    spdlog::info("Create vulkan instance successful");
}

void Engine::SetupDebugMessenger()
{
    if (!m_validationLayers) return;

    auto createInfo = MessengerCreateInfo();

    m_debugMessenger = m_instance->createDebugUtilsMessengerEXTUnique(
        createInfo, nullptr, m_dispatcher);
}

bool CheckDeviceExtensionSupport(const vk::PhysicalDevice& device) {

    auto availableExtensions = device.enumerateDeviceExtensionProperties();

    std::set<std::string> requiredExtensions(Engine::s_deviceExtensions.begin(),
                                             Engine::s_deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

bool Engine::IsDeviceSuitable(const vk::PhysicalDevice& device)
{
    QueueFamilyIndices indices = FindQueueFamilies(device, *m_surface);

    bool extensionsSupported = CheckDeviceExtensionSupport(device);

    bool swapChainOk = false;
    if (extensionsSupported)
    {
        auto details = QuerySwapChainSupport(device, *m_surface);
        swapChainOk = !details.formats.empty() && !details.presentModes.empty();
    }

    return indices.IsComplete() && extensionsSupported && swapChainOk;
}

void Engine::PickPhysicalDevice()
{
    auto devices = m_instance->enumeratePhysicalDevices(m_dispatcher);

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

    auto props = m_physicalDevice.getProperties();
    spdlog::info("Physical device picked: {}", props.deviceName);
}

void Engine::CreateLogicalDevice()
{
    auto indices = FindQueueFamilies(m_physicalDevice, *m_surface);

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
    deviceFeatures.fillModeNonSolid = VK_TRUE;

    vk::DeviceCreateInfo createInfo;
    createInfo.queueCreateInfoCount = queueCreateInfos.size();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = s_deviceExtensions.size();
    createInfo.ppEnabledExtensionNames = s_deviceExtensions.data();


    m_device = m_physicalDevice.createDeviceUnique(createInfo);

    m_graphicsQueue = m_device->getQueue(indices.graphicsFamily.value(), 0);
    m_presentQueue = m_device->getQueue(indices.presentFamily.value(), 0);
}

void Engine::CreateSurface()
{
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(*m_instance, m_window, nullptr, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }

    m_surface = vk::UniqueSurfaceKHR(surface, *m_instance);
}

vk::SurfaceFormatKHR Engine::ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats)
{
    auto it = std::ranges::find_if(formats, [] (const vk::SurfaceFormatKHR& format)
    {
        return format.format == vk::Format::eB8G8R8A8Srgb
            && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });

    return it != formats.end() ? *it : formats[0];
}

vk::Extent2D Engine::ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window)
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

vk::PresentModeKHR Engine::ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& modes)
{
    auto it = std::ranges::find_if(modes, [] (const vk::PresentModeKHR& mode)
    {
        return mode == vk::PresentModeKHR::eImmediate;
    });

    return it != modes.end() ? *it : vk::PresentModeKHR::eFifo;
}

void Engine::CreateSwapChain()
{
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(m_physicalDevice, *m_surface);

    auto surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
    m_swapChainFormat = surfaceFormat.format;
    auto presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
    auto extent = ChooseSwapExtent(swapChainSupport.capabilities, m_window);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo;
    createInfo.surface = *m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    QueueFamilyIndices indices = FindQueueFamilies(m_physicalDevice, *m_surface);
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
    createInfo.oldSwapchain = *m_swapChain;

    m_swapChain = m_device->createSwapchainKHRUnique(createInfo);

    m_swapChainImages = m_device->getSwapchainImagesKHR(*m_swapChain);
    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent = extent;
}

void Engine::CreateImageViews()
{
    m_sceneImages.resize(m_swapChainImages.size());

    for (int i = 0; i < m_sceneImages.size(); i++)
    {
        m_sceneImages[i] = CreateImage(
            m_swapChainExtent.width, m_swapChainExtent.height,
            m_swapChainFormat, vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
            VMA_MEMORY_USAGE_GPU_ONLY);
    }

    m_swapChainImageViews.resize(m_swapChainImages.size());

    for (size_t i = 0; i < m_swapChainImages.size(); i++)
    {
        m_swapChainImageViews[i] =
            CreateImageView(m_swapChainImages[i],
                            m_swapChainImageFormat,
                            vk::ImageAspectFlagBits::eColor);
    }

    m_sceneImageViews.resize(m_sceneImages.size());
    for (size_t i = 0; i < m_sceneImageViews.size(); i++)
    {
        m_sceneImageViews[i] =
            CreateImageView(m_sceneImages[i].image,
                            m_swapChainImageFormat,
                            vk::ImageAspectFlagBits::eColor);
    }

    m_bloomImageViews.resize(m_bloomImages.size());
    for (size_t i = 0; i < m_bloomImageViews.size(); i++)
    {
        m_bloomImageViews[i] =
            CreateImageView(m_bloomImages[i].image,
                            m_swapChainImageFormat,
                            vk::ImageAspectFlagBits::eColor);
    }
}

vk::UniqueShaderModule Engine::CreateShaderModule(const std::vector<uint32_t>& data)
{
    vk::ShaderModuleCreateInfo createInfo;

    createInfo.codeSize = 4*data.size();
    createInfo.pCode = data.data();

    return m_device->createShaderModuleUnique(createInfo);
}

void Engine::CreateRenderPass()
{
    vk::AttachmentDescription colorAttachment;
    colorAttachment.format = m_swapChainImageFormat;
    colorAttachment.samples = vk::SampleCountFlagBits::e1;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachment.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    vk::AttachmentDescription bloomAttachment;
    bloomAttachment.format = m_swapChainImageFormat;
    bloomAttachment.samples = vk::SampleCountFlagBits::e1;
    bloomAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    bloomAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    bloomAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    bloomAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    bloomAttachment.initialLayout = vk::ImageLayout::eUndefined;
    bloomAttachment.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    vk::AttachmentDescription depthAttachment;
    depthAttachment.format = FindDepthFormat();
    depthAttachment.samples = vk::SampleCountFlagBits::e1;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
    depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
    depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::AttachmentReference colorAttachmentRef;
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::AttachmentReference bloomAttachmentRef;
    bloomAttachmentRef.attachment = 1;
    bloomAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::AttachmentReference depthAttachmentRef;
    depthAttachmentRef.attachment = 2;
    depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    auto colorAttachments = {colorAttachmentRef, bloomAttachmentRef};
    vk::SubpassDescription subpass;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.setColorAttachments(colorAttachments);
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    std::array<vk::SubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask =
        vk::PipelineStageFlagBits::eColorAttachmentOutput |
        vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependencies[0].srcAccessMask = vk::AccessFlagBits::eNoneKHR;
    dependencies[0].dstStageMask =
        vk::PipelineStageFlagBits::eColorAttachmentOutput |
        vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependencies[0].dstAccessMask =
        vk::AccessFlagBits::eColorAttachmentWrite |
        vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask =
        vk::PipelineStageFlagBits::eColorAttachmentOutput |
        vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependencies[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead;
    dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
    dependencies[1].dstAccessMask = vk::AccessFlagBits::eShaderRead;

    std::array attachments {colorAttachment, bloomAttachment, depthAttachment};

    vk::RenderPassCreateInfo createInfo;
    createInfo.setAttachments(attachments);
    createInfo.setSubpasses(subpass);
    createInfo.setDependencies(dependencies);

    m_renderPass = m_device->createRenderPassUnique(createInfo);
}

void Engine::CreateAdditiveBlendingRenderPass()
{
    vk::AttachmentDescription colorAttachment;
    colorAttachment.format = m_swapChainImageFormat;
    colorAttachment.samples = vk::SampleCountFlagBits::e1;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference colorAttachmentRef;
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    auto colorAttachments = {colorAttachmentRef};
    vk::SubpassDescription subpass;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.setColorAttachments(colorAttachments);

    vk::SubpassDependency dependency;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask =
        vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead;
    dependency.dstStageMask =
        vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstAccessMask =
        vk::AccessFlagBits::eColorAttachmentWrite;

    std::array attachments {colorAttachment};

    vk::RenderPassCreateInfo createInfo;
    createInfo.setAttachments(attachments);
    createInfo.setSubpasses(subpass);
    createInfo.setDependencies(dependency);

    m_additivePass = m_device->createRenderPassUnique(createInfo);
}

void Engine::CreateBloomRenderPasses()
{
    vk::AttachmentDescription colorAttachment;
    colorAttachment.format = m_swapChainImageFormat;
    colorAttachment.samples = vk::SampleCountFlagBits::e1;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachment.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    vk::AttachmentReference colorAttachmentRef;
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    auto colorAttachments = {colorAttachmentRef};
    vk::SubpassDescription subpass;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.setColorAttachments(colorAttachments);

    std::array<vk::SubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask =
        vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependencies[0].srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead;
    dependencies[0].dstStageMask =
        vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependencies[0].dstAccessMask =
        vk::AccessFlagBits::eColorAttachmentWrite;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask =
        vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependencies[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead;
    dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
    dependencies[1].dstAccessMask = vk::AccessFlagBits::eShaderRead;

    std::array attachments {colorAttachment};

    vk::RenderPassCreateInfo createInfo;
    createInfo.setAttachments(attachments);
    createInfo.setSubpasses(subpass);
    createInfo.setDependencies(dependencies);

    m_verticalBloomRenderPass = m_device->createRenderPassUnique(createInfo);
    m_horizontalBloomRenderPass = m_device->createRenderPassUnique(createInfo);
}

void Engine::CreateBloomPipelines()
{
    auto vertex = CreateShaderModule(
        ShaderCompiler::CompileFromFile(
            Files::Local("res/shaders/blur.vert"),
            shaderc_shader_kind::shaderc_vertex_shader));

    vk::PipelineLayoutCreateInfo hInfo;
    hInfo.setSetLayouts(*m_horizontalDescriptorSetLayout);
    m_horizontalBloomPipelineLayout =
        m_device->createPipelineLayoutUnique(hInfo);

    m_horizontalBloomPipeline = CreateWholeScreenPipeline(
        *vertex,
        *CreateShaderModule(
            ShaderCompiler::CompileFromFile(
                Files::Local("res/shaders/horizontal_blur.frag"),
                shaderc_shader_kind::shaderc_fragment_shader)),
        *m_horizontalBloomPipelineLayout, *m_horizontalBloomRenderPass,
        VK_FALSE, 1);

    vk::PipelineLayoutCreateInfo vInfo;
    vInfo.setSetLayouts(*m_verticalDescriptorSetLayout);
    m_verticalBloomPipelineLayout = m_device->createPipelineLayoutUnique(vInfo);

    m_verticalBloomPipeline = CreateWholeScreenPipeline(
        *vertex,
        *CreateShaderModule(
            ShaderCompiler::CompileFromFile(
                Files::Local("res/shaders/vertical_blur.frag"),
                shaderc_shader_kind::shaderc_fragment_shader)),
        *m_verticalBloomPipelineLayout, *m_verticalBloomRenderPass,
        VK_FALSE, 1);

    vk::PipelineLayoutCreateInfo aInfo;
    aInfo.setSetLayouts(*m_additiveDescriptorSetLayout);
    m_additivePipelineLayout = m_device->createPipelineLayoutUnique(aInfo);

    m_additivePipeline = CreateWholeScreenPipeline(
        *vertex,
        *CreateShaderModule(
            ShaderCompiler::CompileFromFile(
                Files::Local("res/shaders/additive_blend.frag"),
                shaderc_shader_kind::shaderc_fragment_shader)),
        *m_additivePipelineLayout, *m_additivePass,
        VK_FALSE, 1);
}

void Engine::CreateBloomDescriptorSets()
{
    m_horizontalBloomDescriptorSet.resize(m_horizontalBloomImages.size());
    for (int i = 0; i < m_horizontalBloomDescriptorSet.size(); i++)
    {
        vk::DescriptorSetAllocateInfo allocInfo;
        allocInfo.descriptorPool = *m_bloomDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        auto layout = *m_horizontalDescriptorSetLayout;
        allocInfo.setSetLayouts(layout);

        m_horizontalBloomDescriptorSet[i] = m_device->allocateDescriptorSets(allocInfo)[0];

        vk::DescriptorImageInfo imageInfo;
        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        imageInfo.imageView = *m_bloomImageViews[i];
        imageInfo.sampler = *m_bloomSampler;

        auto writes = {
            init::ImageWriteDescriptorSet(0, m_horizontalBloomDescriptorSet[i], imageInfo)
        };

        m_device->updateDescriptorSets(writes, nullptr);
    }

    m_verticalBloomDescriptorSet.resize(m_bloomImages.size());
    for (int i = 0; i < m_verticalBloomDescriptorSet.size(); i++)
    {
        vk::DescriptorSetAllocateInfo allocInfo;
        allocInfo.descriptorPool = *m_bloomDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        auto layout = *m_verticalDescriptorSetLayout;
        allocInfo.setSetLayouts(layout);

        m_verticalBloomDescriptorSet[i] = m_device->allocateDescriptorSets(allocInfo)[0];

        vk::DescriptorImageInfo imageInfo;
        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        imageInfo.imageView = *m_horizontalBloomImageViews[i];
        imageInfo.sampler = *m_bloomSampler;

        auto writes = {
            init::ImageWriteDescriptorSet(0, m_verticalBloomDescriptorSet[i], imageInfo)
        };

        m_device->updateDescriptorSets(writes, nullptr);
    }

    m_additiveDescriptorSet.resize(m_bloomImages.size());
    for (int i = 0; i < m_additiveDescriptorSet.size(); i++)
    {
        vk::DescriptorSetAllocateInfo allocInfo;
        allocInfo.descriptorPool = *m_bloomDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        auto layout = *m_additiveDescriptorSetLayout;
        allocInfo.setSetLayouts(layout);

        m_additiveDescriptorSet[i] = m_device->allocateDescriptorSets(allocInfo)[0];

        vk::DescriptorImageInfo imageInfo;
        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        imageInfo.imageView = *m_sceneImageViews[i];
        imageInfo.sampler = *m_bloomSampler;

        vk::DescriptorImageInfo bloomInfo;
        bloomInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        bloomInfo.imageView = *m_bloomImageViews[i];
        bloomInfo.sampler = *m_bloomSampler;

        auto writes = {
            init::ImageWriteDescriptorSet(0, m_additiveDescriptorSet[i], imageInfo),
            init::ImageWriteDescriptorSet(1, m_additiveDescriptorSet[i], bloomInfo)
        };

        m_device->updateDescriptorSets(writes, nullptr);
    }
}

vk::UniquePipeline Engine::CreateWholeScreenPipeline(vk::ShaderModule vertexModule,
                                                     vk::ShaderModule fragmentModule,
                                                     vk::PipelineLayout pipelineLayout,
                                                     vk::RenderPass renderPass,
                                                     VkBool32 blendEnable,
                                                     int colorAspectsCount)
{
    vk::PipelineShaderStageCreateInfo vertCreateInfo;
    vertCreateInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertCreateInfo.module = vertexModule;
    vertCreateInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo fragCreateInfo;
    fragCreateInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragCreateInfo.module = fragmentModule;
    fragCreateInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertCreateInfo, fragCreateInfo};

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.vertexBindingDescriptionCount = 0;

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    inputAssemblyInfo.topology = vk::PrimitiveTopology::eTriangleList;
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    vk::Viewport viewport(0, 0, m_swapChainExtent.width, m_swapChainExtent.height, 0, 1);
    vk::Rect2D scissor({0, 0}, m_swapChainExtent);
    vk::PipelineViewportStateCreateInfo viewportStateInfo;
    viewportStateInfo.viewportCount = 1;
    viewportStateInfo.pViewports = &viewport;
    viewportStateInfo.scissorCount = 1;
    viewportStateInfo.pScissors = &scissor;

    vk::PipelineRasterizationStateCreateInfo rasterizerInfo;
    rasterizerInfo.depthClampEnable = VK_FALSE;
    rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizerInfo.polygonMode = vk::PolygonMode::eFill;
    rasterizerInfo.lineWidth = 1.0f;
    rasterizerInfo.cullMode = vk::CullModeFlagBits::eBack;
    rasterizerInfo.frontFace = vk::FrontFace::eClockwise;
    rasterizerInfo.depthBiasEnable = VK_FALSE;

    vk::PipelineMultisampleStateCreateInfo multisamplingInfo{};
    multisamplingInfo.sampleShadingEnable = VK_FALSE;
    multisamplingInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    colorBlendAttachment.blendEnable = blendEnable;
    colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
    colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

    std::vector<vk::PipelineColorBlendAttachmentState> blendAttachments(colorAspectsCount, colorBlendAttachment);
    vk::PipelineColorBlendStateCreateInfo colorBlendingInfo;
    colorBlendingInfo.logicOpEnable = VK_FALSE;
    colorBlendingInfo.setAttachments(blendAttachments);

    vk::PipelineDepthStencilStateCreateInfo depthStencil;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = vk::CompareOp::eLess;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    vk::GraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;

    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
    pipelineInfo.pViewportState = &viewportStateInfo;
    pipelineInfo.pRasterizationState = &rasterizerInfo;
    pipelineInfo.pMultisampleState = &multisamplingInfo;
    pipelineInfo.pColorBlendState = &colorBlendingInfo;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    return m_device->createGraphicsPipelineUnique(VK_NULL_HANDLE, pipelineInfo).value;
}


void Engine::CreateFramebuffers()
{
    m_swapChainFramebuffers.resize(m_swapChainImages.size());

    for (int i = 0; i < m_swapChainFramebuffers.size(); i++)
    {
        std::array attachments {
            m_swapChainImageViews[i].get(),
        };

        vk::FramebufferCreateInfo createInfo;
        createInfo.renderPass = *m_additivePass;
        createInfo.setAttachments(attachments);
        createInfo.layers = 1;
        createInfo.width = m_swapChainExtent.width;
        createInfo.height = m_swapChainExtent.height;

        m_swapChainFramebuffers[i] = m_device->createFramebufferUnique(createInfo);
    }

    m_sceneFramebuffers.resize(m_sceneImages.size());

    for (int i = 0; i < m_sceneFramebuffers.size(); i++)
    {
        std::array attachments {
            m_sceneImageViews[i].get(),
            m_bloomImageViews[i].get(),
            m_depthImageView.get()
        };

        vk::FramebufferCreateInfo createInfo;
        createInfo.renderPass = *m_renderPass;
        createInfo.setAttachments(attachments);
        createInfo.layers = 1;
        createInfo.width = m_swapChainExtent.width;
        createInfo.height = m_swapChainExtent.height;

        m_sceneFramebuffers[i] = m_device->createFramebufferUnique(createInfo);
    }
}

void Engine::CreateBloomFramebuffers()
{
    vk::SamplerCreateInfo samplerInfo;
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    m_bloomSampler = m_device->createSamplerUnique(samplerInfo);

    m_horizontalBloomImages.resize(m_sceneImages.size());
    m_horizontalBloomImageViews.resize(m_sceneImages.size());
    m_horizontalBloomFramebuffers.resize(m_sceneImages.size());
    for (int i = 0; i < m_horizontalBloomImages.size(); i++)
    {
        m_horizontalBloomImages[i] = CreateImage(
            m_swapChainExtent.width, m_swapChainExtent.height,
            m_swapChainFormat, vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
            VMA_MEMORY_USAGE_GPU_ONLY);

        m_horizontalBloomImageViews[i] = CreateImageView(
            m_horizontalBloomImages[i].image, m_swapChainFormat,
            vk::ImageAspectFlagBits::eColor);

        std::array attachments {
            *m_horizontalBloomImageViews[i]
        };

        vk::FramebufferCreateInfo createInfo;
        createInfo.renderPass = *m_horizontalBloomRenderPass;
        createInfo.setAttachments(attachments);
        createInfo.layers = 1;
        createInfo.width = m_swapChainExtent.width;
        createInfo.height = m_swapChainExtent.height;

        m_horizontalBloomFramebuffers[i] = m_device->createFramebufferUnique(createInfo);
    }



    m_bloomImages.resize(m_sceneImages.size());
    m_bloomImageViews.resize(m_sceneImages.size());
    m_verticalBloomFramebuffers.resize(m_sceneImages.size());
    for (int i = 0; i < m_bloomImages.size(); i++)
    {
        m_bloomImages[i] = CreateImage(
            m_swapChainExtent.width, m_swapChainExtent.height,
            m_swapChainFormat, vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
            VMA_MEMORY_USAGE_GPU_ONLY);

        m_bloomImageViews[i] = CreateImageView(
            m_bloomImages[i].image, m_swapChainFormat,
            vk::ImageAspectFlagBits::eColor);

        std::array attachments {
            *m_bloomImageViews[i]
        };

        vk::FramebufferCreateInfo createInfo;
        createInfo.renderPass = *m_verticalBloomRenderPass;
        createInfo.setAttachments(attachments);
        createInfo.layers = 1;
        createInfo.width = m_swapChainExtent.width;
        createInfo.height = m_swapChainExtent.height;

        m_verticalBloomFramebuffers[i] = m_device->createFramebufferUnique(createInfo);
    }
}

void Engine::CreateCommandPool()
{
    for (auto& frame : m_frames)
    {
        frame.commandBuffer.reset();
    }

    QueueFamilyIndices queueFamilyIndices =
        FindQueueFamilies(m_physicalDevice, *m_surface);

    vk::CommandPoolCreateInfo poolInfo;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    for (auto& frame : m_frames)
    {
        frame.commandPool = m_device->createCommandPoolUnique(poolInfo);
    }

    vk::CommandPoolCreateInfo uploadPoolInfo;
    uploadPoolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    m_uploadCommandPool = m_device->createCommandPoolUnique(uploadPoolInfo);
}

void Engine::CreateCommandBuffers()
{
    for (auto& frame : m_frames)
    {
        vk::CommandBufferAllocateInfo allocInfo;
        allocInfo.commandPool = *frame.commandPool;
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = 1;

        frame.commandBuffer = std::move(m_device->allocateCommandBuffersUnique(allocInfo).front());
    }
}

void Engine::RecreateCommandBuffer()
{
    ZoneScoped;
    CurrentFrame().commandBuffer->reset();
}

void Engine::CreateSyncObjects()
{
    m_imagesInFlight.resize(m_swapChainImages.size());

    vk::FenceCreateInfo fenceInfo;
    fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

    vk::SemaphoreCreateInfo semaphoreInfo;

    for (auto& frame : m_frames)
    {
        frame.renderSemaphore = m_device->createSemaphoreUnique(semaphoreInfo);
        frame.presentSemaphore = m_device->createSemaphoreUnique(semaphoreInfo);
        frame.renderFence = m_device->createFenceUnique(fenceInfo);
    }


    vk::FenceCreateInfo uploadFenceInfo;
    m_uploadFence = m_device->createFenceUnique(uploadFenceInfo);
}

uint32_t Engine::FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
    auto memProperties = m_physicalDevice.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

AllocatedBuffer Engine::CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                                     VmaMemoryUsage memoryUsage)
{
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = static_cast<VkBufferUsageFlags>(usage);

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;

    AllocatedBuffer newBuffer;

    vmaCreateBuffer(m_vmaAllocator, &bufferInfo, &vmaallocInfo,
                    &newBuffer.buffer,
                    &newBuffer.allocation,
                    nullptr);

    newBuffer.allocator = m_vmaAllocator;
    newBuffer.device = *m_device;

    return newBuffer;
}

void Engine::CopyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size)
{
    ImmediateSubmit([&](vk::CommandBuffer& commandBuffer)
    {
        vk::BufferCopy copyRegion;
        copyRegion.size = size;

        commandBuffer.copyBuffer(srcBuffer, dstBuffer, copyRegion);
    });
}

void Engine::CreateGlobalSetLayout()
{
    vk::DescriptorSetLayoutBinding uboLayoutBinding;
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eAllGraphics;

    vk::DescriptorSetLayoutBinding cubemapLayoutBinding;
    cubemapLayoutBinding.binding = 1;
    cubemapLayoutBinding.descriptorType =
        vk::DescriptorType::eCombinedImageSampler;
    cubemapLayoutBinding.descriptorCount = 1;
    cubemapLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

    auto bindings = {uboLayoutBinding, cubemapLayoutBinding};
    vk::DescriptorSetLayoutCreateInfo layoutInfo;
    layoutInfo.setBindings(bindings);

    m_globalSetLayout = m_device->createDescriptorSetLayoutUnique(layoutInfo);
}

void Engine::CreateTextureSetLayout()
{
    vk::DescriptorSetLayoutBinding albedo;
    albedo.binding = 0;
    albedo.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    albedo.descriptorCount = 1;
    albedo.stageFlags = vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutBinding normal;
    normal.binding = 1;
    normal.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    normal.descriptorCount = 1;
    normal.stageFlags = vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutBinding specular;
    specular.binding = 2;
    specular.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    specular.descriptorCount = 1;
    specular.stageFlags = vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutBinding rough;
    rough.binding = 3;
    rough.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    rough.descriptorCount = 1;
    rough.stageFlags = vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutBinding ao;
    ao.binding = 4;
    ao.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    ao.descriptorCount = 1;
    ao.stageFlags = vk::ShaderStageFlagBits::eFragment;

    auto bindings = {albedo, normal, specular, rough, ao};
    vk::DescriptorSetLayoutCreateInfo layoutInfo;
    layoutInfo.setBindings(bindings);

    m_textureSetLayout = m_device->createDescriptorSetLayoutUnique(layoutInfo);
}

void Engine::CreateBloomDescriptorSetLayouts()
{
    vk::DescriptorSetLayoutBinding inputImage;
    inputImage.binding = 0;
    inputImage.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    inputImage.descriptorCount = 1;
    inputImage.stageFlags = vk::ShaderStageFlagBits::eFragment;

    auto bindings = {inputImage};

    vk::DescriptorSetLayoutCreateInfo horizontalLayoutInfo;
    horizontalLayoutInfo.setBindings(bindings);
    m_horizontalDescriptorSetLayout = m_device->createDescriptorSetLayoutUnique(horizontalLayoutInfo);

    vk::DescriptorSetLayoutCreateInfo verticalLayoutInfo;
    verticalLayoutInfo.setBindings(bindings);
    m_verticalDescriptorSetLayout = m_device->createDescriptorSetLayoutUnique(verticalLayoutInfo);

    vk::DescriptorSetLayoutBinding otherInputImage;
    otherInputImage.binding = 1;
    otherInputImage.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    otherInputImage.descriptorCount = 1;
    otherInputImage.stageFlags = vk::ShaderStageFlagBits::eFragment;
    auto additiveBindings = {inputImage, otherInputImage};

    vk::DescriptorSetLayoutCreateInfo additiveLayoutInfo;
    additiveLayoutInfo.setBindings(additiveBindings);
    m_additiveDescriptorSetLayout = m_device->createDescriptorSetLayoutUnique(additiveLayoutInfo);
}

void Engine::CreateUniformBuffers()
{
    for (auto& frame : m_frames)
    {
        frame.sceneBuffer = CreateBuffer(sizeof(SceneData), vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }
}

void Engine::CreateDescriptorPool()
{
    std::vector<vk::DescriptorPoolSize> sizes {
        {vk::DescriptorType::eUniformBuffer, 100},
        {vk::DescriptorType::eSampledImage, 100},
        {vk::DescriptorType::eSampler, 100},
        {vk::DescriptorType::eCombinedImageSampler, 100}
    };

    vk::DescriptorPoolCreateInfo poolInfo;
    poolInfo.setPoolSizes(sizes);

    poolInfo.maxSets = 10;
    m_descriptorPool = m_device->createDescriptorPoolUnique(poolInfo);
}

void Engine::CreateBloomDescriptorPool()
{
    std::vector<vk::DescriptorPoolSize> sizes {
        {vk::DescriptorType::eUniformBuffer, 100},
        {vk::DescriptorType::eSampledImage, 100},
        {vk::DescriptorType::eSampler, 100},
        {vk::DescriptorType::eCombinedImageSampler, 100}
    };

    vk::DescriptorPoolCreateInfo poolInfo;
    poolInfo.setPoolSizes(sizes);

    poolInfo.maxSets = 100;
    m_bloomDescriptorPool = m_device->createDescriptorPoolUnique(poolInfo);
}

void Engine::CreateDescriptorSets()
{
    for (auto& frame : m_frames)
    {
        vk::DescriptorSetAllocateInfo allocInfo;

        allocInfo.descriptorPool = *m_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.setSetLayouts(*m_globalSetLayout);

        auto sets = m_device->allocateDescriptorSets(allocInfo);
        frame.globalDescriptor = sets.front();

        vk::DescriptorBufferInfo bufferInfo;
        bufferInfo.buffer = frame.sceneBuffer.buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(SceneData);

        vk::WriteDescriptorSet descriptorWrite;
        descriptorWrite.dstSet = frame.globalDescriptor;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.setBufferInfo(bufferInfo);

        m_device->updateDescriptorSets(descriptorWrite, nullptr);
    }
}

void Engine::Init(GLFWwindow* window)
{
    m_window = window;
    spdlog::info("Initializing Vulkan");
    CreateInstance();
    SetupDebugMessenger();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateVmaAllocator();
    CreateSwapChain();
    CreateImageViews();
    CreateRenderPass();
    CreateUniformBuffers();
    CreateGlobalSetLayout();
    CreateTextureSetLayout();
    CreateDescriptorPool();
    CreateBloomDescriptorPool();
    CreateDescriptorSets();
    CreateCommandPool();
    CreateDepthResources();

    CreateAdditiveBlendingRenderPass();
    CreateBloomRenderPasses();
    CreateBloomFramebuffers();
    CreateBloomDescriptorSetLayouts();
    CreateBloomDescriptorSets();
    CreateBloomPipelines();

    CreateFramebuffers();
    CreateCommandBuffers();
    CreateSyncObjects();

    CreateTracyContexts();
}

void Engine::CreateVmaAllocator()
{
    VmaAllocatorCreateInfo createInfo {};
    createInfo.device = *m_device;
    createInfo.instance = *m_instance;
    createInfo.physicalDevice = m_physicalDevice;
    createInfo.vulkanApiVersion = VK_API_VERSION_1_2;

    vmaCreateAllocator(&createInfo, &m_vmaAllocator);
}


vk::Format Engine::FindSupportedFormat(const std::vector<vk::Format>& candidates,
                               vk::ImageTiling tiling,
                               vk::FormatFeatureFlags features)
{
    for (vk::Format format : candidates) {
        vk::FormatProperties props = m_physicalDevice.getFormatProperties(format);

        if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

AllocatedImage Engine::CreateImage(uint32_t width, uint32_t height, vk::Format format,
                                   vk::ImageUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    vk::ImageCreateInfo imageInfo;
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.usage = usage;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;

    AllocatedImage result;
    result.allocator = GetVmaAllocator();
    result.device = *m_device;

    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = memoryUsage;

    VkImageCreateInfo info = imageInfo;

    vmaCreateImage(GetVmaAllocator(), &info, &dimg_allocinfo,
                   &result.image, &result.allocation, nullptr);
    return result;
}

vk::UniqueImageView Engine::CreateImageView(vk::Image image, vk::Format format,
                                         vk::ImageAspectFlags aspectFlags)
{
    vk::ImageViewCreateInfo viewInfo;
    viewInfo.image = image;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    return m_device->createImageViewUnique(viewInfo);
}

void Engine::CreateDepthResources()
{
    vk::Format depthFormat = FindDepthFormat();
    m_depthImage = CreateImage(m_swapChainExtent.width, m_swapChainExtent.height,
                               depthFormat, vk::ImageUsageFlagBits::eDepthStencilAttachment,
                               VMA_MEMORY_USAGE_GPU_ONLY);
    m_depthImageView = CreateImageView(m_depthImage.image, depthFormat,
                                       vk::ImageAspectFlagBits::eDepth);
}

void Engine::WriteGlobalUniformBuffer()
{
    m_ubo.projInv = glm::inverse(m_ubo.proj);
    m_ubo.viewInv = glm::inverse(m_ubo.view);
    m_ubo.projview = m_ubo.proj * m_ubo.view;
    m_ubo.resolution = {m_swapChainExtent.width, m_swapChainExtent.height};

    void* data;
    vmaMapMemory(m_vmaAllocator, CurrentFrame().sceneBuffer.allocation, &data);
    memcpy(data, &m_ubo, sizeof(m_ubo));
    vmaUnmapMemory(m_vmaAllocator, CurrentFrame().sceneBuffer.allocation);
}

void Engine::Terminate()
{
    m_device->waitIdle();
    for (auto ctx : m_tracyCtxs)
        TracyVkDestroy(ctx);

    vmaDestroyAllocator(m_vmaAllocator);
}

void Engine::CreateTracyContexts()
{
    for (auto& frame : m_frames)
    {
        auto ctx = TracyVkContext(m_physicalDevice, *m_device,
                                  m_graphicsQueue, *frame.commandBuffer);
        m_tracyCtxs.push_back(ctx);
    }
}

vk::CommandBuffer Engine::BeginFrame()
{
    auto r = m_device->waitForFences(*CurrentFrame().renderFence,
                                     VK_TRUE, UINT64_MAX);
    auto acquireResult = m_device->acquireNextImageKHR(
        *m_swapChain, UINT64_MAX, *CurrentFrame().presentSemaphore);

    if (acquireResult.result == vk::Result::eErrorOutOfDateKHR)
    {
        RecreateSwapChain();
        return BeginFrame();
    }
    else if (acquireResult.result != vk::Result::eSuccess &&
             acquireResult.result != vk::Result::eSuboptimalKHR)
    {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    WriteGlobalUniformBuffer();

    uint32_t imageIndex = acquireResult.value;
    m_currentImageIndex = imageIndex;

    if (m_imagesInFlight[imageIndex].has_value())
    {
        auto re = m_device->waitForFences(m_imagesInFlight[imageIndex].value(),
                                          VK_TRUE, UINT64_MAX);
    }

    m_imagesInFlight[imageIndex] = *CurrentFrame().renderFence;

    RecreateCommandBuffer();
    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    CurrentFrame().commandBuffer->begin(beginInfo);

    return *CurrentFrame().commandBuffer;
}

void Engine::EndFrame()
{
    uint32_t imageIndex = m_currentImageIndex;
    TracyVkCollect(m_tracyCtxs[m_currentFrame], *CurrentFrame().commandBuffer);
    CurrentFrame().commandBuffer->end();

    vk::SubmitInfo submitInfo;

    std::array waitSemaphores {*CurrentFrame().presentSemaphore};
    vk::PipelineStageFlags waitStages[] {
        vk::PipelineStageFlagBits::eColorAttachmentOutput
    };

    submitInfo.setWaitSemaphores(waitSemaphores);
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.setCommandBuffers(*CurrentFrame().commandBuffer);

    std::array signalSemaphores {*CurrentFrame().renderSemaphore};
    submitInfo.setSignalSemaphores(signalSemaphores);

    m_device->resetFences(*CurrentFrame().renderFence);

    m_graphicsQueue.submit(submitInfo, *CurrentFrame().renderFence);

    std::array swapchains {*m_swapChain};

    vk::PresentInfoKHR presentInfo;
    presentInfo.setWaitSemaphores(signalSemaphores);
    presentInfo.setSwapchains(swapchains);
    presentInfo.setImageIndices(imageIndex);

    VkQueue q = m_graphicsQueue;
    VkPresentInfoKHR p = presentInfo;
    auto presentResult = vkQueuePresentKHR(q, &p);
    if (presentResult == VK_SUBOPTIMAL_KHR ||
        presentResult == VK_ERROR_OUT_OF_DATE_KHR ||
        m_framebufferResized)
    {
        m_framebufferResized = false;
        RecreateSwapChain();
        return;
    }
    else if (presentResult != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }

    m_currentFrame = (m_currentFrame + 1) % m_max_frames_in_flight;
}

void Engine::BeginRenderPass(vk::CommandBuffer cmd)
{
    auto i = m_currentImageIndex;
    vk::RenderPassBeginInfo renderPassInfo;
    renderPassInfo.renderPass = *m_renderPass;
    renderPassInfo.framebuffer = *m_sceneFramebuffers[i];
    renderPassInfo.renderArea.offset = vk::Offset2D{0, 0};
    renderPassInfo.renderArea.extent = m_swapChainExtent;

    vk::ClearValue clearColor{std::array<float, 4>
                              {0.0f, 0.0f, 0.0f, 1.0f}};
    std::array clearValues {
        vk::ClearValue(vk::ClearColorValue(std::array{0.0f, 0.f, 0.f, 0.f})),
        vk::ClearValue(vk::ClearColorValue(std::array{0.0f, 0.f, 0.f, 0.f})),
        vk::ClearValue(vk::ClearDepthStencilValue(1.f, 0.f))
    };

    renderPassInfo.setClearValues(clearValues);

    cmd.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
}

void Engine::EndRenderPass(vk::CommandBuffer cmd)
{
    cmd.endRenderPass();

    auto i = m_currentImageIndex;
    std::array clearValues {
        vk::ClearValue(vk::ClearColorValue(std::array{0.0f, 0.f, 0.f, 0.f}))
    };
    vk::RenderPassBeginInfo horizontalBloomPassInfo;
    horizontalBloomPassInfo.renderPass = *m_horizontalBloomRenderPass;
    horizontalBloomPassInfo.framebuffer = *m_horizontalBloomFramebuffers[i];
    horizontalBloomPassInfo.renderArea.offset = vk::Offset2D{0, 0};
    horizontalBloomPassInfo.renderArea.extent = m_swapChainExtent;
    horizontalBloomPassInfo.setClearValues(clearValues);
    cmd.beginRenderPass(horizontalBloomPassInfo, vk::SubpassContents::eInline);
    {
        TracyVkZone(GetCurrentTracyContext(), cmd, "Bloom horizontal pass");

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_horizontalBloomPipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               *m_horizontalBloomPipelineLayout, 0,
                               m_horizontalBloomDescriptorSet[i], nullptr);


        cmd.draw(3, 1, 0, 0);
    }
    cmd.endRenderPass();

    vk::RenderPassBeginInfo verticalBloomPassInfo;
    verticalBloomPassInfo.renderPass = *m_verticalBloomRenderPass;
    verticalBloomPassInfo.framebuffer = *m_verticalBloomFramebuffers[i];
    verticalBloomPassInfo.renderArea.offset = vk::Offset2D{0, 0};
    verticalBloomPassInfo.renderArea.extent = m_swapChainExtent;
    verticalBloomPassInfo.setClearValues(clearValues);
    cmd.beginRenderPass(verticalBloomPassInfo, vk::SubpassContents::eInline);
    {
        TracyVkZone(GetCurrentTracyContext(), cmd, "Bloom vertical pass");

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_verticalBloomPipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               *m_verticalBloomPipelineLayout, 0,
                               m_verticalBloomDescriptorSet[i], nullptr);

        cmd.draw(3, 1, 0, 0);
    }
    cmd.endRenderPass();

    vk::RenderPassBeginInfo additivePassInfo;
    additivePassInfo.renderPass = *m_additivePass;
    additivePassInfo.framebuffer = *m_swapChainFramebuffers[i];
    additivePassInfo.renderArea.offset = vk::Offset2D{0, 0};
    additivePassInfo.renderArea.extent = m_swapChainExtent;
    additivePassInfo.setClearValues(clearValues);
    cmd.beginRenderPass(additivePassInfo, vk::SubpassContents::eInline);
    {
        TracyVkZone(GetCurrentTracyContext(), cmd, "Additive pass");

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_additivePipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               *m_additivePipelineLayout, 0,
                               m_additiveDescriptorSet[i], nullptr);

        cmd.draw(3, 1, 0, 0);
    }

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    cmd.endRenderPass();
}

vk::UniquePipelineLayout Engine::CreatePushConstantsLayout(
    const vk::ArrayProxyNoTemporaries<const vk::PushConstantRange>
    &pushConstantRanges) const
{
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    auto layout = GetGlobalSetLayout();
    pipelineLayoutInfo.setSetLayouts(layout);
    pipelineLayoutInfo.setPushConstantRanges(pushConstantRanges);

    return m_device->createPipelineLayoutUnique(pipelineLayoutInfo);
}

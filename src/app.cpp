#include "app.hpp"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#include <spdlog/spdlog.h>
#include <optional>
#include <set>
#include <algorithm>
#include <ranges>
#include "shader_compiler.hpp"
#include "files.hpp"
#include "vertex.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <chrono>
#include <imgui.h>
#include "bindings/imgui_impl_glfw.h"
#include "bindings/imgui_impl_vulkan.h"

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

static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
    app->m_framebufferResized = true;
}

static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
{
    static double px = xpos, py = ypos;

    double dx = xpos - px;
    double dy = ypos - py;

    px = xpos;
    py = ypos;

    auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));

    int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    if (state == GLFW_PRESS)
    {
        const glm::vec3 sideways = glm::normalize(glm::cross({0.f, 1.f, 0.f}, app->m_camera.direction));

        app->m_camera.direction = glm::rotate(static_cast<float>(-dx * 0.001f),
                                              glm::vec3{0, 1, 0}) * glm::vec4(app->m_camera.direction, 1.f);

        app->m_camera.direction = glm::rotate(static_cast<float>(dy * 0.001f),
                                              sideways) * glm::vec4(app->m_camera.direction, 1.f);
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
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
    glfwSetCursorPosCallback(m_window, cursorPositionCallback);


    m_camera.SetPerspective(2.f, m_width / static_cast<float>(m_height),
                            0.01, 100);

    m_camera.position = {0.f, 2.f, 5.f};
    m_camera.direction = {0.f, 0.f, -1.f};
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

    m_instance = vk::createInstanceUnique(createInfo);

    m_dispatcher = vk::DispatchLoaderDynamic(*m_instance, vkGetInstanceProcAddr);
    spdlog::info("Create vulkan instance successful");
}

void App::SetupDebugMessenger()
{
    if (!m_validationLayers) return;

    auto createInfo = MessengerCreateInfo();

    m_debugMessenger = m_instance->createDebugUtilsMessengerEXTUnique(
        createInfo, nullptr, m_dispatcher);
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

void App::PickPhysicalDevice()
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
    spdlog::info("Physical device picked");
}

void App::CreateLogicalDevice()
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

void App::CreateSurface()
{
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(*m_instance, m_window, nullptr, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }

    m_surface = vk::UniqueSurfaceKHR(surface, *m_instance);
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
        return mode == vk::PresentModeKHR::eMailbox;
    });

    return it != modes.end() ? *it : vk::PresentModeKHR::eFifo;
}

void App::CreateSwapChain()
{
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(m_physicalDevice, *m_surface);

    auto surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
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

void App::CreateImageViews()
{
    m_swapChainImageViews.resize(m_swapChainImages.size());

    for (size_t i = 0; i < m_swapChainImages.size(); i++)
    {
        m_swapChainImageViews[i] =
            CreateImageView(m_swapChainImages[i],
                            m_swapChainImageFormat,
                            vk::ImageAspectFlagBits::eColor);
    }
}

vk::UniqueShaderModule App::CreateShaderModule(const std::vector<uint32_t>& data)
{
    vk::ShaderModuleCreateInfo createInfo;

    createInfo.codeSize = 4*data.size();
    createInfo.pCode = data.data();

    return m_device->createShaderModuleUnique(createInfo);
}

void App::CreateRenderPass()
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

    vk::AttachmentReference depthAttachmentRef;
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::SubpassDescription subpass;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.setColorAttachments(colorAttachmentRef);
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    vk::SubpassDependency dependency;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask =
        vk::PipelineStageFlagBits::eColorAttachmentOutput |
        vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
    dependency.dstStageMask =
        vk::PipelineStageFlagBits::eColorAttachmentOutput |
        vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.dstAccessMask =
        vk::AccessFlagBits::eColorAttachmentWrite |
        vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    std::array attachments {colorAttachment, depthAttachment};

    vk::RenderPassCreateInfo createInfo;
    createInfo.setAttachments(attachments);
    createInfo.setSubpasses(subpass);
    createInfo.setDependencies(dependency);

    m_renderPass = m_device->createRenderPassUnique(createInfo);
}

vk::UniquePipeline App::CreateWholeScreenPipeline(vk::ShaderModule vertexModule,
                                                  vk::ShaderModule fragmentModule)
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

    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
    colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

    vk::PipelineColorBlendStateCreateInfo colorBlendingInfo;
    colorBlendingInfo.logicOpEnable = VK_FALSE;
    colorBlendingInfo.attachmentCount = 1;
    colorBlendingInfo.pAttachments = &colorBlendAttachment;

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.setSetLayouts(*m_descriptorSetLayout);

    m_pipelineLayout = m_device->createPipelineLayoutUnique(pipelineLayoutInfo);

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
    pipelineInfo.layout = *m_pipelineLayout;
    pipelineInfo.renderPass = *m_renderPass;
    pipelineInfo.subpass = 0;

    return m_device->createGraphicsPipelineUnique(VK_NULL_HANDLE, pipelineInfo).value;
}

void App::CreateGraphicsPipeline()
{
    auto vertModule = CreateShaderModule(
        ShaderCompiler::CompileFromFile(
            Files::Local("shaders/triangle.vert"),
            shaderc_shader_kind::shaderc_glsl_vertex_shader));

    auto fragModule =
        CreateShaderModule(
            ShaderCompiler::CompileFromFile(
                Files::Local("shaders/triangle.frag"),
                shaderc_shader_kind::shaderc_glsl_fragment_shader));

    vk::PipelineShaderStageCreateInfo vertCreateInfo;
    vertCreateInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertCreateInfo.module = *vertModule;
    vertCreateInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo fragCreateInfo;
    fragCreateInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragCreateInfo.module = *fragModule;
    fragCreateInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertCreateInfo, fragCreateInfo};

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    auto bindingDescription = Vertex::GetBindingDescription();
    auto attributeDescriptions = Vertex::GetAttributeDescriptions();
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
    vertexInputInfo.setVertexBindingDescriptions(bindingDescription);
    vertexInputInfo.setVertexAttributeDescriptions(attributeDescriptions);

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    inputAssemblyInfo.topology = vk::PrimitiveTopology::eTriangleFan;
    inputAssemblyInfo.primitiveRestartEnable = VK_TRUE;

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
    rasterizerInfo.polygonMode = vk::PolygonMode::eLine;
    rasterizerInfo.lineWidth = 1.0f;
    rasterizerInfo.cullMode = vk::CullModeFlagBits::eBack;
    rasterizerInfo.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizerInfo.depthBiasEnable = VK_FALSE;

    vk::PipelineMultisampleStateCreateInfo multisamplingInfo{};
    multisamplingInfo.sampleShadingEnable = VK_FALSE;
    multisamplingInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
    colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

    vk::PipelineColorBlendStateCreateInfo colorBlendingInfo;
    colorBlendingInfo.logicOpEnable = VK_FALSE;
    colorBlendingInfo.attachmentCount = 1;
    colorBlendingInfo.pAttachments = &colorBlendAttachment;

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.setSetLayouts(*m_descriptorSetLayout);

    m_pipelineLayout = m_device->createPipelineLayoutUnique(pipelineLayoutInfo);

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
    pipelineInfo.layout = *m_pipelineLayout;
    pipelineInfo.renderPass = *m_renderPass;
    pipelineInfo.subpass = 0;

    m_graphicsPipeline = m_device->createGraphicsPipelineUnique(VK_NULL_HANDLE, pipelineInfo).value;
}

void App::CreateFramebuffers()
{
    m_swapChainFramebuffers.resize(m_swapChainImages.size());

    for (int i = 0; i < m_swapChainFramebuffers.size(); i++)
    {
        std::array attachments {
            m_swapChainImageViews[i].get(),
            m_depthImageView.get()
        };

        vk::FramebufferCreateInfo createInfo;
        createInfo.renderPass = *m_renderPass;
        createInfo.setAttachments(attachments);
        createInfo.layers = 1;
        createInfo.width = m_swapChainExtent.width;
        createInfo.height = m_swapChainExtent.height;

        m_swapChainFramebuffers[i] = m_device->createFramebufferUnique(createInfo);
    }
}

void App::CreateCommandPool()
{
    // Here to properly deallocate command buffers during pool recreation
    m_commandBuffers.clear();

    QueueFamilyIndices queueFamilyIndices =
        FindQueueFamilies(m_physicalDevice, *m_surface);

    vk::CommandPoolCreateInfo poolInfo;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    m_commandPool = m_device->createCommandPoolUnique(poolInfo);
}

//TODO Grid in shader
//TODO Torus in shader
const std::vector<Vertex> vertices = {
    {{0, 0, 0}, {0, 0, 0}},
    {{0, 0.618, 1.618}, {1.f, 0.f, 0.f}},
    {{0, -0.618, 1.618}, {1.f, 0.f, 0.f}},
    {{0, -0.618, -1.618}, {1.f, 0.f, 0.f}},
    {{0, 0.618, -1.618}, {1.f, 0.f, 0.f}},
    {{1.618, 0, 0.618}, {1.f, 0.f, 0.f}},
    {{-1.618, 0, 0.618}, {1.f, 0.f, 0.f}},
    {{-1.618, 0, -0.618}, {1.f, 0.f, 0.f}},
    {{1.618, 0, -0.618}, {1.f, 0.f, 0.f}},
    {{0.618, 1.618, 0}, {1.f, 0.f, 0.f}},
    {{-0.618, 1.618, 0}, {1.f, 0.f, 0.f}},
    {{-0.618, -1.618, 0}, {1.f, 0.f, 0.f}},
    {{0.618, -1.618, 0}, {1.f, 0.f, 0.f}},
    {{1, 1, 1}, {1.f, 0.f, 0.f}},
    {{-1, 1, 1}, {1.f, 0.f, 0.f}},
    {{-1, -1, 1}, {1.f, 0.f, 0.f}},
    {{1, -1, 1}, {1.f, 0.f, 0.f}},
    {{1, -1, -1}, {1.f, 0.f, 0.f}},
    {{1, 1, -1}, {1.f, 0.f, 0.f}},
    {{-1, 1, -1}, {1.f, 0.f, 0.f}},
    {{-1, -1, -1}, {1.f, 0.f, 0.f}}
};

const std::vector<uint16_t> g_indices = {
    1, 2, 16, 5, 13, UINT16_MAX,
    1, 13, 9, 10, 14, UINT16_MAX,
    1, 14, 6, 15, 2, UINT16_MAX,
    2, 15, 11, 12, 16, UINT16_MAX,
    3, 4, 18, 8, 17, UINT16_MAX,
    3, 17, 12, 11, 20, UINT16_MAX,
    3, 20, 7, 19, 4, UINT16_MAX,
    19, 10, 9, 18, 4, UINT16_MAX,
    16, 12, 17, 8, 5, UINT16_MAX,
    5, 8, 18, 9, 13, UINT16_MAX,
    14, 10, 19, 7, 6, UINT16_MAX,
    6, 7, 20, 11, 15
};

void App::CreateCommandBuffers()
{
    m_commandBuffers.resize(m_swapChainFramebuffers.size());

    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.commandPool = *m_commandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = m_commandBuffers.size();

    m_commandBuffers = m_device->allocateCommandBuffersUnique(allocInfo);
}

void App::RecreateCommandBuffer(uint32_t i)
{
    m_commandBuffers[i]->reset();
    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    m_commandBuffers[i]->begin(beginInfo);

    vk::RenderPassBeginInfo renderPassInfo;
    renderPassInfo.renderPass = *m_renderPass;
    renderPassInfo.framebuffer = *m_swapChainFramebuffers[i];
    renderPassInfo.renderArea.offset = vk::Offset2D{0, 0};
    renderPassInfo.renderArea.extent = m_swapChainExtent;

    vk::ClearValue clearColor{std::array<float, 4>
                              {0.0f, 0.0f, 0.0f, 1.0f}};
    std::array clearValues {
        vk::ClearValue(vk::ClearColorValue(std::array{0.0f, 0.f, 0.f, 0.f})),
        vk::ClearValue(vk::ClearDepthStencilValue(1.f, 0.f))
    };

    renderPassInfo.setClearValues(clearValues);

    m_commandBuffers[i]->beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

    m_commandBuffers[i]->bindPipeline(vk::PipelineBindPoint::eGraphics, *m_graphicsPipeline);

    std::array<vk::Buffer, 1> vertexBuffers {*m_vertexBuffer};
    std::array<vk::DeviceSize, 1> offsets = {0};

    m_commandBuffers[i]->bindVertexBuffers(0, vertexBuffers, offsets);
    m_commandBuffers[i]->bindIndexBuffer(*m_indexBuffer, 0, vk::IndexType::eUint16);

    m_commandBuffers[i]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                            *m_pipelineLayout, 0, m_descriptorSets[i],
                                            nullptr);

    m_commandBuffers[i]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                            *m_pipelineLayout, 0, m_descriptorSets[i],
                                            nullptr);

    m_commandBuffers[i]->drawIndexed(static_cast<uint32_t>(g_indices.size()), 1, 0, 0, 0);

    m_commandBuffers[i]->bindPipeline(vk::PipelineBindPoint::eGraphics, *m_gridPipeline);

    m_commandBuffers[i]->draw(3, 1, 0, 0);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *m_commandBuffers[i]);

    m_commandBuffers[i]->endRenderPass();
    m_commandBuffers[i]->end();
}

void App::CreateSyncObjects()
{
    m_imageAvailableSemaphores.resize(m_max_frames_in_flight);
    m_renderFinishedSemaphores.resize(m_max_frames_in_flight);
    m_inFlightFences.resize(m_max_frames_in_flight);
    m_imagesInFlight.resize(m_swapChainImages.size());

    vk::FenceCreateInfo fenceInfo;
    fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;
    for (size_t i = 0; i < m_max_frames_in_flight; i++)
    {
        m_imageAvailableSemaphores[i] = m_device->createSemaphoreUnique({});
        m_renderFinishedSemaphores[i] = m_device->createSemaphoreUnique({});
        m_inFlightFences[i] = m_device->createFenceUnique(fenceInfo);
    }
}

uint32_t App::FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
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

void App::CreateVertexBuffer()
{
    vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    vk::UniqueBuffer stagingBuffer;
    vk::UniqueDeviceMemory stagingBufferMemory;
    CreateBuffer(bufferSize,
                 vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eHostVisible |
                 vk::MemoryPropertyFlagBits::eHostCoherent,
                 stagingBuffer, stagingBufferMemory);

    void* data = m_device->mapMemory(*stagingBufferMemory, 0, bufferSize);

    memcpy(data, vertices.data(), bufferSize);

    m_device->unmapMemory(*stagingBufferMemory);

    CreateBuffer(bufferSize,
                 vk::BufferUsageFlagBits::eTransferDst |
                 vk::BufferUsageFlagBits::eVertexBuffer,
                 vk::MemoryPropertyFlagBits::eDeviceLocal,
                 m_vertexBuffer, m_vertexBufferMemory);

    CopyBuffer(*stagingBuffer, *m_vertexBuffer, bufferSize);
}

void App::CreateIndexBuffer()
{
    vk::DeviceSize bufferSize = sizeof(g_indices[0]) * g_indices.size();
    vk::UniqueBuffer stagingBuffer;
    vk::UniqueDeviceMemory stagingBufferMemory;
    CreateBuffer(bufferSize,
                 vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eHostVisible |
                 vk::MemoryPropertyFlagBits::eHostCoherent,
                 stagingBuffer, stagingBufferMemory);

    void* data = m_device->mapMemory(*stagingBufferMemory, 0, bufferSize);

    memcpy(data, g_indices.data(), bufferSize);

    m_device->unmapMemory(*stagingBufferMemory);

    CreateBuffer(bufferSize,
                 vk::BufferUsageFlagBits::eTransferDst |
                 vk::BufferUsageFlagBits::eIndexBuffer,
                 vk::MemoryPropertyFlagBits::eDeviceLocal,
                 m_indexBuffer, m_indexBufferMemory);

    CopyBuffer(*stagingBuffer, *m_indexBuffer, bufferSize);
}

void App::CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                  vk::MemoryPropertyFlags properties,
                  vk::UniqueBuffer& buffer, vk::UniqueDeviceMemory& bufferMemory)
{
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    buffer = m_device->createBufferUnique(bufferInfo);

    auto memRequirementes = m_device->getBufferMemoryRequirements(*buffer);

    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memRequirementes.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirementes.memoryTypeBits,
                                               properties);

    bufferMemory = m_device->allocateMemoryUnique(allocInfo, nullptr);

    m_device->bindBufferMemory(*buffer, *bufferMemory, 0);
}

void App::CopyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size)
{
    ImmediateSubmit([&](vk::CommandBuffer& commandBuffer)
    {
        vk::BufferCopy copyRegion;
        copyRegion.size = size;

        commandBuffer.copyBuffer(srcBuffer, dstBuffer, copyRegion);
    });
}

void App::CreateDescriptorSetLayout()
{
    vk::DescriptorSetLayoutBinding uboLayoutBinding;
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutCreateInfo layoutInfo;
    layoutInfo.setBindings(uboLayoutBinding);

    m_descriptorSetLayout = m_device->createDescriptorSetLayoutUnique(layoutInfo);
}

void App::CreateUniformBuffers()
{
    vk::DeviceSize bufferSize = sizeof(m_ubo);
    m_uniformBuffers.resize(m_swapChainImages.size());
    m_uniformBuffersMemory.resize(m_swapChainImages.size());

    for (size_t i = 0; i < m_swapChainImages.size(); i++)
    {
        CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
                     vk::MemoryPropertyFlagBits::eHostVisible |
                     vk::MemoryPropertyFlagBits::eHostCoherent,
                     m_uniformBuffers[i], m_uniformBuffersMemory[i]);
    }
}

void App::CreateDescriptorPool()
{
    vk::DescriptorPoolSize poolSize;
    poolSize.descriptorCount = m_swapChainImages.size();

    vk::DescriptorPoolCreateInfo poolInfo;
    poolInfo.setPoolSizes(poolSize);

    poolInfo.maxSets = m_swapChainImages.size();
    m_descriptorPool = m_device->createDescriptorPoolUnique(poolInfo);
}

void App::CreateDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(m_swapChainImages.size(),
                                                 *m_descriptorSetLayout);

    vk::DescriptorSetAllocateInfo allocInfo;

    allocInfo.descriptorPool = *m_descriptorPool;
    allocInfo.descriptorSetCount = m_swapChainImages.size();
    allocInfo.pSetLayouts = layouts.data();

    m_descriptorSets.resize(m_swapChainImages.size());
    m_descriptorSets = m_device->allocateDescriptorSets(allocInfo);

    for (size_t i = 0; i < m_swapChainImages.size(); i++)
    {
        vk::DescriptorBufferInfo bufferInfo;
        bufferInfo.buffer = *m_uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(m_ubo);

        vk::WriteDescriptorSet descriptorWrite;
        descriptorWrite.dstSet = m_descriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.setBufferInfo(bufferInfo);

        m_device->updateDescriptorSets(descriptorWrite, nullptr);
    }
}

void App::InitImGui()
{
    //1: create descriptor pool for IMGUI
    // the size of the pool is very oversize, but it's copied from imgui demo itself.
    VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    m_imguiDescriptorPool = m_device->createDescriptorPoolUnique(pool_info);


    // 2: initialize imgui library

    //this initializes the core structures of imgui
    ImGui::CreateContext();

    //this initializes imgui for SDL
    ImGui_ImplGlfw_InitForVulkan(m_window, true);

    //this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = *m_instance;
    init_info.PhysicalDevice = m_physicalDevice;
    init_info.Device = *m_device;
    init_info.Queue = m_graphicsQueue;
    init_info.DescriptorPool = *m_imguiDescriptorPool;
    init_info.MinImageCount = m_swapChainImages.size();
    init_info.ImageCount = m_swapChainImages.size();
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info, *m_renderPass);

    //execute a gpu command to upload imgui font textures
    ImmediateSubmit([&](VkCommandBuffer cmd) {
        ImGui_ImplVulkan_CreateFontsTexture(cmd);
    });

    //clear font textures from cpu data
    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void App::CreateWholeScreenPipelines()
{
    auto whole = CreateShaderModule(
        ShaderCompiler::CompileFromFile(
            Files::Local("shaders/whole.vert"),
            shaderc_shader_kind::shaderc_glsl_vertex_shader));

    auto grid = CreateShaderModule(
        ShaderCompiler::CompileFromFile(
            Files::Local("shaders/grid.frag"),
            shaderc_shader_kind::shaderc_glsl_fragment_shader));

    m_gridPipeline = CreateWholeScreenPipeline(*whole, *grid);

    //auto torus = CreateShaderModule(
    //    ShaderCompiler::CompileFromFile(
    //        Files::Local("shaders/torus.frag"),
    //        shaderc_shader_kind::shaderc_glsl_fragment_shader));

    //m_torusPipeline = CreateWholeScreenPipeline(*whole, *torus);
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
    CreateRenderPass();
    CreateUniformBuffers();
    CreateDescriptorSetLayout();
    CreateDescriptorPool();
    CreateDescriptorSets();
    CreateGraphicsPipeline();
    CreateWholeScreenPipelines();
    CreateCommandPool();
    CreateDepthResources();
    CreateFramebuffers();
    CreateVertexBuffer();
    CreateIndexBuffer();
    CreateCommandBuffers();
    CreateSyncObjects();
}


vk::Format App::FindSupportedFormat(const std::vector<vk::Format>& candidates,
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

void App::CreateImage(uint32_t width, uint32_t height, vk::Format format,
                      vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                      vk::MemoryPropertyFlags properties,
                      vk::UniqueImage& image,
                      vk::UniqueDeviceMemory& imageMemory)
{
    vk::ImageCreateInfo imageInfo;
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.usage = usage;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;

    image = m_device->createImageUnique(imageInfo);

    vk::MemoryRequirements memRequirements
        = m_device->getImageMemoryRequirements(*image);

    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    imageMemory = m_device->allocateMemoryUnique(allocInfo);
    m_device->bindImageMemory(*image, *imageMemory, 0);
}

vk::UniqueImageView App::CreateImageView(vk::Image image, vk::Format format,
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

void App::CreateDepthResources()
{
    vk::Format depthFormat = FindDepthFormat();
    CreateImage(m_swapChainExtent.width, m_swapChainExtent.height,
                depthFormat, vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eDepthStencilAttachment,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                m_depthImage, m_depthImageMemory);
    m_depthImageView = CreateImageView(*m_depthImage, depthFormat,
                                       vk::ImageAspectFlagBits::eDepth);
}

void App::UpdateUniformBuffer(uint32_t currentImage)
{
    void* data = m_device->mapMemory(*m_uniformBuffersMemory[currentImage], 0, sizeof(m_ubo));
    memcpy(data, &m_ubo, sizeof(m_ubo));
    m_device->unmapMemory(*m_uniformBuffersMemory[currentImage]);
}

void App::DrawFrame(float lag)
{
    ImGui::Render();
    auto r = m_device->waitForFences(*m_inFlightFences[m_currentFrame],
                                    VK_TRUE, UINT64_MAX);
    auto acquireResult = m_device->acquireNextImageKHR(
        *m_swapChain, UINT64_MAX, *m_imageAvailableSemaphores[m_currentFrame]);

    if (acquireResult.result == vk::Result::eErrorOutOfDateKHR)
    {
        RecreateSwapChain();
        return;
    }
    else if (acquireResult.result != vk::Result::eSuccess &&
             acquireResult.result != vk::Result::eSuboptimalKHR)
    {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    uint32_t imageIndex = acquireResult.value;

    if (m_imagesInFlight[imageIndex].has_value())
    {
        auto re = m_device->waitForFences(m_imagesInFlight[imageIndex].value(),
                                         VK_TRUE, UINT64_MAX);
    }

    m_imagesInFlight[imageIndex] = *m_inFlightFences[m_currentFrame];

    UpdateUniformBuffer(imageIndex);
    RecreateCommandBuffer(imageIndex);

    vk::SubmitInfo submitInfo;

    std::array waitSemaphores {*m_imageAvailableSemaphores[m_currentFrame]};
    vk::PipelineStageFlags waitStages[] {
        vk::PipelineStageFlagBits::eColorAttachmentOutput
    };

    submitInfo.setWaitSemaphores(waitSemaphores);
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.setCommandBuffers(*m_commandBuffers[imageIndex]);

    std::array signalSemaphores {*m_renderFinishedSemaphores[m_currentFrame]};
    submitInfo.setSignalSemaphores(signalSemaphores);

    m_device->resetFences(*m_inFlightFences[m_currentFrame]);

    m_graphicsQueue.submit(submitInfo, *m_inFlightFences[m_currentFrame]);

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

void App::InitClock()
{
    m_last_update = std::chrono::high_resolution_clock::now();
}

void App::UpdateClock()
{
    m_current_update = std::chrono::high_resolution_clock::now();
    m_lag += m_current_update - m_last_update;
    m_last_update = m_current_update;
}

void App::Update(float delta)
{

    int state = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT);
    if (state == GLFW_PRESS)
    {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    else
    {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    glm::vec3 forward = glm::normalize(m_camera.direction);
    glm::vec3 sideways = glm::normalize(glm::cross({0.f, 1.f, 0.f}, m_camera.direction));

    glm::vec3 move = {0, 0, 0};
    if (glfwGetKey(m_window, GLFW_KEY_W))
        move += forward;
    if (glfwGetKey(m_window, GLFW_KEY_S))
        move += -forward;
    if (glfwGetKey(m_window, GLFW_KEY_D))
        move += -sideways;
    if (glfwGetKey(m_window, GLFW_KEY_A))
        move += sideways;

    if (glm::length2(move) > 0)
        move = glm::normalize(move);

    m_camera.position += move * delta;
    m_ubo.model = glm::identity<glm::mat4>();
    m_ubo.model = glm::translate(m_ubo.model, glm::vec3{3, 2, 0});
    m_ubo.model = glm::scale(m_ubo.model, glm::vec3{1.f, 1.f, 1.f});
    m_ubo.view = m_camera.GetView();
    m_ubo.proj = m_camera.GetProjection();
}

void App::Loop()
{
    using namespace std::chrono;
    InitClock();

    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        UpdateClock();

        ImGui_ImplGlfw_NewFrame();
        ImGui_ImplVulkan_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Shaders");
        ImGui::Text("Average %.3f ms/frame (%.1f FPS)",
                    1000.0f / ImGui::GetIO().Framerate,
                    ImGui::GetIO().Framerate);
        if (ImGui::Button("Recompile Shaders"))
        {
            m_device->waitIdle();
            try
            {
                CreateGraphicsPipeline();
                CreateWholeScreenPipelines();
            }
            catch(const vk::Error& e)
            {
                spdlog::error("Recreating pipeline failed: {}", e.what());
            }
        }
        ImGui::End();

        ImGui::Begin("Camera");
        ImGui::Text("Position %.3f %.3f %.3f",
                    m_camera.position.x,
                    m_camera.position.y,
                    m_camera.position.z);

        ImGui::Text("Direction %.3f %.3f %.3f",
                    m_camera.direction.x,
                    m_camera.direction.y,
                    m_camera.direction.z);

        if (ImGui::BeginCombo("Projection", Projection::ToString(m_camera.GetProjectionType())))
        {
            for (const auto& name : Projection::projectionStrings)
            {
                if (ImGui::Selectable(name))
                {

                }
            }

            ImGui::EndCombo();
        }
        ImGui::End();

        while (m_lag > m_desired_delta)
        {
            m_lag -= m_desired_delta;
            Update(duration<float>(m_desired_delta).count());
        }

        DrawFrame(duration<float>(m_lag).count() /
                  duration<float>(m_desired_delta).count());
    }

    m_device->waitIdle();
}

void App::Terminate()
{
    ImGui_ImplVulkan_Shutdown();
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

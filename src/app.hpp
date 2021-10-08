#pragma once
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#include <memory>
#include <optional>
#include <glm/glm.hpp>
#include <chrono>
#include <concepts>
#include "camera.hpp"

static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
static void cursorPositionCallback(GLFWwindow* window, double dx, double dy);

class App
{
public:
    void Run()
    {
        InitWindow();
        InitVulkan();
        InitImGui();
        Loop();
        Terminate();
    }

    static const std::vector<const char*> s_validationLayers;
    static const std::vector<const char*> s_deviceExtensions;
private:
    void InitWindow();
    std::vector<const char*> GetRequiredExtensions();
    void CreateInstance();
    void InitVulkan();
    void SetupDebugMessenger();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSurface();
    void CreateSwapChain();
    void CreateImageViews();
    void CreateRenderPass();
    void CreateGraphicsPipeline();
    vk::UniquePipeline CreateWholeScreenPipeline(vk::ShaderModule vertexModule,
                                                 vk::ShaderModule fragmentModule);
    void CreateWholeScreenPipelines();
    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateDepthResources();
    void CreateCommandBuffers();
    void RecreateCommandBuffer(uint32_t imageIndex);
    void CreateSyncObjects();
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void CreateDescriptorSetLayout();
    void CreateUniformBuffers();
    void CreateDescriptorPool();
    void CreateDescriptorSets();
    void InitImGui();

    void CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                      vk::MemoryPropertyFlags properties,
                      vk::UniqueBuffer&, vk::UniqueDeviceMemory&);
    void CopyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);

    void RecreateSwapChain()
    {
        int width = 0, height = 0;
        glfwGetFramebufferSize(m_window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(m_window, &width, &height);
            glfwWaitEvents();
        }

        m_camera.SetPerspective(2.f, width / static_cast<float>(height),
                                0.01, 100);
        m_device->waitIdle();

        CreateSwapChain();
        CreateImageViews();
        CreateRenderPass();
        CreateGraphicsPipeline();
        CreateWholeScreenPipelines();
        CreateDepthResources();
        CreateFramebuffers();
        CreateUniformBuffers();
        CreateCommandPool();
        CreateDescriptorPool();
        CreateDescriptorSets();
        CreateCommandBuffers();
    }

    vk::UniqueShaderModule CreateShaderModule(const std::vector<uint32_t>&);
    void Loop();
    void Terminate();

    void InitClock();
    void UpdateClock();
    void Update(float delta);
    void DrawFrame(float lag);
    void UpdateUniformBuffer(uint32_t currentImage);

    template<std::invocable<vk::CommandBuffer&> T>
    void ImmediateSubmit(T func)
    {
        vk::CommandBufferAllocateInfo allocInfo;
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = 1;
        allocInfo.setCommandPool(*m_commandPool);

        auto commandBuffers = m_device->allocateCommandBuffersUnique(allocInfo);
        auto commandBuffer = *commandBuffers.front();

        vk::CommandBufferBeginInfo beginInfo;
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        commandBuffer.begin(beginInfo);

        func(commandBuffer);

        commandBuffer.end();

        vk::SubmitInfo submitInfo;
        submitInfo.setCommandBuffers(commandBuffer);

        m_graphicsQueue.submit(submitInfo);
        m_graphicsQueue.waitIdle();
    }


    using TimePoint = std::chrono::high_resolution_clock::time_point;
    using Duration = std::chrono::high_resolution_clock::duration;
    TimePoint m_last_update;
    Duration m_desired_delta = std::chrono::duration_cast<Duration>(std::chrono::duration<double>(1.0/60.0));
    Duration m_lag = std::chrono::high_resolution_clock::duration::zero();
    TimePoint m_current_update;

    unsigned m_width = 800;
    unsigned m_height = 600;
    const int m_max_frames_in_flight = 2;
    std::size_t m_currentFrame = 0;
    bool m_framebufferResized = false;

    struct UniformBufferObject
    {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    UniformBufferObject m_ubo;

    //NOTE: declaration order affects destruction order.
    //Device should be destroyed last
    vk::UniqueInstance m_instance;
    vk::UniqueDevice m_device;

    std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphores;
    std::vector<vk::UniqueSemaphore> m_renderFinishedSemaphores;
    std::vector<vk::UniqueFence> m_inFlightFences;
    std::vector<std::optional<vk::Fence>> m_imagesInFlight;

    GLFWwindow* m_window;
    vk::DispatchLoaderDynamic m_dispatcher;
    vk::UniqueHandle<vk::DebugUtilsMessengerEXT,
                     vk::DispatchLoaderDynamic> m_debugMessenger;
    vk::PhysicalDevice m_physicalDevice;
    vk::UniqueSurfaceKHR m_surface;
    vk::Queue m_graphicsQueue;
    vk::Queue m_presentQueue;
    vk::UniqueSwapchainKHR m_swapChain;
    std::vector<vk::Image> m_swapChainImages;
    std::vector<vk::UniqueImageView> m_swapChainImageViews;
    std::vector<vk::UniqueBuffer> m_uniformBuffers;
    std::vector<vk::UniqueDeviceMemory> m_uniformBuffersMemory;
    vk::UniqueImage m_depthImage;
    vk::UniqueDeviceMemory m_depthImageMemory;
    vk::UniqueImageView m_depthImageView;
    vk::Format m_swapChainImageFormat;
    vk::Extent2D m_swapChainExtent;
    vk::UniqueRenderPass m_renderPass;
    vk::UniqueDescriptorSetLayout m_descriptorSetLayout;
    vk::UniquePipelineLayout m_pipelineLayout;
    vk::UniquePipeline m_graphicsPipeline;
    vk::UniquePipeline m_gridPipeline;
    vk::UniquePipeline m_torusPipeline;
    std::vector<vk::UniqueFramebuffer> m_swapChainFramebuffers;
    vk::UniqueCommandPool m_commandPool;
    std::vector<vk::UniqueCommandBuffer> m_commandBuffers;
    vk::UniqueBuffer m_vertexBuffer;
    vk::UniqueDeviceMemory m_vertexBufferMemory;
    vk::UniqueBuffer m_indexBuffer;
    vk::UniqueDeviceMemory m_indexBufferMemory;
    vk::UniqueDescriptorPool m_descriptorPool;
    std::vector<vk::DescriptorSet> m_descriptorSets;
    vk::UniqueDescriptorPool m_imguiDescriptorPool;
#ifdef NDEBUG
    bool m_validationLayers = false;
#else
    bool m_validationLayers = true;
#endif

    uint32_t FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
    bool IsDeviceSuitable(const vk::PhysicalDevice&);

    static vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>&);
    static vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR&, GLFWwindow*);
    static vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>&);

    vk::Format FindSupportedFormat(const std::vector<vk::Format>&, vk::ImageTiling,
                                   vk::FormatFeatureFlags);
    vk::Format FindDepthFormat()
    {
        return FindSupportedFormat(
            {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
             vk::Format::eD24UnormS8Uint}, vk::ImageTiling::eOptimal,
            vk::FormatFeatureFlagBits::eDepthStencilAttachment);
    }

    bool hasStencilComponent(vk::Format format)
    {
        return format == vk::Format::eD32SfloatS8Uint
            || format == vk::Format::eD24UnormS8Uint;
    }

    friend void framebufferResizeCallback(GLFWwindow*, int, int);
    friend void cursorPositionCallback(GLFWwindow*, double, double);

    void CreateImage(uint32_t width, uint32_t height, vk::Format format,
                      vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                      vk::MemoryPropertyFlags properties,
                      vk::UniqueImage& image,
                      vk::UniqueDeviceMemory& imageMemory);

    vk::UniqueImageView CreateImageView(vk::Image, vk::Format,
                                        vk::ImageAspectFlags);

    Camera m_camera;
};

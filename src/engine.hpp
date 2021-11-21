#pragma once
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#include <memory>
#include <optional>
#include <glm/glm.hpp>
#include <chrono>
#include <concepts>
#include "camera.hpp"
#include <spdlog/spdlog.h>
#include <TracyVulkan.hpp>
#include <vk_mem_alloc.h>
#include "allocated.hpp"

class Engine
{
private:
    struct FrameData {
        vk::UniqueCommandPool commandPool;
        vk::UniqueCommandBuffer commandBuffer;
        vk::UniqueFence renderFence;
        vk::UniqueSemaphore presentSemaphore, renderSemaphore;

        AllocatedBuffer sceneBuffer;
        vk::DescriptorSet globalDescriptor;
    };

public:
    void Init(GLFWwindow* window);
    void Terminate();
    void Resize() { m_framebufferResized = true; }

    static const std::vector<const char*> s_validationLayers;
    static const std::vector<const char*> s_deviceExtensions;

    vk::Instance GetInstance() { return *m_instance; }
    vk::PhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
    vk::Device GetDevice() { return *m_device; }
    vk::Queue GetGraphicsQueue() { return m_graphicsQueue; }
    std::size_t GetImageCount() const { return m_swapChainImages.size(); }
    vk::RenderPass GetRenderPass() { return *m_renderPass; }
    vk::Extent2D GetSwapChainExtent() { return m_swapChainExtent; }
    vk::DescriptorSetLayout GetGlobalSetLayout() const { return *m_globalSetLayout; }
    vk::DescriptorSetLayout GetTextureSetLayout() const { return *m_textureSetLayout; }
    TracyVkCtx GetCurrentTracyContext() { return m_tracyCtxs[m_currentFrame]; }
    unsigned GetCurrentFrame() const { return m_currentFrame; }
    unsigned GetCurrentImage() const { return m_currentImageIndex; }
    vk::DescriptorPool GetGlobalDescriptorPool() { return *m_descriptorPool; }
    vk::DescriptorSet GetCurrentGlobalSet() { return CurrentFrame().globalDescriptor; }
    unsigned GetMaxFramesInFlight() const { return m_max_frames_in_flight; }
    const std::vector<vk::UniqueImageView>& GetSwapChainImageViews()
    {
        return m_swapChainImageViews;
    }
    VmaAllocator GetVmaAllocator() { return m_vmaAllocator; }

    vk::UniqueShaderModule CreateShaderModule(const std::vector<uint32_t>&);

    void AddRecreateCallback(std::function<void(Engine&)> callback)
    {
        m_recreateCallbacks.push_back(callback);
    }

    template<std::invocable<vk::CommandBuffer&> T>
    void ImmediateSubmit(T func)
    {
        vk::CommandBufferAllocateInfo allocInfo;
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = 1;
        allocInfo.setCommandPool(*m_uploadCommandPool);

        auto commandBuffers = m_device->allocateCommandBuffersUnique(allocInfo);
        auto commandBuffer = *commandBuffers.front();

        vk::CommandBufferBeginInfo beginInfo;
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        commandBuffer.begin(beginInfo);

        func(commandBuffer);

        commandBuffer.end();

        vk::SubmitInfo submitInfo;
        submitInfo.setCommandBuffers(commandBuffer);

        m_graphicsQueue.submit(submitInfo, *m_uploadFence);

        auto r = m_device->waitForFences(*m_uploadFence, true, UINT64_MAX);
        m_device->resetFences(*m_uploadFence);
        m_device->resetCommandPool(*m_uploadCommandPool);
    }

    struct SceneData
    {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
        alignas(16) glm::mat4 viewInv;
        alignas(16) glm::mat4 projInv;
        alignas(16) glm::mat4 projview;
        alignas(8) glm::vec2 resolution;
        alignas(4) float time;
    };

    SceneData m_ubo;

    struct PushConstants
    {
        alignas(16) glm::mat4 model;
    };

    vk::CommandBuffer BeginFrame();
    void EndFrame();

    void BeginRenderPass(vk::CommandBuffer);
    void EndRenderPass(vk::CommandBuffer);

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

    void CreateImage(uint32_t width, uint32_t height, vk::Format format,
                      vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                      vk::MemoryPropertyFlags properties,
                      vk::UniqueImage& image,
                      vk::UniqueDeviceMemory& imageMemory);

    vk::UniqueImageView CreateImageView(vk::Image, vk::Format,
                                        vk::ImageAspectFlags);

    vk::UniquePipeline CreateWholeScreenPipeline(vk::ShaderModule vertexModule,
                                                 vk::ShaderModule fragmentModule,
                                                 vk::PipelineLayout);

    vk::UniquePipelineLayout CreatePushConstantsLayout(
        const vk::ArrayProxyNoTemporaries<const vk::PushConstantRange>
        &pushConstantRanges_) const;

    AllocatedBuffer CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                                 VmaMemoryUsage memoryUsage);
    void CopyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);

    template<typename T>
    AllocatedBuffer CopyToGPU(const std::vector<T>& data, vk::BufferUsageFlags flags)
    {
        auto data_size = data.size() * sizeof(data[0]);
        AllocatedBuffer result = CreateBuffer(
            data_size, flags | vk::BufferUsageFlagBits::eTransferDst,
            VMA_MEMORY_USAGE_GPU_ONLY);

        AllocatedBuffer stage = CreateBuffer(
            data_size, vk::BufferUsageFlagBits::eTransferSrc,
            VMA_MEMORY_USAGE_CPU_ONLY);

        void* data_ptr;
        vmaMapMemory(GetVmaAllocator(), stage.allocation, &data_ptr);
        memcpy(data_ptr, data.data(), data_size);
        vmaUnmapMemory(GetVmaAllocator(), stage.allocation);

        CopyBuffer(stage.buffer, result.buffer, data_size);

        return result;
    }


private:
    std::vector<const char*> GetRequiredExtensions();
    void CreateInstance();
    void SetupDebugMessenger();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSurface();
    void CreateSwapChain();
    void CreateImageViews();
    void CreateRenderPass();
    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateDepthResources();
    void CreateCommandBuffers();
    void RecreateCommandBuffer();
    void CreateSyncObjects();
    void CreateGlobalSetLayout();
    void CreateTextureSetLayout();
    void CreateUniformBuffers();
    void CreateDescriptorPool();
    void CreateDescriptorSets();
    void CreateVmaAllocator();

    void RecreateSwapChain()
    {
        int width = 0, height = 0;
        glfwGetFramebufferSize(m_window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(m_window, &width, &height);
            glfwWaitEvents();
        }

        m_device->waitIdle();

        CreateSwapChain();
        CreateImageViews();
        CreateRenderPass();
        CreateDepthResources();
        CreateFramebuffers();
        CreateUniformBuffers();
        CreateCommandPool();
        //CreateDescriptorPool();
        //CreateDescriptorSets();
        CreateCommandBuffers();

        for (auto& callback : m_recreateCallbacks)
        {
            callback(*this);
        }
    }

    void Loop();

    void WriteGlobalUniformBuffer();


    bool m_framebufferResized = false;

    GLFWwindow* m_window;

    //NOTE: declaration order affects destruction order.
    //Device should be destroyed last
    vk::UniqueInstance m_instance;
    vk::UniqueDevice m_device;

    std::vector<std::optional<vk::Fence>> m_imagesInFlight;

    vk::DispatchLoaderDynamic m_dispatcher;
    vk::UniqueHandle<vk::DebugUtilsMessengerEXT,
                     vk::DispatchLoaderDynamic> m_debugMessenger;
    vk::PhysicalDevice m_physicalDevice;
    vk::UniqueSurfaceKHR m_surface;
    vk::Queue m_graphicsQueue;
    vk::Queue m_presentQueue;
    vk::UniqueSwapchainKHR m_swapChain;

    static const unsigned m_max_frames_in_flight = 2;
    std::size_t m_currentFrame = 0;
    std::array<FrameData, m_max_frames_in_flight> m_frames;

    FrameData& CurrentFrame()
    {
        return m_frames[m_currentFrame];
    }

    uint32_t m_currentImageIndex;
    std::vector<vk::Image> m_swapChainImages;
    std::vector<vk::UniqueImageView> m_swapChainImageViews;
    std::vector<vk::UniqueFramebuffer> m_swapChainFramebuffers;

    vk::UniqueImage m_depthImage;
    vk::UniqueDeviceMemory m_depthImageMemory;
    vk::UniqueImageView m_depthImageView;
    vk::Format m_swapChainImageFormat;
    vk::Extent2D m_swapChainExtent;
    vk::UniqueRenderPass m_renderPass;
    vk::UniquePipelineLayout m_pipelineLayout;
    vk::UniqueDescriptorPool m_descriptorPool;
    vk::UniqueDescriptorSetLayout m_globalSetLayout;
    vk::UniqueDescriptorSetLayout m_textureSetLayout;
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

    void CreateTracyContexts();
    std::vector<TracyVkCtx> m_tracyCtxs;

    VmaAllocator m_vmaAllocator;

    std::vector<std::function<void(Engine&)>> m_recreateCallbacks;

    vk::UniqueCommandPool m_uploadCommandPool;
    vk::UniqueFence m_uploadFence;
};

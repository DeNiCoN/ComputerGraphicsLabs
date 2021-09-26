#pragma once
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#include <memory>
#include <optional>

static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

class App
{
public:
    void Run()
    {
        InitWindow();
        InitVulkan();
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
    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSyncObjects();
    void CreateVertexBuffer();

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
        CreateGraphicsPipeline();
        CreateFramebuffers();
        CreateCommandPool();
        CreateCommandBuffers();
    }
    vk::UniqueShaderModule CreateShaderModule(const std::vector<uint32_t>&);
    void Loop();
    void Terminate();

    void DrawFrame();

    unsigned m_width = 800;
    unsigned m_height = 600;
    const int m_max_frames_in_flight = 2;
    std::size_t m_currentFrame = 0;
    bool m_framebufferResized = false;

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
    vk::Format m_swapChainImageFormat;
    vk::Extent2D m_swapChainExtent;
    vk::UniqueRenderPass m_renderPass;
    vk::UniquePipelineLayout m_pipelineLayout;
    vk::UniquePipeline m_graphicsPipeline;
    std::vector<vk::UniqueFramebuffer> m_swapChainFramebuffers;
    vk::UniqueCommandPool m_commandPool;
    std::vector<vk::UniqueCommandBuffer> m_commandBuffers;
    vk::UniqueBuffer m_vertexBuffer;
    vk::UniqueDeviceMemory m_vertexBufferMemory;
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

    friend void framebufferResizeCallback(GLFWwindow*, int, int);
};

#pragma once
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#include <memory>

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
    void CreateGraphicsPipeline();
    vk::ShaderModule CreateShaderModule(const std::vector<uint32_t>&);
    void Loop();
    void Terminate();

    unsigned m_width = 800;
    unsigned m_height = 600;

    GLFWwindow* m_window;
    vk::Instance m_instance;
    vk::DispatchLoaderDynamic m_dispatcher;
    vk::DebugUtilsMessengerEXT m_debugMessenger;
    vk::PhysicalDevice m_physicalDevice;
    vk::Device m_device;
    vk::SurfaceKHR m_surface;
    vk::Queue m_graphicsQueue;
    vk::Queue m_presentQueue;
    vk::SwapchainKHR m_swapChain;
    std::vector<vk::Image> m_swapChainImages;
    std::vector<vk::ImageView> m_swapChainImageViews;
    vk::Format m_swapChainImageFormat;
    vk::Extent2D m_swapChainExtent;
#ifdef NDEBUG
    bool m_validationLayers = false;
#else
    bool m_validationLayers = true;
#endif

    bool IsDeviceSuitable(const vk::PhysicalDevice&);

    static vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>&);
    static vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR&, GLFWwindow*);
    static vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>&);
};

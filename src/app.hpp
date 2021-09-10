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
private:
    void InitWindow();
    std::vector<const char*> GetRequiredExtensions();
    void CreateInstance();
    void InitVulkan();
    void Loop();
    void Terminate();

    unsigned m_width = 800;
    unsigned m_height = 600;

    GLFWwindow* m_window;
    vk::Instance m_instance;
#ifdef NDEBUG
    bool m_validationLayers = false;
#else
    bool m_validationLayers = true;
#endif

    static const std::vector<const char*> s_validationLayers;
};

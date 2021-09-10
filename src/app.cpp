#include "app.hpp"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#include <spdlog/spdlog.h>

const std::vector<const char*> App::s_validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

void App::InitWindow()
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    if (!glfwInit())
    {
        spdlog::error("GLFW initialization failed");
        throw std::runtime_error("GLFW init failed");
    }

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
    vk::ApplicationInfo appInfo("Viewer", 1, "No Engine", 1, VK_API_VERSION_1_2);
    vk::InstanceCreateInfo createInfo;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = GetRequiredExtensions();
    createInfo.enabledExtensionCount = extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();

    m_instance = vk::createInstance(createInfo);
}

void App::InitVulkan()
{
    CreateInstance();
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

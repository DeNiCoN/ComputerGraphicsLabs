#pragma once
#include "engine.hpp"

class Editor
{
public:
    void Run()
    {
        Init();
        Loop();
        Terminate();
    }
private:
    Engine m_engine;
    GLFWwindow* m_window;

    void InitWindow();
    void InitWindowCallbacks();
    void InitCamera();
    void InitEngine();
    void InitImGui();
    void Init()
    {
        InitWindow();
        InitCamera();
        InitEngine();
        InitImGui();
    }
    void InitClock();
    void UpdateClock();
    void Update(float delta);
    void ImGuiFrame();
    void DrawFrame(float lag);
    void Loop();
    void Terminate();

    Camera m_camera;
    unsigned m_width = 800;
    unsigned m_height = 600;
    double m_old_xpos = 0;
    double m_old_ypos = 0;

    using TimePoint = std::chrono::high_resolution_clock::time_point;
    using Duration = std::chrono::high_resolution_clock::duration;
    TimePoint m_last_update;
    Duration m_desired_delta = std::chrono::duration_cast<Duration>(std::chrono::duration<double>(1.0/60.0));
    Duration m_lag = std::chrono::high_resolution_clock::duration::zero();
    TimePoint m_current_update;

    float AspectRatio() const { return static_cast<float>(m_width) / m_height; }
    bool ShouldClose() const { return glfwWindowShouldClose(m_window); }

    void OnResize(int width, int height);
    void OnMouseMove(double xpos, double ypos);

    vk::UniqueDescriptorPool m_imguiDescriptorPool;
};

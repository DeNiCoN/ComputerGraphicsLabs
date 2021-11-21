#pragma once
#include "engine.hpp"
#include "debug_pipelines.hpp"
#include "editor_object.hpp"
#include "object_entry.hpp"
#include "mesh_renderer.hpp"
#include <algorithm>
#include <ranges>
#include <numeric>

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
    void InitPipelines();
    void InitImGui();
    void InitDefaultObjects();
    void Init()
    {
        InitWindow();
        InitCamera();
        InitEngine();
        InitPipelines();
        InitImGui();

        InitDefaultObjects();
    }
    void InitClock();
    void UpdateClock();
    void Update(float delta);
    void ImGuiFrame();
    void ImGuiEditorObjects();
    void DrawFrame(float lag);
    void Loop();
    void Terminate();

    ObjectGroup m_objects;

    Camera m_camera;
    unsigned m_width = 800;
    unsigned m_height = 600;
    double m_old_xpos = 0;
    double m_old_ypos = 0;

    bool touch_x = false;
    bool touch_y = false;
    bool touch_z = false;

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
    DebugPipelines m_debug;

    MaterialManager m_material_manager {m_engine};
    MeshManager m_mesh_manager {m_engine};
    MeshRenderer m_mesh_renderer;
    TextureManager m_texture_manager { m_engine };

    std::vector<EditorObject::Ptr> m_rotate;
};

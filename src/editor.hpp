#pragma once
#include "engine.hpp"
#include "debug_pipelines.hpp"
#include "editor_object.hpp"

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


    class ObjectEntry
    {
    public:
        std::string name;
        EditorObject::Ptr object;
        bool is_enabled = true;
    };

    class ObjectGroup
    {
        using Objects = std::vector<ObjectEntry>;
    public:
        using iterator = Objects::iterator;
        glm::vec3 GetSelectedPosition();
        glm::vec3 GetSelectedBBox();

        std::size_t size() const { return m_objects.size(); }
        iterator begin() { return m_objects.begin(); }
        iterator end() { return m_objects.end(); }
        ObjectEntry& operator[](std::size_t i) { return m_objects[i]; }

        void Add(std::string_view name, const EditorObject::Ptr& ptr)
        {
            m_objects.push_back(ObjectEntry{std::string(name), ptr});
        }
        bool IsSelected(std::size_t i) const { return m_selected.contains(i); }
        bool Unselect(std::size_t i) { return m_selected.erase(i); }
        bool Select(std::size_t i) { return m_selected.insert(i).second; }
        void ClearSelected() { m_selected.clear(); }

    private:
        Objects m_objects;
        std::unordered_set<std::size_t> m_selected;
    };

    ObjectGroup m_objects;

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
    DebugPipelines m_debug;
};

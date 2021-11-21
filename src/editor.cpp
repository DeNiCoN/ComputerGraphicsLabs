#include "editor.hpp"
#include <glm/gtx/norm.hpp>
#include <spdlog/spdlog.h>
#include <imgui.h>
#include "bindings/imgui_impl_vulkan.h"
#include "bindings/imgui_impl_glfw.h"
#include <Tracy.hpp>
#include "grid_object.hpp"
#include "mesh_object.hpp"
#include <glm/gtx/closest_point.hpp>

void Editor::InitWindow()
{
    if (!glfwInit())
    {
        spdlog::error("GLFW initialization failed");
        throw std::runtime_error("GLFW init failed");
    }
    glfwSetErrorCallback([](int code, const char* description) {
        spdlog::error("GFLW: {}", description);
    });

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(m_width, m_height, "Vulkan", nullptr, nullptr);

    InitWindowCallbacks();
}

void Editor::InitCamera()
{
    m_camera.SetPerspective(m_camera.m_fov, AspectRatio(), 0.01, 100);

    m_camera.position = {0.f, 2.f, 5.f};
    m_camera.direction = {0.f, 0.f, -1.f};
}

void Editor::InitEngine()
{
    m_engine.Init(m_window);
}

void Editor::InitPipelines()
{
    m_debug.Init(m_engine);
    m_engine.AddRecreateCallback([&](Engine& engine) {m_debug.Recreate(engine);});
}

void Editor::OnResize(int width, int height)
{
    m_width = width;
    m_height = height;

    m_camera.SetViewport(width, height);
    m_engine.Resize();
}

void Editor::OnMouseMove(double xpos, double ypos)
{
    //TODO Camera update viewport
    //Compute delta
    //Check does it imgui?
    //Update camera

    double dx = xpos - m_old_xpos;
    double dy = ypos - m_old_ypos;

    m_old_xpos = xpos;
    m_old_ypos = ypos;

    if (ImGui::GetIO().WantCaptureMouse)
        return;

    int state = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT);
    if (state == GLFW_PRESS)
    {
        const glm::vec3 sideways = glm::normalize(glm::cross({0.f, 1.f, 0.f}, m_camera.direction));

        m_camera.direction = glm::rotate(static_cast<float>(-dx * 0.001f),
                                         glm::vec3{0, 1, 0}) * glm::vec4(m_camera.direction, 1.f);

        m_camera.direction = glm::rotate(static_cast<float>(dy * 0.001f),
                                         sideways) * glm::vec4(m_camera.direction, 1.f);
    }
}

void Editor::InitWindowCallbacks()
{
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int width, int height) {
        reinterpret_cast<Editor*>(glfwGetWindowUserPointer(window))->OnResize(width, height);
    });
    glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xpos, double ypos) {
        reinterpret_cast<Editor*>(glfwGetWindowUserPointer(window))->OnMouseMove(xpos, ypos);
    });
}

void Editor::InitClock()
{
    m_last_update = std::chrono::high_resolution_clock::now();
}

void Editor::UpdateClock()
{
    m_current_update = std::chrono::high_resolution_clock::now();
    m_lag += m_current_update - m_last_update;
    m_last_update = m_current_update;
}

void Editor::Update(float delta)
{
    ZoneScoped;
    int state = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT);
    if (state == GLFW_PRESS && !ImGui::GetIO().WantCaptureMouse)
    {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    else
    {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    glm::vec3 forward;
    if (m_camera.GetProjectionType() == Projection::PERSPECTIVE)
    {
        forward = glm::normalize(m_camera.direction);
    }
    else if (m_camera.GetProjectionType() == Projection::ORTHO)
    {
        forward = glm::normalize(
            glm::vec3(m_camera.direction.x, 0, m_camera.direction.z));
    }
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
    m_engine.m_ubo.view = m_camera.GetView();
    m_engine.m_ubo.proj = m_camera.GetProjection();
    m_engine.m_ubo.time += delta;

    auto extent = m_engine.GetSwapChainExtent();
    m_engine.m_ubo.resolution = glm::vec2(extent.width, extent.height);

    //Check which axis does cursor touch
    if (m_objects.SelectedSize())
    {
        m_objects.GetSelectedPosition();
    }
}

void Editor::InitImGui()
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

    m_imguiDescriptorPool = m_engine.GetDevice().createDescriptorPoolUnique(pool_info);


    // 2: initialize imgui library

    //this initializes the core structures of imgui
    ImGui::CreateContext();

    //this initializes imgui for SDL
    ImGui_ImplGlfw_InitForVulkan(m_window, true);

    //this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_engine.GetInstance();
    init_info.PhysicalDevice = m_engine.GetPhysicalDevice();
    init_info.Device = m_engine.GetDevice();
    init_info.Queue = m_engine.GetGraphicsQueue();
    init_info.DescriptorPool = *m_imguiDescriptorPool;
    init_info.MinImageCount = m_engine.GetImageCount();
    init_info.ImageCount = m_engine.GetImageCount();
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info, m_engine.GetRenderPass());

    //execute a gpu command to upload imgui font textures
    m_engine.ImmediateSubmit([&](VkCommandBuffer cmd) {
        ImGui_ImplVulkan_CreateFontsTexture(cmd);
    });

    //clear font textures from cpu data
    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void Editor::InitDefaultObjects()
{


    m_mesh_manager.NewFromObj("Soldier", Files::Local("res/models/PinkSoldier_sketch.obj"));
    m_mesh_manager.NewFromObj("Armor", Files::Local("res/models/armor 2021.obj"));
    m_mesh_manager.NewFromObj("Octahedron", Files::Local("res/models/octahedron.obj"));
    m_mesh_manager.NewFromObj("Frostbone", Files::Local("res/models/castle_01.obj"));

    m_texture_manager.NewFromFile(
        "Soldier_diffuse",
        Files::Local("res/textures/PinkSoldier_BaseColor_1001.png"));

    m_material_manager.FromShaders("Default",
                                   Files::Local("res/shaders/default.vert"),
                                   Files::Local("res/shaders/default.frag"));
    m_material_manager.FromShaders("Model_UV",
                                   Files::Local("res/shaders/default.vert"),
                                   Files::Local("res/shaders/model_uv.frag"));
    m_material_manager.FromShaders("Model_Normal",
                                   Files::Local("res/shaders/default.vert"),
                                   Files::Local("res/shaders/model_normal.frag"));

    auto armor = std::make_shared<MeshObject>(
        m_mesh_renderer,
        m_mesh_manager.Get("Armor"),
        m_material_manager.Get("Default"),
        m_texture_manager.NewTextureSet(m_texture_manager.Get("Soldier_diffuse")),
        m_material_manager);

    armor->position = {-4, 1, 5};
    armor->scale = 0.1;
    armor->yaw = 0.4;

    m_objects.Add("Armor", std::move(armor));

    auto soldier = std::make_shared<MeshObject>(
        m_mesh_renderer,
        m_mesh_manager.Get("Soldier"),
        m_material_manager.Get("Default"),
        m_texture_manager.NewTextureSet(m_texture_manager.Get("Soldier_diffuse")),
        m_material_manager);
    soldier->position = {4, 1, 5};
    soldier->scale = 0.03;

    m_objects.Add("Soldier", std::move(soldier));

    m_objects.Add("Octahedron", std::make_shared<MeshObject>(
                      m_mesh_renderer,
                      m_mesh_manager.Get("Octahedron"),
                      m_material_manager.Get("Default"),
                      m_texture_manager.NewTextureSet(m_texture_manager.Get("Soldier_diffuse")),
                      m_material_manager));

    m_objects.Add("Grid", std::make_shared<GridObject>(m_engine));

    auto frostbone = std::make_shared<MeshObject>(
        m_mesh_renderer,
        m_mesh_manager.Get("Frostbone"),
        m_material_manager.Get("Default"),
        m_texture_manager.NewTextureSet(m_texture_manager.Get("Soldier_diffuse")),
        m_material_manager);
    frostbone->position = {5, 0, -5};
    frostbone->scale = 0.1;

    m_objects.Add("Frostbone", std::move(frostbone));
}

void Editor::ImGuiFrame()
{
    ImGui_ImplGlfw_NewFrame();
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
    ImGui::ShowDemoWindow();

    ImGuiEditorObjects();

    ImGui::Begin("Shaders");
    ImGui::Text("Average %.3f ms/frame (%.1f FPS)",
                1000.0f / ImGui::GetIO().Framerate,
                ImGui::GetIO().Framerate);
    if (ImGui::Button("Recompile Shaders"))
    {
        m_engine.GetDevice().waitIdle();
        try
        {
            m_debug.Recreate(m_engine);
            m_material_manager.Recreate();
            for (auto entry : m_objects)
            {
                entry.object->Recreate(m_engine);
            }
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
        if (ImGui::Selectable(Projection::ToString(Projection::ORTHO)))
        {
            m_camera.SetOrtho(m_width/100, m_height/100, 100);
        }

        if (ImGui::Selectable(Projection::ToString(Projection::PERSPECTIVE)))
        {
            m_camera.SetPerspective(m_camera.m_fov, AspectRatio(), 0.01, 100);
        }

        ImGui::EndCombo();
    }

    if (m_camera.GetProjectionType() == Projection::PERSPECTIVE)
    {
        if (ImGui::SliderFloat("FOV", &m_camera.m_fov, 0.01, std::numbers::pi - 0.2))
        {
            m_camera.SetPerspective(m_camera.m_fov, AspectRatio(), 0.01, 100);
        }
    }
    ImGui::End();
}

void Editor::ImGuiEditorObjects()
{
    if (ImGui::Begin("Objects"))
    {
        for (int i = 0; i < m_objects.size(); i++)
        {
            ImGuiTreeNodeFlags node_flags = 0;
            bool is_selected = m_objects.IsSelected(i);
            if (is_selected)
                node_flags |= ImGuiTreeNodeFlags_Selected;

            bool node_open = ImGui::TreeNodeEx(m_objects[i].name.c_str(),
                                               node_flags);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)
                && !ImGui::IsItemToggledOpen())
            {
                m_objects.ClearSelected();
                if (!is_selected)
                    m_objects.Select(i);
            }

            if (node_open)
            {
                if (ImGui::Checkbox("Visible", &m_objects[i].is_enabled))
                {
                }
                m_objects[i].object->ImGuiOptions();
                ImGui::TreePop();
            }
        }
    }
    ImGui::End();
}

void Editor::Loop()
{
    using namespace std::chrono;
    InitClock();

    while (!ShouldClose()) {
        glfwPollEvents();

        UpdateClock();

        ImGuiFrame();

        while (m_lag > m_desired_delta)
        {
            m_lag -= m_desired_delta;
            Update(duration<float>(m_desired_delta).count());
        }

        DrawFrame(duration<float>(m_lag).count() /
                  duration<float>(m_desired_delta).count());
        FrameMark;
    }
}

void Editor::DrawFrame(float lag)
{
    m_debug.Begin();
    if (m_objects.SelectedSize())
    {
        auto bbox = m_objects.GetSelectedBBox();
        auto pos = m_objects.GetSelectedPosition();
        m_debug.DrawLine(pos, pos + glm::vec3{1, 0.0, 0},
                         {1.f, 0, 0, touch_x ? 1.0 : 0.5f}, 5);
        m_debug.DrawLine(pos, pos + glm::vec3{0, 1.0, 0},
                         {0, 1.f, 0, touch_y ? 1.0 : 0.5f}, 5);
        m_debug.DrawLine(pos, pos + glm::vec3{0, 0.0, 1},
                         {0, 0, 1.f, touch_z ? 1.0 : 0.5f}, 5);
        m_debug.DrawBox(pos, bbox, {1.f, 1.f, 0, 0.8f});
    }
    m_debug.End(m_engine);

    m_mesh_renderer.Begin();
    for (auto& obj : m_objects)
    {
        if (obj.is_enabled)
            obj.object->Render(lag);
    }
    m_mesh_renderer.End();

    ImGui::Render();
    //m_engine.DrawFrame(lag);
    auto cmd = m_engine.BeginFrame();
    m_engine.BeginRenderPass(cmd);

    m_mesh_renderer.WriteCmdBuffer(cmd, m_engine);

    for (auto& entry : m_objects)
    {
        if (entry.is_enabled)
            entry.object->Draw(cmd, m_engine);
    }

    m_debug.WriteCmdBuffer(cmd, m_engine);

    m_engine.EndRenderPass(cmd);
    m_engine.EndFrame();
}

void Editor::Terminate()
{
    m_engine.Terminate();
    ImGui_ImplVulkan_Shutdown();
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

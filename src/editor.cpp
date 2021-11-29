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
#include <algorithm>

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
    m_material_manager.Recreate();
}

void Editor::OnMouseMove(double xpos, double ypos)
{
    double dx = xpos - m_old_xpos;
    double dy = ypos - m_old_ypos;

    m_old_xpos = xpos;
    m_old_ypos = ypos;

    if (ImGui::GetIO().WantCaptureMouse)
        return;

    int state = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT);
    if (state == GLFW_PRESS)
    {
        m_orbiting_camera.yaw += static_cast<float>(-dx * 0.001f);
        m_orbiting_camera.pitch += static_cast<float>(-dy * 0.001f);
    }

    m_orbiting_camera.Update();
}

void Editor::OnMouseScroll(double xoffset, double yoffset)
{
    m_orbiting_camera.radius -= yoffset * 0.1f * m_orbiting_camera.radius;
    m_orbiting_camera.radius = std::max(0.1f, m_orbiting_camera.radius);
    m_orbiting_camera.Update();
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
    glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xoffset, double yoffset) {
        reinterpret_cast<Editor*>(glfwGetWindowUserPointer(window))->OnMouseScroll(xoffset, yoffset);
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

    for (auto& orbit : m_orbit)
    {
        orbit.angle += 3 * delta / orbit.radius;
        orbit.objAngle += 1 * delta;
        orbit.object->position = {orbit.radius, 0, 0};
        orbit.object->position =
            glm::rotate(glm::angleAxis(orbit.angle, orbit.axis),
                        orbit.object->position);

        auto angles = glm::eulerAngles(glm::angleAxis(orbit.objAngle, orbit.objAxis));
        orbit.object->yaw = angles.x;
        orbit.object->pitch = angles.y;
        orbit.object->roll = angles.z;
    }

    if (focused.has_value())
    {
        auto focusedPtr = m_objects[focused.value()].object;
        m_orbiting_camera.center = focusedPtr->position;
        if (m_orbiting_camera.radius < 1.5)
        {
            auto translate = glm::translate(-focusedPtr->position);
            auto translateInv = glm::translate(focusedPtr->position);
            m_camera.transform =
                translateInv * glm::yawPitchRoll(
                    focusedPtr->yaw, focusedPtr->pitch, focusedPtr->roll)
                * translate;
        }
        else
        {
            m_camera.transform = glm::identity<glm::mat4>();
        }
        m_orbiting_camera.Update();

    }
    else
    {
        m_camera.transform = glm::identity<glm::mat4>();
    }

    m_engine.m_ubo.view = m_camera.GetView();
    m_engine.m_ubo.proj = m_camera.GetProjection();
    m_engine.m_ubo.time += delta;
    m_engine.m_ubo.viewPos = m_camera.position;

    auto extent = m_engine.GetSwapChainExtent();
    m_engine.m_ubo.resolution = glm::vec2(extent.width, extent.height);

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
    m_material_manager.FromShaders("Default",
                                   Files::Local("res/shaders/default.vert"),
                                   Files::Local("res/shaders/default.frag"));
    m_material_manager.FromShaders("Model_UV",
                                   Files::Local("res/shaders/default.vert"),
                                   Files::Local("res/shaders/model_uv.frag"));
    m_material_manager.FromShaders("Model_Normal",
                                   Files::Local("res/shaders/default.vert"),
                                   Files::Local("res/shaders/model_normal.frag"));
    m_material_manager.Textureless("White_Bloom",
                                  Files::Local("res/shaders/default.vert"),
                                  Files::Local("res/shaders/white_bloom.frag"));


    m_mesh_manager.NewFromObj("flintlock", Files::Local("res/models/fa_flintlockPistol.obj"));
    m_mesh_manager.NewFromObj("pot", Files::Local("res/models/Pot.obj"));
    m_mesh_manager.NewFromObj("cherry", Files::Local("res/models/cherry.obj"));
    m_mesh_manager.NewFromObj("paper", Files::Local("res/models/br_tpaperRoll.obj"));
    m_mesh_manager.NewFromObj("orange", Files::Local("res/models/fr_caraOrange.obj"));
    m_mesh_manager.NewFromObj("lemon", Files::Local("res/models/fr_avalonLemon.obj"));
    m_mesh_manager.NewFromObj("sun", Files::Local("res/models/sun.obj"));

    m_texture_manager.NewFromFile("paper_ao", Files::Local("res/textures/br_tpaperRoll_ao.jpg"));
    m_texture_manager.NewFromFile("paper_nrm", Files::Local("res/textures/br_tpaperRoll_nrm.jpg"));
    m_texture_manager.NewFromFile("paper_rough", Files::Local("res/textures/br_tpaperRoll_rough.jpg"));
    m_texture_manager.NewFromFile("paper_specular", Files::Local("res/textures/br_tpaperRoll_specular.jpg"));
    m_texture_manager.NewFromFile("paper_albedo", Files::Local("res/textures/br_tpaperRoll_albedo.jpg"));
    m_texture_manager.NewFromFile("paper_scattering", Files::Local("res/textures/br_tpaperRoll_scattering.jpg"));

    m_texture_manager.NewFromFile("flintlock_ao", Files::Local("res/textures/fa_flintlockPistol_ao.jpg"));
    m_texture_manager.NewFromFile("flintlock_nrm", Files::Local("res/textures/fa_flintlockPistol_nrm.jpg"));
    m_texture_manager.NewFromFile("flintlock_rough", Files::Local("res/textures/fa_flintlockPistol_rough.jpg"));
    m_texture_manager.NewFromFile("flintlock_specular", Files::Local("res/textures/fa_flintlockPistol_specular.jpg"));
    m_texture_manager.NewFromFile("flintlock_albedo", Files::Local("res/textures/fa_flintlockPistol_albedo.jpg"));

    m_texture_manager.NewFromFile("lemon_nrm", Files::Local("res/textures/fr_avalonLemon_nrm.jpg"));
    m_texture_manager.NewFromFile("lemon_rough", Files::Local("res/textures/fr_avalonLemon_rough.jpg"));
    m_texture_manager.NewFromFile("lemon_specular", Files::Local("res/textures/fr_avalonLemon_specular.jpg"));
    m_texture_manager.NewFromFile("lemon_albedo", Files::Local("res/textures/fr_avalonLemon_albedo.jpg"));

    m_texture_manager.NewFromFile("orange_nrm", Files::Local("res/textures/fr_caraOrange_nrm.jpg"));
    m_texture_manager.NewFromFile("orange_rough", Files::Local("res/textures/fr_caraOrange_rough.jpg"));
    m_texture_manager.NewFromFile("orange_specular", Files::Local("res/textures/fr_caraOrange_specular.jpg"));
    m_texture_manager.NewFromFile("orange_albedo", Files::Local("res/textures/fr_caraOrange_albedo.jpg"));
    m_texture_manager.NewFromFile("orange_scattering", Files::Local("res/textures/fr_caraOrange_scattering.jpg"));

    m_texture_manager.NewFromFile("pot_specular", Files::Local("res/textures/pot_specular.jpg"));
    m_texture_manager.NewFromFile("pot_normal", Files::Local("res/textures/pot_normal.jpg"));
    m_texture_manager.NewFromFile("pot_gloss", Files::Local("res/textures/pot_gloss.jpg"));
    m_texture_manager.NewFromFile("pot_albedo", Files::Local("res/textures/pot_albedo.jpg"));

    m_texture_manager.NewFromFile("cherry_specular", Files::Local("res/textures/cherry_specular.tga.png"));
    m_texture_manager.NewFromFile("cherry_normal", Files::Local("res/textures/cherry_normal.tga.png"));
    m_texture_manager.NewFromFile("cherry_gloss", Files::Local("res/textures/cherry_gloss.tga.png"));
    m_texture_manager.NewFromFile("cherry_color", Files::Local("res/textures/cherry_color.tga.png"));
    m_texture_manager.NewFromFile("cherry_ao", Files::Local("res/textures/cherry_ao.tga.png"));

    m_texture_manager.NewFromFile("sun_color", Files::Local("res/textures/sun.jpg"));

    auto paper = std::make_shared<MeshObject>(
        m_mesh_renderer,
        m_mesh_manager.Get("paper"),
        m_material_manager.Get("Default"),
        m_texture_manager.NewTextureSet(
            m_texture_manager.Get("paper_albedo"),
            m_texture_manager.Get("paper_nrm"),
            m_texture_manager.Get("paper_specular"),
            m_texture_manager.Get("paper_rough"),
            m_texture_manager.Get("paper_ao")
            ),
        m_material_manager);
    paper->mesh_center = {0, 0.05, 0};
    paper->scale = 10;

    m_objects.Add("Paper", paper);
    m_orbit.push_back({
            .object = paper,
            .center = {0, 0, 0},
            .radius = 4,
            .axis = {glm::normalize(glm::vec3(0, 1, 0.1))},
            .objAxis = {glm::normalize(glm::vec3(0, 1, 2.5))}
        });

    auto sun = std::make_shared<MeshObject>(
        m_mesh_renderer,
        m_mesh_manager.Get("sun"),
        m_material_manager.Get("White_Bloom"),
        m_texture_manager.NewTextureSet(
            m_texture_manager.Get("sun_color"),
            nullptr,
            nullptr,
            nullptr,
            nullptr
            ),
        m_material_manager);
    sun->mesh_center = {1, 1, 1};
    m_objects.Add("Sun", sun);

    auto flintlock = std::make_shared<MeshObject>(
        m_mesh_renderer,
        m_mesh_manager.Get("flintlock"),
        m_material_manager.Get("Default"),
        m_texture_manager.NewTextureSet(
            m_texture_manager.Get("flintlock_albedo"),
            m_texture_manager.Get("flintlock_nrm"),
            m_texture_manager.Get("flintlock_specular"),
            m_texture_manager.Get("flintlock_rough"),
            m_texture_manager.Get("flintlock_ao")
            ),
        m_material_manager);
    flintlock->scale = 10;
    flintlock->mesh_center = {0, 0.01, 0};
    m_objects.Add("Flintlock", flintlock);
    m_orbit.push_back({
            .object = flintlock,
            .center = {0, 0, 0},
            .radius = 8,
            .axis = {glm::normalize(glm::vec3(0.1, 1, 0.1))},
            .objAxis = {glm::normalize(glm::vec3(2, 1, 2.5))}
        });

    auto lemon = std::make_shared<MeshObject>(
        m_mesh_renderer,
        m_mesh_manager.Get("lemon"),
        m_material_manager.Get("Default"),
        m_texture_manager.NewTextureSet(
            m_texture_manager.Get("lemon_albedo"),
            m_texture_manager.Get("lemon_nrm"),
            m_texture_manager.Get("lemon_specular"),
            m_texture_manager.Get("lemon_rough"),
            nullptr
            ),
        m_material_manager);
    lemon->scale = 10;
    lemon->mesh_center = {0, 0.025, 0};
    m_objects.Add("Lemon", lemon);
    m_orbit.push_back({
            .object = lemon,
            .center = {0, 0, 0},
            .radius = 12,
            .axis = {glm::normalize(glm::vec3(0.2, 1, 0.0))},
            .objAxis = {glm::normalize(glm::vec3(2, 1, 0))}
        });

    auto orange = std::make_shared<MeshObject>(
        m_mesh_renderer,
        m_mesh_manager.Get("orange"),
        m_material_manager.Get("Default"),
        m_texture_manager.NewTextureSet(
            m_texture_manager.Get("orange_albedo"),
            m_texture_manager.Get("orange_nrm"),
            m_texture_manager.Get("orange_specular"),
            m_texture_manager.Get("orange_rough"),
            nullptr
            ),
        m_material_manager);
    orange->scale = 10;
    orange->mesh_center = {0, 0.03, 0};
    m_objects.Add("Orange", orange);
    m_orbit.push_back({
            .object = orange,
            .center = {0, 0, 0},
            .radius = 16,
            .axis = {glm::normalize(glm::vec3(0.05, 1, 0.1))},
            .objAxis = {glm::normalize(glm::vec3(2, 1, 0.4))}
        });

    auto pot = std::make_shared<MeshObject>(
        m_mesh_renderer,
        m_mesh_manager.Get("pot"),
        m_material_manager.Get("Default"),
        m_texture_manager.NewTextureSet(
            m_texture_manager.Get("pot_albedo"),
            m_texture_manager.Get("pot_normal"),
            m_texture_manager.Get("pot_specular"),
            m_texture_manager.Get("pot_gloss"),
            nullptr
            ),
        m_material_manager);
    pot->scale = 0.001;
    pot->mesh_center = {0, 10, 0};

    m_objects.Add("Pot", pot);
    m_orbit.push_back({
            .object = pot,
            .center = {0, 0, 0},
            .radius = 20,
            .axis = {glm::normalize(glm::vec3(0.05, 1, 0.4))},
            .objAxis = {glm::normalize(glm::vec3(4, 1, 0.4))}
        });

    auto cherry = std::make_shared<MeshObject>(
        m_mesh_renderer,
        m_mesh_manager.Get("cherry"),
        m_material_manager.Get("Default"),
        m_texture_manager.NewTextureSet(
            m_texture_manager.Get("cherry_color"),
            m_texture_manager.Get("cherry_normal"),
            m_texture_manager.Get("cherry_specular"),
            m_texture_manager.Get("cherry_gloss"),
            m_texture_manager.Get("cherry_ao")
            ),
        m_material_manager);
    cherry->scale = 0.001;
    cherry->mesh_center = {300, 300, 300};
    m_orbit.push_back({
            .object = cherry,
            .center = {0, 0, 0},
            .radius = 24,
            .axis = {glm::normalize(glm::vec3(0.05, 1, 0.01))},
            .objAxis = {glm::normalize(glm::vec3(0.1, 1, 0.2))}
        });
    m_objects.Add("Cherry", cherry);

    m_objects.Add("Grid", std::make_shared<GridObject>(m_engine));
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
    ImGui::Text("Center %.3f %.3f %.3f",
        m_orbiting_camera.center.x,
        m_orbiting_camera.center.y,
        m_orbiting_camera.center.z);

    ImGui::Text("Radius %.3f",
        m_orbiting_camera.radius);

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
        m_orbiting_camera.Update();

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

                ImGui::SameLine();

                if (ImGui::Button("Focus"))
                {
                    if (focused == i)
                    {
                        focused = std::nullopt;
                    }
                    else
                    {
                        focused = i;
                    }
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

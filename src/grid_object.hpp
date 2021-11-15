#include "editor_object.hpp"
#include "engine.hpp"
#include "shader_compiler.hpp"
#include "files.hpp"
#include "imgui.h"

class GridObject : public EditorObject
{
public:
    explicit GridObject(Engine& engine) : EditorObject(false)
    {
        GridObject::Recreate(engine);
        engine.AddRecreateCallback([&] (Engine& engine) { GridObject::Recreate(engine); });
    }

    struct PushConstants {
        alignas(16) glm::vec3 xcolor = {1.0, 0.2, 0.2};
        alignas(16) glm::vec3 zcolor = {0.2, 0.2, 1.0};
        alignas(16) glm::vec3 grid_color = {0.2, 0.2, 0.2};
    } push_constants;

    void Recreate(Engine& engine) override final
    {
        CreatePipeline(engine);
    }

    void Draw(vk::CommandBuffer cmd, Engine& engine) override
    {
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               *m_pipelineLayout, 0, engine.GetCurrentGlobalSet(),
                               nullptr);

        cmd.pushConstants(*m_pipelineLayout, m_pushConstantsStageFlags, 0, sizeof(push_constants), &push_constants);
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_pipeline);
        {
            TracyVkZone(engine.GetCurrentTracyContext(), cmd, "Grid");
            cmd.draw(3, 1, 0, 0);
        }
    }

    void ImGuiOptions() override
    {
        ImGui::ColorPicker3("X axis", &push_constants.xcolor[0]);
        ImGui::ColorPicker3("Z axis", &push_constants.zcolor[0]);
        ImGui::ColorPicker3("Grid", &push_constants.grid_color[0]);
    }


private:
    void CreatePipeline(Engine& engine)
    {
        auto whole = engine.CreateShaderModule(
            ShaderCompiler::CompileFromFile(
                Files::Local("res/shaders/whole.vert"),
                shaderc_shader_kind::shaderc_glsl_vertex_shader));

        auto grid = engine.CreateShaderModule(
            ShaderCompiler::CompileFromFile(
                Files::Local("res/shaders/grid.frag"),
                shaderc_shader_kind::shaderc_glsl_fragment_shader));


        vk::PushConstantRange range(
            m_pushConstantsStageFlags,
            0, sizeof(PushConstants)
            );

        m_pipelineLayout = engine.CreatePushConstantsLayout(range);
        m_pipeline = engine.CreateWholeScreenPipeline(*whole, *grid, *m_pipelineLayout);
    }

    vk::UniquePipeline m_pipeline;
    vk::UniquePipelineLayout m_pipelineLayout;
    vk::ShaderStageFlags m_pushConstantsStageFlags = vk::ShaderStageFlagBits::eFragment;
};

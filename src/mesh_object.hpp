#pragma once
#include "mesh_renderer.hpp"
#include "editor_object.hpp"
#include <glm/gtx/euler_angles.hpp>
#include <imgui.h>

class MeshObject : public EditorObject
{
public:
    MeshObject(MeshRenderer& renderer, Mesh::Ptr mesh,
               Material::Ptr material, TextureSet::Ptr textures,
               MaterialManager& matMan)
        : EditorObject(true), m_renderer(renderer), m_mesh(mesh),
          m_material(material), m_textures(textures), m_materialManager(matMan)
    {}

    void Render(float lag) override
    {
        glm::mat4 model = glm::identity<glm::mat4>();

        model = glm::yawPitchRoll(yaw, pitch, roll) * model;
        model = glm::translate(model, position);
        model = glm::scale(model, {scale, scale, scale});
        m_renderer.Add(m_mesh, m_material, m_textures, model);
    }

    void ImGuiOptions() override
    {
        const auto& currentName = m_materialManager.GetName(m_material);
        if (ImGui::BeginCombo("Material", currentName.c_str()))
        {
            for (const auto& name : m_materialManager.GetNames())
            {
                if (ImGui::Selectable(name.c_str()))
                {
                    m_material = m_materialManager.Get(name);
                }
            }
            ImGui::EndCombo();
        }
    };

private:
    MeshRenderer& m_renderer;
    Mesh::Ptr m_mesh;
    Material::Ptr m_material;
    TextureSet::Ptr m_textures;
    MaterialManager& m_materialManager;
};

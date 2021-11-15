#pragma once
#include "mesh_renderer.hpp"
#include "editor_object.hpp"
#include <glm/gtx/euler_angles.hpp>

class MeshObject : public EditorObject
{
public:
    MeshObject(MeshRenderer& renderer, Mesh::Ptr mesh,
               Material::Ptr material)
        : EditorObject(true), m_renderer(renderer), m_mesh(mesh),
          m_material(material)
    {}

    void Render(float lag) override
    {
        glm::mat4 model = glm::identity<glm::mat4>();

        model = glm::yawPitchRoll(yaw, pitch, roll) * model;
        model = glm::translate(model, position);
        model = glm::scale(model, {scale, scale, scale});
        m_renderer.Add(m_mesh, m_material, model);
    }

private:
    MeshRenderer& m_renderer;
    Mesh::Ptr m_mesh;
    Material::Ptr m_material;
};

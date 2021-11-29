#pragma once
#include "camera.hpp"
#include <glm/gtx/euler_angles.hpp>


class OrbitingCamera
{
public:
    explicit OrbitingCamera(Camera& camera)
        : m_camera(camera)
    {

    }

    float yaw = 0;
    float pitch = 0;
    float roll = 0;
    float radius = 2;
    glm::vec3 center = {0, 0, 0};

    void Update()
    {
        glm::vec3 direction = {0, 0, -1};
        direction = (glm::yawPitchRoll(yaw, pitch, roll) * glm::vec4(direction, 0));

        m_camera.direction = direction;
        m_camera.position = center - direction * radius;

        if (m_camera.GetProjectionType() == Projection::ORTHO)
        {
            m_camera.scale = 1.f / (radius / 10);
        }
        else
        {
            m_camera.scale = 1.f;
        }
    }
private:

    Camera& m_camera;
};

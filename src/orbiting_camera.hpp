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
    float radius = 1;
    glm::vec3 center = {0, 0, 0};

    void Update()
    {
        glm::vec3 direction = {0, 0, -1};
        direction = (glm::eulerAngleYX(yaw, pitch) * glm::vec4(direction, 0));

        m_camera.direction = direction;
        m_camera.position = center - direction * radius;
    }
private:

    Camera& m_camera;
};

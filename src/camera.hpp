#pragma once
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <array>
#include <string_view>
#include <algorithm>
#include <ranges>

namespace Projection
{
enum Projection
{
    ORTHO, PERSPECTIVE
};

const std::array projectionStrings
{
    "Ortho", "Perspective"
};

inline const char* ToString(Projection proj)
{
    std::size_t value = static_cast<std::size_t>(proj);
    assert(value < projectionStrings.size());
    return projectionStrings[value];
}

inline Projection FromString(std::string_view sv)
{
    auto it = std::ranges::find(projectionStrings, sv);
    assert(it != projectionStrings.end());
    return static_cast<Projection>(
        std::distance(projectionStrings.begin(), it));
}

}

class Camera
{
public:
    const glm::mat4& GetProjection() const { return m_proj; }
    glm::mat4 GetView() const
    {
        auto look = glm::lookAt(position, position + direction,
                           glm::vec3(0.f, 1.f, 0.f));

        //Inverting y axis, so it points up
        return glm::scale(look, {scale, scale, scale});
    }

    glm::vec3 direction = {0.f, 0.f, 1.f};
    glm::vec3 position = {0.f, 0.f, 0.f};
    float scale = 1.f;

    void SetOrtho(float width, float height, unsigned depth)
    {
        m_proj = glm::ortho(-width/2.f, width/2.f,
                            -height/2.f, height/2.f,
                            0.f, depth/1.f);
        m_proj[1][1] = -m_proj[1][1];
        m_projectionType = Projection::ORTHO;
    }

    void SetPerspective(float fov, float aspect, float near, float far)
    {
        m_proj = glm::perspective(fov, aspect, near, far);
        m_proj[1][1] = -m_proj[1][1];
        m_projectionType = Projection::PERSPECTIVE;
    }

    Projection::Projection GetProjectionType() const
    {
        return m_projectionType;
    }

    void SetViewport(int width, int height)
    {
        if (GetProjectionType() == Projection::PERSPECTIVE)
        {
            SetPerspective(m_fov, width / static_cast<float>(height),
                           0.01f, 100.f);
        }
        else if (GetProjectionType() == Projection::ORTHO)
        {
            SetOrtho(width/100.f, height/100.f, 100.f);
        }

    }

    float m_fov = 2.f;
private:
    Projection::Projection m_projectionType = Projection::ORTHO;
    glm::mat4 m_proj = glm::identity<glm::mat4>();
};

#pragma once
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <numbers>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription GetBindingDescription() {
        vk::VertexInputBindingDescription bindingDescription;
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = vk::VertexInputRate::eVertex;
        return bindingDescription;
    }

    static std::array<vk::VertexInputAttributeDescription, 2>
    GetAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions;
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }

    static void FillVertexArray(float R, float r, int p, int q)
    {

        auto f = [R, r, p, q](int i, int j) -> float
        {
            return ( ( R + r * cos( (-1 + 2*(float)j/q) * std::numbers::pi ) ) *
                     cos( (-1 + 2*(float)i/p) * std::numbers::pi ) );
        };

        auto g = [R, r, p, q](int i, int j) -> float
        {
            return ( ( R + r * cos( (-1 + 2*(float)j/q) * std::numbers::pi ) ) *
                     sin( (-1 + 2*(float)i/p) * std::numbers::pi ) );
        };

        auto h = [R, r, p, q](int i, int j) -> float
        {
            return ( r * sin( (-1 + 2*(float)j/q) * std::numbers::pi ) );
        };

        std::vector<Vertex> result;
        for (int j = 0; j <= q; j++)
        {
            for (int i = 0; i <= p; i++)
            {
                result.push_back({{f(i, j), g(i, j), h(i, j)},
                                  {1.f, 0.f, 0.f}});
            }
        }
    }


};

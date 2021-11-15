#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <unordered_set>
#include <vulkan/vulkan.hpp>

class Engine;
class EditorObject
{
public:
    explicit EditorObject(bool is_movable)
        : is_movable(is_movable)
    {}

    virtual ~EditorObject() = default;

    glm::vec3 position = {0, 0, 0};
    glm::vec3 bbox = {1, 1, 1};
    float yaw = 0;
    float pitch = 0;
    float roll = 0;
    float scale = 1;
    bool is_movable;

    virtual void ImGuiOptions() {};
    virtual void Recreate(Engine&) {};
    virtual void Draw(vk::CommandBuffer cmd, Engine&) {};
    virtual void Render(float lag) {};
    virtual void Update(float delta) {};
    using Ptr = std::shared_ptr<EditorObject>;
};


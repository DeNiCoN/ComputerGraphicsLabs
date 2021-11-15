#pragma once
#include "editor_object.hpp"

class ObjectEntry
{
public:
    std::string name;
    EditorObject::Ptr object;
    bool is_enabled = true;
};

class ObjectGroup
{
    using Objects = std::vector<ObjectEntry>;
public:
    using iterator = Objects::iterator;
    glm::vec3 GetSelectedPosition()
    {
        glm::vec3 sum {0, 0, 0};
        for (const auto& id : m_selected)
        {
            sum += m_objects[id].object->position;
        }
        sum /= m_selected.size();
        return sum;
    }

    glm::vec3 GetSelectedBBox()
    {
        glm::vec3 max {0, 0, 0};
        for (const auto& id : m_selected)
        {
            max = {
                std::max(max.x, m_objects[id].object->bbox.x),
                std::max(max.y, m_objects[id].object->bbox.y),
                std::max(max.z, m_objects[id].object->bbox.z)
            };
        }
        return max;
    }

    std::size_t size() const { return m_objects.size(); }
    iterator begin() { return m_objects.begin(); }
    iterator end() { return m_objects.end(); }
    ObjectEntry& operator[](std::size_t i) { return m_objects[i]; }

    void Add(std::string_view name, const EditorObject::Ptr& ptr)
    {
        m_objects.push_back(ObjectEntry{std::string(name), ptr});
    }
    bool IsSelected(std::size_t i) const { return m_selected.contains(i); }
    bool Unselect(std::size_t i) { return m_selected.erase(i); }
    bool Select(std::size_t i) { return m_selected.insert(i).second; }
    std::size_t SelectedSize() const { return m_selected.size(); }
    void ClearSelected() { m_selected.clear(); }

private:
    Objects m_objects;
    std::unordered_set<std::size_t> m_selected;
};

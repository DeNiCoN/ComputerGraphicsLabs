#pragma once
#include "engine.hpp"
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>
#include <ranges>

struct PushConstants
{
    alignas(16) glm::mat4 model;
};

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 textureCoord;

    static std::array<vk::VertexInputAttributeDescription, 4>
    AttributeDescriptions();

    static vk::VertexInputBindingDescription BindingDescription();
};

struct Texture
{
    using Ptr = std::shared_ptr<Texture>;
    AllocatedImage image;
    vk::UniqueImageView imageView;
    vk::UniqueSampler sampler;
};

struct TextureSet
{
    using Ptr = std::shared_ptr<TextureSet>;
    Texture::Ptr diffuse;
    vk::DescriptorSet descriptor;
};

class TextureManager
{
public:
    explicit TextureManager(Engine& engine)
        : m_engine(engine)
    {}

    Texture::Ptr NewFromFile(const std::string& name,
                             const std::filesystem::path& filename);

    TextureSet::Ptr NewTextureSet(Texture::Ptr diffuse);

    Texture::Ptr Get(const std::string& name) const
    {
        return m_textures.at(name);
    }
private:
    std::unordered_map<std::string, Texture::Ptr> m_textures;
    Engine& m_engine;
};

struct Mesh
{
    using Ptr = std::shared_ptr<Mesh>;
    AllocatedBuffer vertexBuffer;
    AllocatedBuffer indexBuffer;
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
};

class MeshManager
{
public:
    explicit MeshManager(Engine& engine)
        : m_engine(engine)
    {}

    Mesh::Ptr NewFromObj(const std::string& name, const std::filesystem::path& filename);
    Mesh::Ptr NewFromVertices(const std::string& name,
                              std::vector<Vertex>, std::vector<uint32_t>);

    Mesh::Ptr Get(const std::string& name) const
    {
        return m_meshes.at(name);
    }
private:
    std::unordered_map<std::string, Mesh::Ptr> m_meshes;
    Engine& m_engine;
};

struct Material
{
    using Ptr = std::shared_ptr<Material>;
    vk::UniquePipelineLayout pipelineLayout;
    vk::UniquePipeline pipeline;
};

class MaterialManager
{
public:
    explicit MaterialManager(Engine& engine)
        : m_engine(engine)
    {}
    Material::Ptr FromShaders(const std::string& name,
                              const std::filesystem::path& vertex,
                              const std::filesystem::path& fragment);
    Material::Ptr Get(const std::string& name) const {
        return m_materials.at(name);
    }

    const std::string GetName(const Material::Ptr& ptr) const {
        return m_names.at(ptr);
    }

    std::vector<std::string> GetNames() const
    {
        auto keys = std::views::keys(m_materials);
        return {keys.begin(), keys.end()};
    }

    void Recreate();
private:
    std::unordered_map<std::string, Material::Ptr> m_materials;
    std::unordered_map<Material::Ptr, std::string> m_names;
    std::unordered_map<std::string,
                       std::pair<std::filesystem::path,
                                 std::filesystem::path>> m_used_shaders;

    Material::Ptr Create(const std::string& name,
                         const std::filesystem::path& vertex,
                         const std::filesystem::path& fragment);

    Engine& m_engine;
};

class MeshRenderer
{
public:
    void Init(Engine& m_engine);
    void Begin();
    void Add(Mesh::Ptr mesh, Material::Ptr material, TextureSet::Ptr textures, glm::mat4 model)
    {
        m_toDraw.push_back({model, material, mesh, textures});
    }
    void End();
    void WriteCmdBuffer(vk::CommandBuffer cmd, Engine&);
private:
    struct ToDraw
    {
        glm::mat4 model;
        Material::Ptr material;
        Mesh::Ptr mesh;
        TextureSet::Ptr textures;
    };

    std::vector<ToDraw> m_toDraw;
};
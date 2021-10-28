#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <engine.hpp>
#include <iostream>

class DebugPipelines
{
public:
    ~DebugPipelines();

    void Init(Engine& engine);
    void Begin();

    void DrawLine(glm::vec3 from, glm::vec3 to, glm::vec4 color, float width = 1)
    {
        m_lines.push_back({
                .color = color,
                .from = from,
                .to = to,
                .width = width
            });
    }

    void DrawArrow(glm::vec3 from, glm::vec3 to, glm::vec4 color, float width = 1)
    {
        m_arrows.push_back({
                .color = color,
                .from = from,
                .to = to,
                .width = width
            });
    }

    void DrawBox(glm::vec3 center, glm::vec3 dim, glm::vec4 color)
    {
        BoxData data {
            .color = color,
            .center = center,
            .dimensions = dim
        };
        m_boxes.push_back(data);
    }

    void End(Engine& engine);
    void WriteCmdBuffer(vk::CommandBuffer cmd, Engine& engine);

    void Recreate(Engine& engine);
private:
    void CreateGraphicsPipelines(Engine& engine);
    void CreateVertexBuffers(Engine& engine);

    template<typename T>
    static void ReallocAndCopyToBuffer(const std::vector<T>&, VmaAllocation&,
                                       VkBuffer&, Engine& engine, std::size_t&);

    vk::VertexInputBindingDescription GetLineBindingDescription();
    vk::VertexInputBindingDescription GetBoxBindingDescription();
    vk::VertexInputBindingDescription GetArrowBindingDescription();

    struct LineData
    {
        alignas(16) glm::vec4 color;
        alignas(16) glm::vec3 from;
        alignas(16) glm::vec3 to;
        alignas(4) float width = 0.005;
    };

    struct BoxData
    {
        alignas(16) glm::vec4 color;
        alignas(16) glm::vec3 center;
        alignas(16) glm::vec3 dimensions;
    };

    std::array<vk::VertexInputAttributeDescription, 4> GetLineAttributeDescriptions();
    std::array<vk::VertexInputAttributeDescription, 4> GetArrowAttributeDescriptions();
    std::array<vk::VertexInputAttributeDescription, 3> GetBoxAttributeDescriptions();

    vk::UniquePipeline m_linePipeline;
    vk::UniquePipeline m_arrowPipeline;
    vk::UniquePipeline m_boxPipeline;

    std::vector<LineData> m_lines;
    std::vector<LineData> m_arrows;
    std::vector<BoxData> m_boxes;

    std::vector<VkBuffer> m_lineBuffers;
    std::vector<VkBuffer> m_arrowBuffers;
    std::vector<VkBuffer> m_boxBuffers;

    std::vector<std::size_t> m_lineBuffersSize;
    std::vector<std::size_t> m_arrowBuffersSize;
    std::vector<std::size_t> m_boxBuffersSize;

    std::vector<VmaAllocation> m_lineBuffersAllocation;
    std::vector<VmaAllocation> m_arrowBuffersAllocation;
    std::vector<VmaAllocation> m_boxBuffersAllocation;

    std::optional<VmaAllocator> m_vmaAllocator;
};

template<typename T>
void DebugPipelines::ReallocAndCopyToBuffer(
    const std::vector<T>& vec, VmaAllocation& allocation,
    VkBuffer& buffer, Engine& engine, std::size_t& currentSize)
{
    auto newSize = vec.size() * sizeof(T);

    if (newSize > 0)
    {
        VmaAllocationInfo allocationInfo {};
        vmaGetAllocationInfo(engine.GetVmaAllocator(), allocation, &allocationInfo);

        if (currentSize < newSize)
        {
            VkBufferCreateInfo bufferInfo {};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = newSize;
            currentSize = newSize;
            bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

            vmaDestroyBuffer(engine.GetVmaAllocator(), buffer, allocation);

            vmaCreateBuffer(engine.GetVmaAllocator(),
                            &bufferInfo,
                            &allocInfo,
                            &buffer,
                            &allocation,
                            nullptr);
        }

        void* data;
        vmaMapMemory(engine.GetVmaAllocator(), allocation, &data);
        memcpy(data, vec.data(), newSize);
        vmaUnmapMemory(engine.GetVmaAllocator(), allocation);
    }
}

#include "mesh_renderer.hpp"
#include <tiny_obj_loader.h>
#include <spdlog/spdlog.h>
#include "shader_compiler.hpp"

std::array<vk::VertexInputAttributeDescription, 4>
Vertex::AttributeDescriptions()
{
    std::array<vk::VertexInputAttributeDescription, 4> attributeDescriptions;
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[0].offset = offsetof(Vertex, position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[1].offset = offsetof(Vertex, position);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[2].offset = offsetof(Vertex, position);

    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = vk::Format::eR32G32Sfloat;
    attributeDescriptions[3].offset = offsetof(Vertex, position);

    return attributeDescriptions;

}

vk::VertexInputBindingDescription Vertex::BindingDescription()
{
    vk::VertexInputBindingDescription bindingDescription;
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = vk::VertexInputRate::eVertex;
    return bindingDescription;
}

Mesh::Ptr MeshManager::NewFromObj(const std::string &name, const std::filesystem::path &filename)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string err;

    tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filename.c_str());
    if (!err.empty())
    {
        spdlog::error(err);
        //throw std::runtime_error("Could not load mesh");
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices)
        {
            Vertex vertex;

            vertex.position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            if (attrib.texcoords.size() > 0)
            {
                vertex.textureCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }
            else
            {
                vertex.textureCoord = {
                    0, 0
                };
            }

            vertex.color = {1.0f, 1.0f, 1.0f};

            vertices.push_back(vertex);
            indices.push_back(indices.size());
        }
    }

    return NewFromVertices(name, std::move(vertices), std::move(indices));
}

Mesh::Ptr MeshManager::NewFromVertices(const std::string& name,
                                       std::vector<Vertex> vertices,
                                       std::vector<uint32_t> indices)
{
    Mesh::Ptr result = std::make_shared<Mesh>();

    result->vertexBuffer =
        m_engine.CopyToGPU(vertices, vk::BufferUsageFlagBits::eVertexBuffer);
    result->vertices = std::move(vertices);

    result->indexBuffer =
        m_engine.CopyToGPU(indices, vk::BufferUsageFlagBits::eIndexBuffer);
    result->indices = std::move(indices);

    m_meshes[name] = result;

    return result;
}

Material::Ptr MaterialManager::Create(
    const std::string &name,
    const std::filesystem::path &vertex,
    const std::filesystem::path &fragment
    )
{
    Material::Ptr result = std::make_shared<Material>();

    auto vertexModule = m_engine.CreateShaderModule(
        ShaderCompiler::CompileFromFile(
            vertex, shaderc_shader_kind::shaderc_glsl_vertex_shader));

    auto fragmentModule = m_engine.CreateShaderModule(
        ShaderCompiler::CompileFromFile(
            fragment, shaderc_shader_kind::shaderc_glsl_fragment_shader));

    vk::PipelineShaderStageCreateInfo vertCreateInfo;
    vertCreateInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertCreateInfo.module = *vertexModule;
    vertCreateInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo fragCreateInfo;
    fragCreateInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragCreateInfo.module = *fragmentModule;
    fragCreateInfo.pName = "main";

    std::array shaderStages {vertCreateInfo, fragCreateInfo};

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    auto bindingDescription = Vertex::BindingDescription();
    auto attributeDescriptions = Vertex::AttributeDescriptions();
    vertexInputInfo.setVertexBindingDescriptions(bindingDescription);
    vertexInputInfo.setVertexAttributeDescriptions(attributeDescriptions);

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    inputAssemblyInfo.topology = vk::PrimitiveTopology::eTriangleList;
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    vk::Extent2D extent = m_engine.GetSwapChainExtent();
    vk::Viewport viewport(0, 0, extent.width, extent.height, 0, 1);
    vk::Rect2D scissor({0, 0}, extent);
    vk::PipelineViewportStateCreateInfo viewportStateInfo;
    viewportStateInfo.viewportCount = 1;
    viewportStateInfo.pViewports = &viewport;
    viewportStateInfo.scissorCount = 1;
    viewportStateInfo.pScissors = &scissor;

    vk::PipelineRasterizationStateCreateInfo rasterizerInfo;
    rasterizerInfo.depthClampEnable = VK_FALSE;
    rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizerInfo.polygonMode = vk::PolygonMode::eFill;
    rasterizerInfo.lineWidth = 1.0f;
    rasterizerInfo.cullMode = vk::CullModeFlagBits::eBack;
    rasterizerInfo.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizerInfo.depthBiasEnable = VK_FALSE;

    vk::PipelineMultisampleStateCreateInfo multisamplingInfo{};
    multisamplingInfo.sampleShadingEnable = VK_FALSE;
    multisamplingInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
    colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

    vk::PipelineColorBlendStateCreateInfo colorBlendingInfo;
    colorBlendingInfo.logicOpEnable = VK_FALSE;
    colorBlendingInfo.attachmentCount = 1;
    colorBlendingInfo.pAttachments = &colorBlendAttachment;

    vk::PipelineDepthStencilStateCreateInfo depthStencil;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    vk::PipelineLayoutCreateInfo layoutInfo;
    auto globalLayout = m_engine.GetGlobalSetLayout();
    vk::PushConstantRange range(
        vk::ShaderStageFlagBits::eAllGraphics,
        0, sizeof(PushConstants)
        );
    layoutInfo.setSetLayouts(globalLayout);
    layoutInfo.setPushConstantRanges(range);

    result->pipelineLayout = m_engine.GetDevice().createPipelineLayoutUnique(layoutInfo);

    vk::GraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.setStages(shaderStages);

    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
    pipelineInfo.pViewportState = &viewportStateInfo;
    pipelineInfo.pRasterizationState = &rasterizerInfo;
    pipelineInfo.pMultisampleState = &multisamplingInfo;
    pipelineInfo.pColorBlendState = &colorBlendingInfo;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.layout = *result->pipelineLayout;
    pipelineInfo.renderPass = m_engine.GetRenderPass();
    pipelineInfo.subpass = 0;

    result->pipeline = m_engine.GetDevice().createGraphicsPipelineUnique(VK_NULL_HANDLE, pipelineInfo).value;

    return result;
}

Material::Ptr MaterialManager::FromShaders(
    const std::string &name,
    const std::filesystem::path &vertex,
    const std::filesystem::path &fragment)
{
    Material::Ptr result = Create(name, vertex, fragment);
    m_materials[name] = result;
    m_used_shaders[name] = {vertex, fragment};

    return result;
}

void MaterialManager::Recreate()
{
    for (auto& [name, material] : m_materials)
    {
        const auto& [vertex, fragment] = m_used_shaders[name];
        *material = std::move(*Create(name, vertex, fragment));
    }
}

void MeshRenderer::Begin()
{
    m_toDraw.clear();
}

void MeshRenderer::End()
{

}

void MeshRenderer::WriteCmdBuffer(vk::CommandBuffer cmd, Engine& engine)
{
    Material::Ptr lastMaterial;
    Mesh::Ptr lastMesh;

    TracyVkZone(engine.GetCurrentTracyContext(), cmd, "Meshes");
    for (auto& drawData : m_toDraw)
    {
        if (drawData.material != lastMaterial)
        {
            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *drawData.material->pipeline);
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                   *drawData.material->pipelineLayout, 0,
                                   engine.GetCurrentGlobalSet(), nullptr);
            lastMaterial = drawData.material;
        }
        PushConstants constants;
        constants.model = drawData.model;

        cmd.pushConstants(*drawData.material->pipelineLayout,
                          vk::ShaderStageFlagBits::eAllGraphics, 0, sizeof(constants), &constants);

        if (drawData.mesh != lastMesh)
        {
            vk::DeviceSize offset = 0;
            vk::Buffer buffer = drawData.mesh->vertexBuffer.buffer;
            cmd.bindVertexBuffers(0, buffer, offset);
            cmd.bindIndexBuffer(drawData.mesh->indexBuffer.buffer, 0, vk::IndexType::eUint32);
            lastMesh = drawData.mesh;
        }

        cmd.drawIndexed(drawData.mesh->indices.size(), 1, 0, 0, 0);
    }
}

#include "debug_pipelines.hpp"
#include "files.hpp"
#include "shader_compiler.hpp"
#include <TracyVulkan.hpp>
#include <iostream>

void DebugPipelines::Init(Engine& engine)
{
    m_vmaAllocator = engine.GetVmaAllocator();

    CreateVertexBuffers(engine);
    CreateGraphicsPipelines(engine);
}

void DebugPipelines::CreateVertexBuffers(Engine& engine)
{
    for (int i = 0; i < engine.GetMaxFramesInFlight(); i++)
    {
        VkBufferCreateInfo bufferInfo {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        VmaAllocationCreateInfo allocInfo {};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        m_lineBuffers.push_back({});
        m_lineBuffersAllocation.push_back({});
        bufferInfo.size = sizeof(LineData);
        m_lineBuffersSize.push_back(bufferInfo.size);
        vmaCreateBuffer(engine.GetVmaAllocator(),
                        &bufferInfo,
                        &allocInfo,
                        &m_lineBuffers[i],
                        &m_lineBuffersAllocation[i],
                        nullptr);

        m_arrowBuffers.push_back({});
        m_arrowBuffersAllocation.push_back({});
        bufferInfo.size = sizeof(LineData);
        m_arrowBuffersSize.push_back(bufferInfo.size);
        vmaCreateBuffer(engine.GetVmaAllocator(),
                        &bufferInfo,
                        &allocInfo,
                        &m_arrowBuffers[i],
                        &m_arrowBuffersAllocation[i],
                        nullptr);

        m_boxBuffers.push_back({});
        m_boxBuffersAllocation.push_back({});
        bufferInfo.size = sizeof(BoxData);
        m_boxBuffersSize.push_back(bufferInfo.size);
        vmaCreateBuffer(engine.GetVmaAllocator(),
                        &bufferInfo,
                        &allocInfo,
                        &m_boxBuffers[i],
                        &m_boxBuffersAllocation[i],
                        nullptr);
    }
}

vk::VertexInputBindingDescription DebugPipelines::GetLineBindingDescription()
{
    vk::VertexInputBindingDescription bindingDescription;
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(LineData);
    bindingDescription.inputRate = vk::VertexInputRate::eInstance;
    return bindingDescription;
}

vk::VertexInputBindingDescription DebugPipelines::GetArrowBindingDescription()
{
    return GetLineBindingDescription();
}

vk::VertexInputBindingDescription DebugPipelines::GetBoxBindingDescription()
{
    vk::VertexInputBindingDescription bindingDescription;
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(BoxData);
    bindingDescription.inputRate = vk::VertexInputRate::eInstance;
    return bindingDescription;
}

std::array<vk::VertexInputAttributeDescription, 4>
DebugPipelines::GetLineAttributeDescriptions()
{
    std::array<vk::VertexInputAttributeDescription, 4> attributeDescriptions;
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = vk::Format::eR32G32B32A32Sfloat;
    attributeDescriptions[0].offset = offsetof(LineData, color);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[1].offset = offsetof(LineData, from);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[2].offset = offsetof(LineData, to);

    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = vk::Format::eR32Sfloat;
    attributeDescriptions[3].offset = offsetof(LineData, width);

    return attributeDescriptions;
}

std::array<vk::VertexInputAttributeDescription, 4>
DebugPipelines::GetArrowAttributeDescriptions()
{
    return GetLineAttributeDescriptions();
}

std::array<vk::VertexInputAttributeDescription, 3>
DebugPipelines::GetBoxAttributeDescriptions()
{
    std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions;
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = vk::Format::eR32G32B32A32Sfloat;
    attributeDescriptions[0].offset = offsetof(LineData, color);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[1].offset = offsetof(LineData, from);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[2].offset = offsetof(LineData, to);
    return attributeDescriptions;
}

void DebugPipelines::CreateGraphicsPipelines(Engine& engine)
{
    auto lineVert = engine.CreateShaderModule(
        ShaderCompiler::CompileFromFile(
            Files::Local("shaders/line.vert"),
            shaderc_shader_kind::shaderc_glsl_vertex_shader));

    auto arrowVert = engine.CreateShaderModule(
        ShaderCompiler::CompileFromFile(
            Files::Local("shaders/arrow.vert"),
            shaderc_shader_kind::shaderc_glsl_vertex_shader));

    auto boxVert = engine.CreateShaderModule(
        ShaderCompiler::CompileFromFile(
            Files::Local("shaders/box.vert"),
            shaderc_shader_kind::shaderc_glsl_vertex_shader));

    auto fragModule =
        engine.CreateShaderModule(
            ShaderCompiler::CompileFromFile(
                Files::Local("shaders/color.frag"),
                shaderc_shader_kind::shaderc_glsl_fragment_shader));

    vk::PipelineShaderStageCreateInfo vertCreateInfo;
    vertCreateInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertCreateInfo.module = *lineVert;
    vertCreateInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo fragCreateInfo;
    fragCreateInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragCreateInfo.module = *fragModule;
    fragCreateInfo.pName = "main";

    std::array shaderStages {vertCreateInfo, fragCreateInfo};

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    auto bindingDescription = GetLineBindingDescription();
    auto attributeDescriptions = GetLineAttributeDescriptions();
    vertexInputInfo.setVertexBindingDescriptions(bindingDescription);
    vertexInputInfo.setVertexAttributeDescriptions(attributeDescriptions);

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    inputAssemblyInfo.topology = vk::PrimitiveTopology::eTriangleList;
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    vk::Extent2D extent = engine.GetSwapChainExtent();
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
    rasterizerInfo.cullMode = vk::CullModeFlagBits::eNone;
    rasterizerInfo.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizerInfo.depthBiasEnable = VK_FALSE;

    vk::PipelineMultisampleStateCreateInfo multisamplingInfo{};
    multisamplingInfo.sampleShadingEnable = VK_FALSE;
    multisamplingInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    colorBlendAttachment.blendEnable = VK_TRUE;
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

    vk::GraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.setStages(shaderStages);

    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
    pipelineInfo.pViewportState = &viewportStateInfo;
    pipelineInfo.pRasterizationState = &rasterizerInfo;
    pipelineInfo.pMultisampleState = &multisamplingInfo;
    pipelineInfo.pColorBlendState = &colorBlendingInfo;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.layout = engine.GetPipelineLayout();
    pipelineInfo.renderPass = engine.GetRenderPass();
    pipelineInfo.subpass = 0;

    m_linePipeline = engine.GetDevice().createGraphicsPipelineUnique(VK_NULL_HANDLE, pipelineInfo).value;

    shaderStages[0].module = *arrowVert;
    pipelineInfo.setStages(shaderStages);

    m_arrowPipeline = engine.GetDevice().createGraphicsPipelineUnique(VK_NULL_HANDLE, pipelineInfo).value;

    shaderStages[0].module = *boxVert;
    pipelineInfo.setStages(shaderStages);
    inputAssemblyInfo.topology = vk::PrimitiveTopology::eLineStrip;

    auto boxBindingDescription = GetBoxBindingDescription();
    auto boxAttributeDescriptions = GetBoxAttributeDescriptions();
    vertexInputInfo.setVertexBindingDescriptions(boxBindingDescription);
    vertexInputInfo.setVertexAttributeDescriptions(boxAttributeDescriptions);
    m_boxPipeline = engine.GetDevice().createGraphicsPipelineUnique(VK_NULL_HANDLE, pipelineInfo).value;
}

void DebugPipelines::Begin()
{
    m_lines.clear();
    m_arrows.clear();
    m_boxes.clear();
}

void DebugPipelines::End(Engine& engine)
{
    auto i = engine.GetCurrentFrame();

    ReallocAndCopyToBuffer(m_lines, m_lineBuffersAllocation[i], m_lineBuffers[i], engine, m_lineBuffersSize[i]);
    ReallocAndCopyToBuffer(m_arrows, m_arrowBuffersAllocation[i], m_arrowBuffers[i], engine, m_arrowBuffersSize[i]);
    ReallocAndCopyToBuffer(m_boxes, m_boxBuffersAllocation[i], m_boxBuffers[i], engine, m_boxBuffersSize[i]);
}

void DebugPipelines::WriteCmdBuffer(vk::CommandBuffer cmd, Engine& engine)
{
    auto i = engine.GetCurrentFrame();

    vk::ClearAttachment clearAttachment;
    clearAttachment.aspectMask = vk::ImageAspectFlagBits::eDepth;
    clearAttachment.clearValue = vk::ClearValue(vk::ClearDepthStencilValue(1.f, 0.f));

    auto extent = engine.GetSwapChainExtent();
    vk::ClearRect clearRect;
    clearRect.layerCount = 1;
    clearRect.rect.extent = extent;

    cmd.clearAttachments(clearAttachment, clearRect);

    if (m_lines.size() > 0)
    {
        TracyVkZone(engine.GetCurrentTracyContext(), cmd, "Debug lines");
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_linePipeline);
        cmd.bindVertexBuffers(0, {m_lineBuffers[i]}, {0});
        cmd.draw(6, m_lines.size(), 0, 0);
    }

    if (m_arrows.size() > 0)
    {
        TracyVkZone(engine.GetCurrentTracyContext(), cmd, "Debug arrows");
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_arrowPipeline);
        cmd.bindVertexBuffers(0, {m_arrowBuffers[i]}, {0});
        cmd.draw(9, m_arrows.size(), 0, 0);
    }

    if (m_boxes.size() > 0)
    {
        TracyVkZone(engine.GetCurrentTracyContext(), cmd, "Debug boxes");
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_boxPipeline);
        cmd.bindVertexBuffers(0, {m_boxBuffers[i]}, {0});
        cmd.draw(16, m_boxes.size(), 0, 0);
    }
}

DebugPipelines::~DebugPipelines()
{
    for (int i = 0; i  < m_lines.size(); i++)
    {
        vmaDestroyBuffer(*m_vmaAllocator, m_lineBuffers[i], m_lineBuffersAllocation[i]);
        vmaDestroyBuffer(*m_vmaAllocator, m_arrowBuffers[i], m_arrowBuffersAllocation[i]);
        vmaDestroyBuffer(*m_vmaAllocator, m_boxBuffers[i], m_boxBuffersAllocation[i]);
    }
}

void DebugPipelines::Recreate(Engine &engine)
{
    CreateGraphicsPipelines(engine);
}

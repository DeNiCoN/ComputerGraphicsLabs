#include "mesh_renderer.hpp"
#include "initializers.hpp"
#include <tiny_obj_loader.h>
#include <spdlog/spdlog.h>
#include "shader_compiler.hpp"
#include <stb_image.h>

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
    attributeDescriptions[1].offset = offsetof(Vertex, normal);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[2].offset = offsetof(Vertex, color);

    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = vk::Format::eR32G32Sfloat;
    attributeDescriptions[3].offset = offsetof(Vertex, textureCoord);

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

Texture::Ptr TextureManager::NewFromFile(const std::string &name,
                                         const std::filesystem::path &filename)
{
    int texWidth, texHeight, texChannels;

    stbi_uc* pixels = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels) {
        spdlog::error("Failed to load texture file {}", filename.c_str());
        throw std::runtime_error("");
    }
    auto result = NewFromPixels(name, pixels, texWidth, texHeight);

    stbi_image_free(pixels);
    m_textures[name] = result;
    return result;
}

Texture::Ptr TextureManager::NewFromPixels(const std::string& name,
                                           void* pixel_ptr,
                                           int texWidth, int texHeight)
{
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    //the format R8G8B8A8 matches exactly with the pixels loaded from stb_image lib
    VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB;

    //allocate temporary buffer for holding texture data to upload
    AllocatedBuffer stagingBuffer =
        m_engine.CreateBuffer(imageSize,
                              vk::BufferUsageFlagBits::eTransferSrc,
                              VMA_MEMORY_USAGE_CPU_ONLY);

    //copy data to buffer
    void* data;
    vmaMapMemory(m_engine.GetVmaAllocator(), stagingBuffer.allocation, &data);
    memcpy(data, pixel_ptr, static_cast<size_t>(imageSize));
    vmaUnmapMemory(m_engine.GetVmaAllocator(), stagingBuffer.allocation);
    //we no longer need the loaded data, so we can free the pixels as they are now in the staging buffer

    VkExtent3D imageExtent;
    imageExtent.width = static_cast<uint32_t>(texWidth);
    imageExtent.height = static_cast<uint32_t>(texHeight);
    imageExtent.depth = 1;

    VkImageCreateInfo dimg_info {};
    dimg_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    dimg_info.imageType = VK_IMAGE_TYPE_2D;
    dimg_info.mipLevels = 1;
    dimg_info.arrayLayers = 1;
    dimg_info.samples = VK_SAMPLE_COUNT_1_BIT;
    dimg_info.tiling = VK_IMAGE_TILING_OPTIMAL;

    dimg_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    dimg_info.extent = imageExtent;
    dimg_info.format = image_format;

    AllocatedImage newImage;

    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    //allocate and create the image
    vmaCreateImage(m_engine.GetVmaAllocator(), &dimg_info, &dimg_allocinfo,
                   &newImage.image, &newImage.allocation, nullptr);
    newImage.allocator = m_engine.GetVmaAllocator();
    newImage.device = m_engine.GetDevice();

    m_engine.ImmediateSubmit([&](VkCommandBuffer cmd) {
        VkImageSubresourceRange range;
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;

        VkImageMemoryBarrier imageBarrier_toTransfer = {};
        imageBarrier_toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

        imageBarrier_toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier_toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier_toTransfer.image = newImage.image;
        imageBarrier_toTransfer.subresourceRange = range;

        imageBarrier_toTransfer.srcAccessMask = 0;
        imageBarrier_toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        //barrier the image into the transfer-receive layout
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toTransfer);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = imageExtent;

        //copy the buffer into the image
        vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        VkImageMemoryBarrier imageBarrier_toReadable = imageBarrier_toTransfer;

        imageBarrier_toReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier_toReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imageBarrier_toReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier_toReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        //barrier the image into the shader readable layout
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toReadable);
    });

    Texture::Ptr result = std::make_shared<Texture>();
    result->image = std::move(newImage);

    vk::ImageViewCreateInfo imageViewInfo;
    imageViewInfo.viewType = vk::ImageViewType::e2D;
    imageViewInfo.image = result->image.image;
    imageViewInfo.format = vk::Format::eR8G8B8A8Srgb;
    imageViewInfo.subresourceRange.baseMipLevel = 0;
    imageViewInfo.subresourceRange.levelCount = 1;
    imageViewInfo.subresourceRange.baseArrayLayer = 0;
    imageViewInfo.subresourceRange.layerCount = 1;
    imageViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;

    result->imageView = m_engine.GetDevice().createImageViewUnique(imageViewInfo);

    //create a sampler for the texture
    vk::SamplerCreateInfo samplerInfo;
    samplerInfo.magFilter = vk::Filter::eNearest;
    samplerInfo.minFilter = vk::Filter::eNearest;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;

    result->sampler =
        m_engine.GetDevice().createSamplerUnique(samplerInfo);

    m_textures[name] = result;
    return result;
}

TextureSet::Ptr TextureManager::NewTextureSet(
    Texture::Ptr albedo,
    Texture::Ptr normal,
    Texture::Ptr specular,
    Texture::Ptr roughness,
    Texture::Ptr ao)
{
    auto result = std::make_shared<TextureSet>();
    result->albedo = albedo;
    result->normal = normal;
    result->specular = specular;
    result->roughness = roughness;
    result->ao = ao;

    //allocate the descriptor set for single-texture to use on the material
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool = m_engine.GetGlobalDescriptorPool();
    allocInfo.descriptorSetCount = 1;
    auto layout = m_engine.GetTextureSetLayout();
    allocInfo.setSetLayouts(layout);

    result->descriptor = m_engine.GetDevice().allocateDescriptorSets(allocInfo)[0];

    vk::DescriptorImageInfo albedoInfo;
    if (albedo)
    {
        albedoInfo.sampler = *albedo->sampler;
        albedoInfo.imageView = *albedo->imageView;
        albedoInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    } else {
        albedoInfo.sampler = *m_default_albedo->sampler;
        albedoInfo.imageView = *m_default_albedo->imageView;
        albedoInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }

    vk::DescriptorImageInfo normalInfo;
    if (normal)
    {
        normalInfo.sampler = *normal->sampler;
        normalInfo.imageView = *normal->imageView;
        normalInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    } else {
        normalInfo.sampler = *m_default_normal->sampler;
        normalInfo.imageView = *m_default_normal->imageView;
        normalInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }

    vk::DescriptorImageInfo roughnessInfo;
    if (roughness)
    {
        roughnessInfo.sampler = *roughness->sampler;
        roughnessInfo.imageView = *roughness->imageView;
        roughnessInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    } else {
        roughnessInfo.sampler = *m_default_roughness->sampler;
        roughnessInfo.imageView = *m_default_roughness->imageView;
        roughnessInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }

    vk::DescriptorImageInfo specularInfo;
    if (specular)
    {
        specularInfo.sampler = *specular->sampler;
        specularInfo.imageView = *specular->imageView;
        specularInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    } else {
        specularInfo.sampler = *m_default_specular->sampler;
        specularInfo.imageView = *m_default_specular->imageView;
        specularInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }

    vk::DescriptorImageInfo aoInfo;
    if (ao)
    {
        aoInfo.sampler = *ao->sampler;
        aoInfo.imageView = *ao->imageView;
        aoInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    } else {
        aoInfo.sampler = *m_default_ao->sampler;
        aoInfo.imageView = *m_default_ao->imageView;
        aoInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }

    auto writes = {
        init::ImageWriteDescriptorSet(0, result->descriptor, albedoInfo),
        init::ImageWriteDescriptorSet(1, result->descriptor, normalInfo),
        init::ImageWriteDescriptorSet(2, result->descriptor, specularInfo),
        init::ImageWriteDescriptorSet(3, result->descriptor, roughnessInfo),
        init::ImageWriteDescriptorSet(4, result->descriptor, aoInfo)
    };
    m_engine.GetDevice().updateDescriptorSets(writes, nullptr);
    return result;
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

            if (attrib.normals.size() > 0)
            {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            }

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
    const std::filesystem::path &fragment,
    bool textures
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

    vk::PipelineLayoutCreateInfo layoutInfo;
    auto globalLayout = m_engine.GetGlobalSetLayout();

    std::vector<vk::DescriptorSetLayout> layouts;
    layouts.push_back(globalLayout);

    if (textures)
    {
        layouts.push_back(m_engine.GetTextureSetLayout());
    }

    result->textures = textures;

    vk::PushConstantRange range(
        vk::ShaderStageFlagBits::eAllGraphics,
        0, sizeof(PushConstants)
        );
    layoutInfo.setSetLayouts(layouts);
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
    m_names[result] = name;
    m_used_shaders[name] = {vertex, fragment};

    return result;
}

Material::Ptr MaterialManager::Textureless(
    const std::string &name,
    const std::filesystem::path &vertex,
    const std::filesystem::path &fragment)
{
    Material::Ptr result = Create(name, vertex, fragment, false);
    m_materials[name] = result;
    m_names[result] = name;
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
    TextureSet::Ptr lastTextureSet;

    TracyVkZone(engine.GetCurrentTracyContext(), cmd, "Meshes");
    for (auto& drawData : m_toDraw)
    {
        if (drawData.textures != lastTextureSet)
        {
            if (drawData.material->textures)
                lastTextureSet = drawData.textures;

            if (drawData.material == lastMaterial)
            {
                if (lastTextureSet != nullptr && drawData.material->textures)
                {
                    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                           *drawData.material->pipelineLayout, 1,
                                           lastTextureSet->descriptor, nullptr);
                }
            }
        }

        if (drawData.material != lastMaterial)
        {
            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *drawData.material->pipeline);
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                   *drawData.material->pipelineLayout, 0,
                                   engine.GetCurrentGlobalSet(), nullptr);

            if (lastTextureSet != nullptr && drawData.material->textures)
            {
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                       *drawData.material->pipelineLayout, 1,
                                       lastTextureSet->descriptor, nullptr);
            }

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

void TextureManager::Init()
{
    uint32_t albedo_pixels[] {glm::packSnorm4x8({1, 0, 1, 1})};
    m_default_albedo = NewFromPixels("default_albedo", albedo_pixels, 1, 1);

    uint32_t normal_pixels[] {glm::packSnorm4x8({0, 0, 1, 1})};
    m_default_normal = NewFromPixels("default_normal", normal_pixels, 1, 1);

    uint32_t specular_pixels[] {glm::packSnorm4x8({0.5, 0.5, 0.5, 0.5})};
    m_default_specular = NewFromPixels("default_specular", specular_pixels, 1, 1);

    uint32_t roughness_pixels[] {glm::packSnorm4x8({0.5, 0.5, 0.5, 0.5})};
    m_default_roughness = NewFromPixels("default_roughness", roughness_pixels, 1, 1);

    uint32_t ao_pixels[] {glm::packSnorm4x8({1, 1, 1, 1})};
    m_default_ao = NewFromPixels("default_ao", ao_pixels, 1, 1);
}

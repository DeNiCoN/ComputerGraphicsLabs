#pragma once
#include <vulkan/vulkan.hpp>

namespace init
{
    inline vk::WriteDescriptorSet ImageWriteDescriptorSet(
        int binding, vk::DescriptorSet dst,
        vk::DescriptorImageInfo& imageInfo)
    {
        vk::WriteDescriptorSet diffuseWrite;
        diffuseWrite.dstBinding = binding;
        diffuseWrite.dstSet = dst;
        diffuseWrite.descriptorCount = 1;
        diffuseWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        diffuseWrite.pImageInfo = &imageInfo;
        return diffuseWrite;
    }
}

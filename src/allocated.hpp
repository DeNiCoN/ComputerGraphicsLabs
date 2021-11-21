#pragma once
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

struct AllocatedImage
{
    VkImage image = VK_NULL_HANDLE;
    VkDevice device;
    VmaAllocation allocation = {};
    VmaAllocator allocator = {};

    AllocatedImage() = default;
    AllocatedImage(const AllocatedImage&) = delete;
    AllocatedImage(AllocatedImage&& rhs)
    {
        swap(*this, rhs);
    }

    friend void swap(AllocatedImage& lhs, AllocatedImage& rhs)
    {
        using std::swap;

        swap(lhs.image, rhs.image);
        swap(lhs.device, rhs.device);
        swap(lhs.allocation, rhs.allocation);
        swap(lhs.allocator, rhs.allocator);
    }

    AllocatedImage& operator=(AllocatedImage&& other)
    {
        AllocatedImage tmp(std::move(other));
        swap(tmp, *this);
        return *this;
    };

    ~AllocatedImage()
    {
        if (image != VK_NULL_HANDLE)
        {
            vkDestroyImage(device, image, nullptr);
            vmaFreeMemory(allocator, allocation);
        }
    }
};

struct AllocatedBuffer
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDevice device;
    VmaAllocation allocation = {};
    VmaAllocator allocator = {};

    AllocatedBuffer() = default;
    AllocatedBuffer(const AllocatedBuffer&) = delete;
    AllocatedBuffer(AllocatedBuffer&& rhs)
    {
        swap(*this, rhs);
    }

    friend void swap(AllocatedBuffer& lhs, AllocatedBuffer& rhs)
    {
        using std::swap;

        swap(lhs.buffer, rhs.buffer);
        swap(lhs.device, rhs.device);
        swap(lhs.allocation, rhs.allocation);
        swap(lhs.allocator, rhs.allocator);
    }

    AllocatedBuffer& operator=(AllocatedBuffer&& other)
    {
        AllocatedBuffer tmp(std::move(other));
        swap(tmp, *this);
        return *this;
    };

    ~AllocatedBuffer()
    {
        if (buffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device, buffer, nullptr);
            vmaFreeMemory(allocator, allocation);
        }
    }
};

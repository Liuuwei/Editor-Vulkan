#pragma once

#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <vulkan/vulkan.h>
#include "Tools.h"

#include <iostream>

class Buffer {
public:
    Buffer(VkPhysicalDevice physicalDevice, VkDevice device);
    ~Buffer();

    void init();

    VkBuffer buffer() const { return buffer_; }
    VkDeviceMemory memory() const { return memory_; }
    void* map(VkDeviceSize size) {
        if (data_) {
            return data_;
        }
        
        VK_CHECK(vkMapMemory(device_, memory_, 0, size, 0, &data_));
        return data_;
    }

    void unMap() {
        data_ = nullptr;
        vkUnmapMemory(device_, memory_);
    }

    int count() const {
        return count_;
    }

    void setCount(uint32_t count) {
        count_ = count;
    }

private:
    VkPhysicalDevice physicalDevice_;
    VkDevice device_;
    VkBuffer buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    void* data_ = nullptr;
    int count_ = 0;

    void checkResource();
    void clean();
    
public:
    VkDeviceSize size_{};
    VkBufferUsageFlags usage_{};
    VkSharingMode sharingMode_{};
    uint32_t queueFamilyIndexCount_{};
    uint32_t* pQueueFamilyIndices_{};

    VkMemoryPropertyFlags memoryProperties_{};
};
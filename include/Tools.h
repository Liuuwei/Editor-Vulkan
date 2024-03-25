#pragma once

#include "GLFW/glfw3.h"
#include "vulkan/vulkan_core.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>

#include <vector>
#include <string>
#include <set>
#include <optional>
#include <fstream>
#include <iostream>
#include <cassert>

struct Tools {

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;
    std::optional<uint32_t> transfer;

    bool compeleted() const;

    bool multiple() const;

    std::vector<uint32_t> sets() const;

    VkSharingMode sharingMode() const;

    static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
};

struct SwapChainSupportDetail {
    VkSurfaceCapabilitiesKHR capabilities_;
    std::vector<VkSurfaceFormatKHR> formats_;
    std::vector<VkPresentModeKHR> presentModes_;

    VkSurfaceFormatKHR chooseSurfaceFormat() const;

    VkExtent2D chooseSwapExtent(GLFWwindow* window) const;

    VkPresentModeKHR choosePresentMode() const;

    static SwapChainSupportDetail querySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
};

static std::vector<const char*> getRequiredExtensions();

static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo, PFN_vkDebugUtilsMessengerCallbackEXT callback);

static VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

static std::vector<char> readFile(const std::string& path);

static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

static void setImageLayout(
    VkCommandBuffer cmdBuffer, 
    VkImage image, 
    VkImageLayout oldLayout, 
    VkImageLayout newLayout, 
    VkImageSubresourceRange subresourcesRange, 
    VkPipelineStageFlags srcStageMask, 
    VkPipelineStageFlags dstStageMask);

static std::string errorString(VkResult errorCode);

#ifndef VK_CHECK
#define VK_CHECK(f) \
{ \
    VkResult res = (f); \
    if (res != VK_SUCCESS) { \
        std::cout << "Fatal: VkResult is " << Tools::errorString(res) << __FILE__ << " at line " << __LINE__ << std::endl; \
        assert(res == VK_SUCCESS); \
    } \
}

#endif

static float pointToLineLength(float a, float b, float x, float y);

static char keyToChar(int key);

static char charToShiftChar(char c);

static std::string rmFrontSpace(const std::string& str);

static std::string rmBackSpace(const std::string& str);

static std::string rmSpace(const std::string& str);

static std::string getForwardWord(const std::string& str, int index);

};
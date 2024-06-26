#include "Vulkan.h"
#include "Buffer.h"
#include "CommandBuffer.h"
#include "CommandLine.h"
#include "CommandPool.h"
#include "DescriptorPool.h"
#include "DescriptorSetLayout.h"
#include "Editor.h"
#include "FrameBuffer.h"
#include "GLFW/glfw3.h"
#include "Image.h"
#include "Pipeline.h"
#include "PipelineLayout.h"
#include "RenderTarget.h"
#include "Swapchain.h"
#include "Timer.h"
#include "Tools.h"
#include "vulkan/vulkan_core.h"
#include <cassert>
#include <cerrno>
#include <cmath>
#include <format>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <sys/stat.h>
#include <unordered_set>
#include <algorithm>
#include <format>
#include <utility>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define GLM_FORCE_RADIANS
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include "ShaderModule.h"
#include "Plane.h"
#include "LineNumber.h"
#include "Grammar.h"

Vulkan::Vulkan(const std::string& title, uint32_t width, uint32_t height) : width_(width), height_(height), title_(title) {
    camera_ = std::make_shared<Camera>();
    initWindow();
    initVulkan();
    initOther();
}

void Vulkan::run() {
    while (!glfwWindowShouldClose(windows_)) {
        glfwPollEvents();
        static unsigned long long prev = Timer::nowMilliseconds();
        draw();
        auto curr = Timer::nowMilliseconds();
        char msg[64];
        snprintf(msg, 10, "ms: %.0f", 1.0f / (static_cast<float>(curr - prev) / 1000));
        prev = curr;
        // glfwSetWindowTitle(windows_, msg);
    }

    vkDeviceWaitIdle(device_);
}

void Vulkan::initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

    windows_ = glfwCreateWindow(width_, height_, title_.c_str(), nullptr, nullptr);
    // glfwSetInputMode(windows_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowUserPointer(windows_, this);
    glfwSetKeyCallback(windows_, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        int inWindow = glfwGetWindowAttrib(window, GLFW_HOVERED);
        if (!inWindow) {
            return ;
        }
        auto vulkan = reinterpret_cast<Vulkan*>(glfwGetWindowUserPointer(window));
        
        vulkan->input(key, scancode, action, mods);
    });
    glfwSetCursorPosCallback(windows_, [](GLFWwindow* window, double xpos, double ypos) {
        auto vulkan = reinterpret_cast<Vulkan*>(glfwGetWindowUserPointer(window));
    });
    glfwSetMouseButtonCallback(windows_, [](GLFWwindow* window, int button, int action, int mods) {
        auto vulkan = reinterpret_cast<Vulkan*>(glfwGetWindowUserPointer(window));
    });
}

void Vulkan::initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicDevice();
    createSwapChain();
    createRenderPass();
    createCommandPool();
    createCommandBuffers();
    createUniformBuffers();
    loadAssets();
    createSamplers();
    createDescriptorPool();
    createDescriptorSetLayout();
    createDescriptorSet();
    createVertex();
    createEditor();
    createGraphicsPipelines();
    createColorResource();
    createDepthResource();
    createFrameBuffer();
    createVertexBuffer();
    createIndexBuffer();
    createSyncObjects();
    mergeResource();
}

void Vulkan::initOther() {
    keyboard_ = std::make_shared<Keyboard>(60);
    grammar_ = std::make_shared<Grammar>("../config/grammar.json");
}

void Vulkan::createInstance() {
    if (!checkValidationLayerSupport()) {
        std::runtime_error("Validation layers requested, but no avaliable!");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.apiVersion = VK_API_VERSION_1_3;

    
    auto extensions = Tools::getRequiredExtensions();
    VkInstanceCreateInfo creatInfo{};
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    Tools::populateDebugMessengerCreateInfo(debugCreateInfo, debugCallback);
    creatInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    creatInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    creatInfo.pApplicationInfo = &appInfo;
    creatInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers_.size());
    creatInfo.ppEnabledLayerNames = validationLayers_.data();
    creatInfo.enabledExtensionCount = extensions.size();
    creatInfo.ppEnabledExtensionNames = extensions.data();
    creatInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;

    if (vkCreateInstance(&creatInfo, nullptr, &instance_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }
}

void Vulkan::setupDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    Tools::populateDebugMessengerCreateInfo(createInfo, debugCallback);

    if (Tools::createDebugUtilsMessengerEXT(instance_, &createInfo, nullptr, &debugMessenger_) != VK_SUCCESS) {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

void Vulkan::createSurface() {
    if (glfwCreateWindowSurface(instance_, windows_, nullptr, &surface_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
}

void Vulkan::pickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) {
        throw std::runtime_error("faield to find GPU with Vulkan support!");
    }
    std::vector<VkPhysicalDevice> physicalDevices(count);
    vkEnumeratePhysicalDevices(instance_, &count, physicalDevices.data());

    for (const auto& device : physicalDevices) {
        VkPhysicalDeviceProperties pro;
        vkGetPhysicalDeviceProperties(device, &pro);
        if (deviceSuitable(device)) {
            physicalDevice_ = device;
            msaaSamples_ = getMaxUsableSampleCount();
            break;
        }
    }

    if (physicalDevice_ == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable physical device!");
    }
}

void Vulkan::createLogicDevice() {
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    float queuePriority = 1.0f;
    for (auto queueFamily : queueFamilies_.sets()) {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = queueFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;

        queueInfos.push_back(queueInfo);
    }

    VkDeviceCreateInfo deviceInfo{};
    VkPhysicalDeviceFeatures features{};
    vkGetPhysicalDeviceFeatures(physicalDevice_, &features);
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = queueInfos.size();
    deviceInfo.pQueueCreateInfos = queueInfos.data();
    deviceInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers_.size());
    deviceInfo.ppEnabledLayerNames = validationLayers_.data();
    deviceInfo.enabledExtensionCount = deviceExtensions_.size();
    deviceInfo.ppEnabledExtensionNames = deviceExtensions_.data();
    deviceInfo.pEnabledFeatures = &features;

    if (vkCreateDevice(physicalDevice_, &deviceInfo, nullptr, &device_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logic device!");
    }

    vkGetDeviceQueue(device_, queueFamilies_.graphics.value(), 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, queueFamilies_.present.value(), 0, &presentQueue_);
    vkGetDeviceQueue(device_, queueFamilies_.transfer.value(), 0, &transferQueue_);
}

void Vulkan::createSwapChain() {
    auto swapChainSupport = Tools::SwapChainSupportDetail::querySwapChainSupport(physicalDevice_, surface_);
    
    auto surfaceFormat = swapChainSupport.chooseSurfaceFormat();
    auto presentMode = swapChainSupport.choosePresentMode();
    auto extent = swapChainSupport.chooseSwapExtent(windows_);
    
    uint32_t imageCount = 0;
    imageCount = swapChainSupport.capabilities_.minImageCount + 1;
    if (swapChainSupport.capabilities_.maxImageCount > 0) {
        imageCount = std::min(imageCount, swapChainSupport.capabilities_.maxImageCount);
    }

    swapChain_ = std::make_shared<SwapChain>(device_);
    swapChain_->surface_ = surface_;
    swapChain_->minImageCount_ = imageCount;
    swapChain_->imageFormat_ = surfaceFormat.format;
    swapChain_->imageColorSpace_ = surfaceFormat.colorSpace;
    swapChain_->imageExtent_ = extent;
    swapChain_->imageArrayLayers_ = 1;
    swapChain_->imageUsage_ = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapChain_->imageSharingMode_ = queueFamilies_.multiple() ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
    swapChain_->queueFamilyIndexCount_ = queueFamilies_.sets().size();
    swapChain_->pQueueFamilyIndices_ = queueFamilies_.sets().data();
    swapChain_->preTransform_ = swapChainSupport.capabilities_.currentTransform;
    swapChain_->compositeAlpha_ = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChain_->presentMode_ = presentMode;
    swapChain_->clipped_ = VK_TRUE;
    swapChain_->oldSwapchain_ = VK_NULL_HANDLE;
    swapChain_->init();
}

void Vulkan::createRenderPass() {
    VkAttachmentDescription colorAttachment{}, colorAttachmentResolve{}, depthAttachment{};

    colorAttachment.format = swapChain_->format();
    colorAttachment.samples = msaaSamples_;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    colorAttachmentResolve.format = swapChain_->format();
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = msaaSamples_;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{}, colorAttachmentResolveRef{}, depthAttachmentRef{};
    
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    colorAttachmentResolveRef.attachment = 1;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    depthAttachmentRef.attachment = 2;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pResolveAttachments = &colorAttachmentResolveRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency subpassDependency{};
    subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependency.dstSubpass = 0;
    subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    subpassDependency.srcAccessMask =  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::vector<VkAttachmentDescription> attachment{colorAttachment, colorAttachmentResolve, depthAttachment};

    renderPass_ = std::make_shared<RenderPass>(device_);
    renderPass_->subpassCount_ = 1;
    renderPass_->pSubpasses_ = &subpass;
    renderPass_->dependencyCount_ = 1;
    renderPass_->pDependencies_ = &subpassDependency;
    renderPass_->attachmentCount_ = static_cast<uint32_t>(attachment.size());
    renderPass_->pAttachments_ = attachment.data();
    renderPass_->init();
}

void Vulkan::createUniformBuffers() {
    auto size = sizeof(UniformBufferObject);

    uniformBuffers_ = std::make_shared<Buffer>(physicalDevice_, device_);
    uniformBuffers_->size_ = size;
    uniformBuffers_->usage_ = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    uniformBuffers_->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
    uniformBuffers_->pQueueFamilyIndices_ = queueFamilies_.sets().data();
    uniformBuffers_->memoryProperties_ = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    uniformBuffers_->sharingMode_ = VK_SHARING_MODE_EXCLUSIVE;
    uniformBuffers_->init();

    uniformBuffers_->map(size);

    canvasUniformBuffer_ = std::make_shared<Buffer>(physicalDevice_, device_);
    canvasUniformBuffer_->size_ = size;
    canvasUniformBuffer_->usage_ = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    canvasUniformBuffer_->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
    canvasUniformBuffer_->pQueueFamilyIndices_ = queueFamilies_.sets().data();
    canvasUniformBuffer_->memoryProperties_ = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    canvasUniformBuffer_->sharingMode_ = VK_SHARING_MODE_EXCLUSIVE;
    canvasUniformBuffer_->init();
    auto data = canvasUniformBuffer_->map(sizeof(UniformBufferObject));
    UniformBufferObject ubo{};
    ubo.proj_ = glm::ortho(-static_cast<float>(swapChain_->width() / 2.0f), static_cast<float>(swapChain_->width() / 2.0f), -static_cast<float>(swapChain_->height() / 2.0), static_cast<float>(swapChain_->height() / 2.0f));
    memcpy(data, &ubo, sizeof(UniformBufferObject));
    canvasUniformBuffer_->unMap();
}

void Vulkan::createSamplers() {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice_, &properties);
    canvasSampler_ = std::make_shared<Sampler>(device_);

    canvasSampler_->addressModeU_ = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    canvasSampler_->addressModeV_ = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    canvasSampler_->addressModeW_ = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    canvasSampler_->minLod_ = 0.0f;
    canvasSampler_->maxLod_ = VK_LOD_CLAMP_NONE;
    canvasSampler_->anisotropyEnable_ = VK_TRUE;
    canvasSampler_->maxAnisotropy_ = properties.limits.maxSamplerAnisotropy;
    canvasSampler_->init();

    for (auto& name : textureNames_) {
        auto sampler = std::make_shared<Sampler>(device_);
        sampler->addressModeU_ = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->addressModeV_ = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->addressModeW_ = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->minLod_ = 0.0f;
        sampler->maxLod_ = static_cast<float>(canvasMipLevels_.at(name));
        sampler->anisotropyEnable_ = VK_TRUE;
        sampler->maxAnisotropy_ = properties.limits.maxSamplerAnisotropy;
        sampler->init();

        canvasSamplers_.insert({name, std::move(sampler)});
    }
}

void Vulkan::createDescriptorPool() {
    createTextDescriptorPool();
    createCanvasDescriptorPool();
}

void Vulkan::createTextDescriptorPool() {
    std::vector<VkDescriptorPoolSize> poolSizes(2);
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(dictionary_.size());

    fontDescriptorPool_ = std::make_shared<DescriptorPool>(device_);
    fontDescriptorPool_->poolSizeCount_ = static_cast<uint32_t>(poolSizes.size());
    fontDescriptorPool_->pPoolSizes_ = poolSizes.data();
    fontDescriptorPool_->maxSets_ = 1;
    fontDescriptorPool_->init();
}

void Vulkan::createCanvasDescriptorPool() {
    std::vector<VkDescriptorPoolSize> poolSizes(2);
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;

    canvasDescriptorPool_ = std::make_shared<DescriptorPool>(device_);
    canvasDescriptorPool_->flags_ = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    canvasDescriptorPool_->poolSizeCount_ = static_cast<uint32_t>(poolSizes.size());
    canvasDescriptorPool_->pPoolSizes_ = poolSizes.data();
    canvasDescriptorPool_->maxSets_ = 2;
    canvasDescriptorPool_->init();
}

void Vulkan::createDescriptorSetLayout() {
    createTextDescriptorSetLayout();
    createCanvasDescriptorSetLayout();
}


void Vulkan::createTextDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboBinding{}, samplerBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    samplerBinding.binding = 1;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::vector<VkDescriptorSetLayoutBinding> bindings = {uboBinding, samplerBinding};

    canvasDescriptorSetLayout_ = std::make_shared<DescriptorSetLayout>(device_);
    canvasDescriptorSetLayout_->bindingCount_ = static_cast<uint32_t>(bindings.size());
    canvasDescriptorSetLayout_->pBindings_ = bindings.data();
    canvasDescriptorSetLayout_->init();
}

void Vulkan::createCanvasDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboBinding{}, samplerBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    samplerBinding.binding = 1;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = dictionary_.size();
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::vector<VkDescriptorSetLayoutBinding> bindings = {uboBinding, samplerBinding};

    fontDescriptorSetLayout_ = std::make_shared<DescriptorSetLayout>(device_);
    fontDescriptorSetLayout_->bindingCount_ = static_cast<uint32_t>(bindings.size());
    fontDescriptorSetLayout_->pBindings_ = bindings.data();
    fontDescriptorSetLayout_->init();
}

void Vulkan::createDescriptorSet() {
    createTextDescriptorSet();
    createCanvasDescriptorSet();
    createCursorDescriptorSet();
}

void Vulkan::createTextDescriptorSet() {
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts(1, fontDescriptorSetLayout_->descriptorSetLayout());

    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = fontDescriptorPool_->descriptorPool();
    allocateInfo.descriptorSetCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    allocateInfo.pSetLayouts = descriptorSetLayouts.data();

    if (vkAllocateDescriptorSets(device_, &allocateInfo, &fontDescriptorSet_) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffers_->buffer();
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);

    
    std::vector<VkDescriptorImageInfo> imageInfos(dictionary_.size());
    for (uint32_t i = 0; i < 128; i++) {
        char c = static_cast<char>(i);
        imageInfos[i].imageView = dictionary_[c].image_->view();
        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[i].sampler = canvasSampler_->sampler();
    }

    std::vector<VkWriteDescriptorSet> descriptorWrites(2);
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = fontDescriptorSet_;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = fontDescriptorSet_;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].descriptorCount = static_cast<uint32_t>(imageInfos.size());
    descriptorWrites[1].pImageInfo = imageInfos.data();
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void Vulkan::createCanvasDescriptorSet() {
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts(1, canvasDescriptorSetLayout_->descriptorSetLayout());

    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = canvasDescriptorPool_->descriptorPool();
    allocateInfo.descriptorSetCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    allocateInfo.pSetLayouts = descriptorSetLayouts.data();

    if (canvasDescriptorSets_ != VK_NULL_HANDLE) {
        VK_CHECK(vkFreeDescriptorSets(device_, canvasDescriptorPool_->descriptorPool(), 1, &canvasDescriptorSets_));
    }
    VK_CHECK(vkAllocateDescriptorSets(device_, &allocateInfo, &canvasDescriptorSets_));

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffers_->buffer();
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);

    VkDescriptorImageInfo samplerInfo{};
    samplerInfo.imageView = canvasImages_[canvasTextureName_]->view();
    samplerInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    samplerInfo.sampler = canvasSamplers_[canvasTextureName_]->sampler();

    std::vector<VkWriteDescriptorSet> descriptorWrites(2);
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = canvasDescriptorSets_;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = canvasDescriptorSets_;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].pImageInfo = &samplerInfo;

    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void Vulkan::createCursorDescriptorSet() {
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts(1, canvasDescriptorSetLayout_->descriptorSetLayout());

    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = canvasDescriptorPool_->descriptorPool();
    allocateInfo.descriptorSetCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    allocateInfo.pSetLayouts = descriptorSetLayouts.data();

    if (canvasDescriptorSets_ != VK_NULL_HANDLE) {
        VK_CHECK(vkFreeDescriptorSets(device_, canvasDescriptorPool_->descriptorPool(), 1, &cursorDescriptorSet_));
    }
    VK_CHECK(vkAllocateDescriptorSets(device_, &allocateInfo, &cursorDescriptorSet_));

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffers_->buffer();
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);

    VkDescriptorImageInfo samplerInfo{};
    samplerInfo.imageView = canvasImages_[canvasTextureName_]->view();
    samplerInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    samplerInfo.sampler = canvasSampler_->sampler();

    std::vector<VkWriteDescriptorSet> descriptorWrites(2);
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = cursorDescriptorSet_;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = cursorDescriptorSet_;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].pImageInfo = &samplerInfo;

    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void Vulkan::createVertex() {
    canvas_ = std::make_shared<Plane>();
    auto t = canvas_->vertices(0, 0, swapChain_->width(), swapChain_->height());
    canvasVertices_ = t.first;
    canvasIndices_ = t.second;

    textVertexBuffer_ = std::make_shared<Buffer>(physicalDevice_, device_);
    textIndexBuffer_ = std::make_shared<Buffer>(physicalDevice_, device_);

    lineNumberVertexBuffer_ = std::make_shared<Buffer>(physicalDevice_, device_);
    lineNumberIndexBuffer_ = std::make_shared<Buffer>(physicalDevice_, device_);
    
    cmdVertexBuffer_ = std::make_shared<Buffer>(physicalDevice_, device_);
    cmdIndexBuffer_ = std::make_shared<Buffer>(physicalDevice_, device_);

    modeVertexBuffer_ = std::make_shared<Buffer>(physicalDevice_, device_);
    modeIndexBuffer_ = std::make_shared<Buffer>(physicalDevice_, device_);

    cursorVertexBuffer_ = std::make_shared<Buffer>(physicalDevice_, device_);
    cursorIndexBuffer_ = std::make_shared<Buffer>(physicalDevice_, device_);
}

void Vulkan::createEditor() {
    editor_ = std::make_shared<Editor>(swapChain_->width(), swapChain_->height(), font_->lineHeight_, font_->advance_, 5);

    lineNumber_ = std::make_shared<LineNumber>(*editor_);

    commandLine_ = std::make_shared<CommandLine>(*editor_);    
    
    // for (int i = 0; i < 1000; i ++) {
    //     for (int j = 0; j < 50; j++) {
    //         editor_->insertStr("int ");
    //     }
    //     editor_->enter();
    //     lineNumber_->adjust(*editor_);
    // }
}

void Vulkan::createGraphicsPipelines() {
    createCanvasPipeline();
    createTextPipeline();
    createCursorPipeline();
}

void Vulkan::createTextPipeline() {
    auto vertFont = ShaderModule(device_, "../shaders/spv/FontVert.spv"); 
    auto fragFont = ShaderModule(device_, "../shaders/spv/FontFrag.spv");

    VkPipelineShaderStageCreateInfo vertexStageInfo{}, fragmentStageInfo{};
    vertexStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStageInfo.module = vertFont.shader();
    vertexStageInfo.pName = "main";

    fragmentStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentStageInfo.module = fragFont.shader();
    fragmentStageInfo.pName = "main";

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages{vertexStageInfo, fragmentStageInfo};
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    auto bindingDescription = font_->bindingDescription(0);
    auto attributeDescription = font_->attributeDescription(0);

    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescription.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
    inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.topology = font_->topology();

    VkPipelineViewportStateCreateInfo viewportInfo{};
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.viewportCount = 1;
    viewportInfo.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizaInfo{};
    rasterizaInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizaInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizaInfo.cullMode = VK_CULL_MODE_NONE;
    rasterizaInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizaInfo.lineWidth = 5.0f;

    VkPipelineMultisampleStateCreateInfo multipleInfo{};
    multipleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multipleInfo.rasterizationSamples = msaaSamples_;
    multipleInfo.minSampleShading = 1.0f;
    
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
    depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilInfo.depthTestEnable = VK_TRUE;
    depthStencilInfo.depthWriteEnable = VK_TRUE;
    depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilInfo.minDepthBounds = 0.0f;
    depthStencilInfo.maxDepthBounds = 1.0f;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentInfo{};
    colorBlendAttachmentInfo.blendEnable = VK_FALSE;
    colorBlendAttachmentInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachmentInfo.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachmentInfo.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachmentInfo.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentInfo.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentInfo.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentInfo.alphaBlendOp = VK_BLEND_OP_ADD;
    
    VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
    colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendInfo.attachmentCount = 1;
    colorBlendInfo.pAttachments = &colorBlendAttachmentInfo;

    std::vector<VkDynamicState> dynamics{
        VK_DYNAMIC_STATE_VIEWPORT, 
        VK_DYNAMIC_STATE_SCISSOR, 
    };
    VkPipelineDynamicStateCreateInfo dynamicInfo{};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = static_cast<uint32_t>(dynamics.size());
    dynamicInfo.pDynamicStates = dynamics.data();

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {fontDescriptorSetLayout_->descriptorSetLayout()};
    fontPipelineLayout_ = std::make_shared<PipelineLayout>(device_);
    fontPipelineLayout_->setLayoutCount_ = static_cast<uint32_t>(descriptorSetLayouts.size());
    fontPipelineLayout_->pSetLayouts_ = descriptorSetLayouts.data();
    fontPipelineLayout_->init();

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    fontPipeline_ = std::make_shared<Pipeline>(device_);
    fontPipeline_->stageCount_ = shaderStages.size();
    fontPipeline_->pStages_ = shaderStages.data();
    fontPipeline_->pVertexInputState_ = &vertexInputInfo;
    fontPipeline_->pInputAssemblyState_ = &inputAssemblyInfo;
    fontPipeline_->pViewportState_ = &viewportInfo;
    fontPipeline_->pRasterizationState_ = &rasterizaInfo;;
    fontPipeline_->pMultisampleState_ = &multipleInfo;
    fontPipeline_->pDepthStencilState_ = &depthStencilInfo;
    fontPipeline_->pColorBlendState_ = &colorBlendInfo;
    fontPipeline_->pDynamicState_ = &dynamicInfo;
    fontPipeline_->layout_ = fontPipelineLayout_->pipelineLayout();
    fontPipeline_->renderPass_ = renderPass_->renderPass();
    fontPipeline_->init();

    fontPipeline_->descriptorSets_["font"] = fontDescriptorSet_;
}

void Vulkan::createCanvasPipeline() {
    auto vertCanvas = ShaderModule(device_, "../shaders/spv/CanvasVert.spv");
    auto fragCanvas = ShaderModule(device_, "../shaders/spv/CanvasFrag.spv");

    VkPipelineShaderStageCreateInfo vertexStageInfo{}, fragmentStageInfo{};
    vertexStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStageInfo.module = vertCanvas.shader();
    vertexStageInfo.pName = "main";

    fragmentStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentStageInfo.module = fragCanvas.shader();
    fragmentStageInfo.pName = "main";

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages{vertexStageInfo, fragmentStageInfo};
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    auto bindingDescription = canvas_->bindingDescription(0);
    auto attributeDescription = canvas_->attributeDescription(0);

    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescription.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
    inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.topology = canvas_->topology();

    VkPipelineViewportStateCreateInfo viewportInfo{};
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.viewportCount = 1;
    viewportInfo.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizaInfo{};
    rasterizaInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizaInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizaInfo.cullMode = VK_CULL_MODE_NONE;
    rasterizaInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizaInfo.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multipleInfo{};
    multipleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multipleInfo.rasterizationSamples = msaaSamples_;
    multipleInfo.minSampleShading = 1.0f;
    
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
    depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilInfo.depthTestEnable = VK_TRUE;
    depthStencilInfo.depthWriteEnable = VK_TRUE;
    depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilInfo.minDepthBounds = 0.0f;
    depthStencilInfo.maxDepthBounds = 1.0f;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentInfo{};
    colorBlendAttachmentInfo.blendEnable = VK_TRUE;
    colorBlendAttachmentInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachmentInfo.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    colorBlendAttachmentInfo.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    colorBlendAttachmentInfo.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentInfo.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentInfo.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentInfo.alphaBlendOp = VK_BLEND_OP_ADD;
    
    VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
    colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendInfo.attachmentCount = 1;
    colorBlendInfo.pAttachments = &colorBlendAttachmentInfo;

    std::vector<VkDynamicState> dynamics{
        VK_DYNAMIC_STATE_VIEWPORT, 
        VK_DYNAMIC_STATE_SCISSOR, 
    };
    VkPipelineDynamicStateCreateInfo dynamicInfo{};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = static_cast<uint32_t>(dynamics.size());
    dynamicInfo.pDynamicStates = dynamics.data();

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {canvasDescriptorSetLayout_->descriptorSetLayout()};
    canvasPipelineLayout_ = std::make_shared<PipelineLayout>(device_);
    canvasPipelineLayout_->setLayoutCount_ = static_cast<uint32_t>(descriptorSetLayouts.size());
    canvasPipelineLayout_->pSetLayouts_ = descriptorSetLayouts.data();
    canvasPipelineLayout_->init();

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    canvasPipeline_ = std::make_shared<Pipeline>(device_);
    canvasPipeline_->stageCount_ = shaderStages.size();
    canvasPipeline_->pStages_ = shaderStages.data();
    canvasPipeline_->pVertexInputState_ = &vertexInputInfo;
    canvasPipeline_->pInputAssemblyState_ = &inputAssemblyInfo;
    canvasPipeline_->pViewportState_ = &viewportInfo;
    canvasPipeline_->pRasterizationState_ = &rasterizaInfo;;
    canvasPipeline_->pMultisampleState_ = &multipleInfo;
    canvasPipeline_->pDepthStencilState_ = &depthStencilInfo;
    canvasPipeline_->pColorBlendState_ = &colorBlendInfo;
    canvasPipeline_->pDynamicState_ = &dynamicInfo;
    canvasPipeline_->layout_ = canvasPipelineLayout_->pipelineLayout();
    canvasPipeline_->renderPass_ = renderPass_->renderPass();
    canvasPipeline_->init();

    canvasPipeline_->descriptorSets_["canvas"] = canvasDescriptorSets_;
}

void Vulkan::createCursorPipeline() {
    auto vertCanvas = ShaderModule(device_, "../shaders/spv/CursorVert.spv");
    auto fragCanvas = ShaderModule(device_, "../shaders/spv/CursorFrag.spv");

    VkPipelineShaderStageCreateInfo vertexStageInfo{}, fragmentStageInfo{};
    vertexStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStageInfo.module = vertCanvas.shader();
    vertexStageInfo.pName = "main";

    fragmentStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentStageInfo.module = fragCanvas.shader();
    fragmentStageInfo.pName = "main";

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages{vertexStageInfo, fragmentStageInfo};
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    auto bindingDescription = canvas_->bindingDescription(0);
    auto attributeDescription = canvas_->attributeDescription(0);

    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescription.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
    inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.topology = canvas_->topology();

    VkPipelineViewportStateCreateInfo viewportInfo{};
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.viewportCount = 1;
    viewportInfo.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizaInfo{};
    rasterizaInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizaInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizaInfo.cullMode = VK_CULL_MODE_NONE;
    rasterizaInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizaInfo.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multipleInfo{};
    multipleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multipleInfo.rasterizationSamples = msaaSamples_;
    multipleInfo.minSampleShading = 1.0f;
    
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
    depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilInfo.depthTestEnable = VK_TRUE;
    depthStencilInfo.depthWriteEnable = VK_TRUE;
    depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilInfo.minDepthBounds = 0.0f;
    depthStencilInfo.maxDepthBounds = 1.0f;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentInfo{};
    colorBlendAttachmentInfo.blendEnable = VK_TRUE;
    colorBlendAttachmentInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachmentInfo.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    colorBlendAttachmentInfo.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    colorBlendAttachmentInfo.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentInfo.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentInfo.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentInfo.alphaBlendOp = VK_BLEND_OP_ADD;
    
    VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
    colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendInfo.attachmentCount = 1;
    colorBlendInfo.pAttachments = &colorBlendAttachmentInfo;

    std::vector<VkDynamicState> dynamics{
        VK_DYNAMIC_STATE_VIEWPORT, 
        VK_DYNAMIC_STATE_SCISSOR, 
    };
    VkPipelineDynamicStateCreateInfo dynamicInfo{};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = static_cast<uint32_t>(dynamics.size());
    dynamicInfo.pDynamicStates = dynamics.data();

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {canvasDescriptorSetLayout_->descriptorSetLayout()};
    cursorPipelineLayout_ = std::make_shared<PipelineLayout>(device_);
    cursorPipelineLayout_->setLayoutCount_ = static_cast<uint32_t>(descriptorSetLayouts.size());
    cursorPipelineLayout_->pSetLayouts_ = descriptorSetLayouts.data();
    cursorPipelineLayout_->init();

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    cursorPipeline_ = std::make_shared<Pipeline>(device_);
    cursorPipeline_->stageCount_ = shaderStages.size();
    cursorPipeline_->pStages_ = shaderStages.data();
    cursorPipeline_->pVertexInputState_ = &vertexInputInfo;
    cursorPipeline_->pInputAssemblyState_ = &inputAssemblyInfo;
    cursorPipeline_->pViewportState_ = &viewportInfo;
    cursorPipeline_->pRasterizationState_ = &rasterizaInfo;;
    cursorPipeline_->pMultisampleState_ = &multipleInfo;
    cursorPipeline_->pDepthStencilState_ = &depthStencilInfo;
    cursorPipeline_->pColorBlendState_ = &colorBlendInfo;
    cursorPipeline_->pDynamicState_ = &dynamicInfo;
    cursorPipeline_->layout_ = cursorPipelineLayout_->pipelineLayout();
    cursorPipeline_->renderPass_ = renderPass_->renderPass();
    cursorPipeline_->init();

    cursorPipeline_->descriptorSets_["cursor"] = cursorDescriptorSet_;
}

void Vulkan::createColorResource() {
    colorImage_ = std::make_shared<Image>(physicalDevice_, device_);
    colorImage_->imageType_ = VK_IMAGE_TYPE_2D;
    colorImage_->format_ = swapChain_->format();
    colorImage_->extent_ = {swapChain_->extent().width, swapChain_->extent().height, 1};
    colorImage_->mipLevles_ = 1;
    colorImage_->arrayLayers_ = 1;
    colorImage_->samples_ = msaaSamples_;
    colorImage_->tiling_ = VK_IMAGE_TILING_OPTIMAL;
    colorImage_->usage_ = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    colorImage_->sharingMode_ = queueFamilies_.multiple() ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
    colorImage_->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
    colorImage_->pQueueFamilyIndices_ = queueFamilies_.sets().data();
    colorImage_->viewType_ = VK_IMAGE_VIEW_TYPE_2D;
    colorImage_->subresourcesRange_ = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    colorImage_->memoryProperties_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    colorImage_->init();
}

void Vulkan::createDepthResource() {
    depthImage_ = std::make_shared<Image>(physicalDevice_, device_);
    depthImage_->imageType_ = VK_IMAGE_TYPE_2D;
    depthImage_->format_ = findDepthFormat();
    depthImage_->mipLevles_ = 1;
    depthImage_->arrayLayers_ = 1;
    depthImage_->samples_ = msaaSamples_;
    depthImage_->extent_ = {swapChain_->extent().width, swapChain_->extent().height, 1};
    depthImage_->tiling_ = VK_IMAGE_TILING_OPTIMAL;
    depthImage_->usage_ = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthImage_->sharingMode_ = queueFamilies_.multiple() ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
    depthImage_->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
    depthImage_->pQueueFamilyIndices_ = queueFamilies_.sets().data();
    depthImage_->viewType_ = VK_IMAGE_VIEW_TYPE_2D;
    depthImage_->subresourcesRange_ = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    depthImage_->memoryProperties_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    depthImage_->init();
}

void Vulkan::createFrameBuffer() {
    frameBuffers_.resize(swapChain_->size());
    
    for (size_t i = 0; i < frameBuffers_.size(); i++) {
        std::vector<VkImageView> attachment = {
            colorImage_->view(), 
            swapChain_->imageView(i), 
            depthImage_->view(),   
        };

        frameBuffers_[i] = std::make_shared<FrameBuffer>(device_);
        frameBuffers_[i]->renderPass_ = renderPass_->renderPass();
        frameBuffers_[i]->width_ = swapChain_->width();
        frameBuffers_[i]->height_ = swapChain_->height();
        frameBuffers_[i]->layers_ = 1;
        frameBuffers_[i]->attachmentCount_ = static_cast<uint32_t>(attachment.size());
        frameBuffers_[i]->pAttachments_ = attachment.data();
        frameBuffers_[i]->init();
    }
}

void Vulkan::createCommandPool() {
    commandPool_ = std::make_shared<CommandPool>(device_);
    commandPool_->flags_ = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPool_->queueFamilyIndex_ = queueFamilies_.graphics.value();
    commandPool_->init();
}

void Vulkan::createCommandBuffers() {
    commandBuffers_ = std::make_shared<CommandBuffer>(device_);
    commandBuffers_->commandPool_ = commandPool_->commanddPool();
    commandBuffers_->level_ = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBuffers_->commandBufferCount_ = 1;
    commandBuffers_->init();
}

void Vulkan::createVertexBuffer() {
    {
        VkDeviceSize size = sizeof(canvasVertices_[0]) * canvasVertices_.size();

        canvasVertexBuffer_ = std::make_shared<Buffer>(physicalDevice_, device_);
        canvasVertexBuffer_->size_ = size;
        canvasVertexBuffer_->usage_ = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        canvasVertexBuffer_->sharingMode_ = queueFamilies_.multiple() ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
        canvasVertexBuffer_->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
        canvasVertexBuffer_->pQueueFamilyIndices_ = queueFamilies_.sets().data();
        canvasVertexBuffer_->memoryProperties_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        canvasVertexBuffer_->init();

        std::shared_ptr<Buffer> staginBuffer = std::make_shared<Buffer>(physicalDevice_, device_);
        staginBuffer->size_ = size;
        staginBuffer->usage_ = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        staginBuffer->sharingMode_ = queueFamilies_.multiple() ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
        staginBuffer->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
        staginBuffer->pQueueFamilyIndices_ = queueFamilies_.sets().data();
        staginBuffer->memoryProperties_ = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        staginBuffer->init();

        auto data = staginBuffer->map(size);
        memcpy(data, canvasVertices_.data(), size);
        staginBuffer->unMap();

        copyBuffer(staginBuffer->buffer(), canvasVertexBuffer_->buffer(), size);
    }
}

void Vulkan::createIndexBuffer() {
    {
        VkDeviceSize size = sizeof(canvasIndices_[0]) * canvasIndices_.size();

        canvasIndexBuffer_ = std::make_shared<Buffer>(physicalDevice_, device_);
        canvasIndexBuffer_->size_ = size;
        canvasIndexBuffer_->usage_ = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        canvasIndexBuffer_->sharingMode_ = queueFamilies_.multiple() ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
        canvasIndexBuffer_->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
        canvasIndexBuffer_->pQueueFamilyIndices_ = queueFamilies_.sets().data();
        canvasIndexBuffer_->memoryProperties_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        canvasIndexBuffer_->init();
        canvasIndexBuffer_->setCount(canvasIndices_.size());

        std::shared_ptr<Buffer> staginBuffer = std::make_shared<Buffer>(physicalDevice_, device_);
        staginBuffer->size_ = size;
        staginBuffer->usage_ = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        staginBuffer->sharingMode_ = queueFamilies_.multiple() ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
        staginBuffer->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
        staginBuffer->pQueueFamilyIndices_ = queueFamilies_.sets().data();
        staginBuffer->memoryProperties_ = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        staginBuffer->init();

        auto data = staginBuffer->map(size);
        memcpy(data, canvasIndices_.data(), size);
        staginBuffer->unMap();

        copyBuffer(staginBuffer->buffer(), canvasIndexBuffer_->buffer(), size);
    }
}

void Vulkan::createSyncObjects() {
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    inFlightFences_ = std::make_shared<Fence>(device_);
    inFlightFences_->flags_ = VK_FENCE_CREATE_SIGNALED_BIT;
    inFlightFences_->init();
    imageAvaiableSemaphores_ = std::make_shared<Semaphore>(device_);
    imageAvaiableSemaphores_->init();
    renderFinishSemaphores_ = std::make_shared<Semaphore>(device_);
    renderFinishSemaphores_->init();
}

void Vulkan::mergeResource() {
    renderTargets_["text"] = std::make_shared<RenderTarget>(fontPipeline_);
    renderTargets_["lineNumber"] = std::make_shared<RenderTarget>(fontPipeline_);
    renderTargets_["cmd"] = std::make_shared<RenderTarget>(fontPipeline_);
    renderTargets_["mode"] = std::make_shared<RenderTarget>(fontPipeline_);
    renderTargets_["cursor"] = std::make_shared<RenderTarget>(cursorPipeline_);
    renderTargets_["canvas"] = std::make_shared<RenderTarget>(canvasPipeline_);    
}

void Vulkan::recordCommadBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo commandBufferBeginInfo{};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin command buffer!");
    }

    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = renderPass_->renderPass();
    renderPassBeginInfo.framebuffer = frameBuffers_[imageIndex]->frameBuffer();
    renderPassBeginInfo.renderArea.extent = swapChain_->extent();
    std::vector<VkClearValue> clearValues(3);
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 0.0f};
    clearValues[1].color = {0.0f, 0.0f, 0.0f, 0.0f};
    clearValues[2].depthStencil = {1.0f, 0};
    renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassBeginInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = static_cast<float>(swapChain_->extent().height);
        viewport.width = static_cast<float>(swapChain_->extent().width);
        viewport.height = -static_cast<float>(swapChain_->extent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{};
        scissor.extent = swapChain_->extent();
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        VkDeviceSize offsets[] = {0};
        if (fontPipeline_->pipeline() == canvasPipeline_->pipeline()) {
            std::cout << "equal" << std::endl;
        }

        // line number
        if (lineNumber_->wordCount_ > 0) {
            renderTargets_["lineNumber"]->render(commandBuffer, lineNumberVertexBuffer_, lineNumberIndexBuffer_);
        }

        // text
        if (!editor_->empty()) {
            renderTargets_["text"]->render(commandBuffer, textVertexBuffer_, textIndexBuffer_);
        }

        // command
        if (editor_->mode_ == Editor::Mode::Command) {
            renderTargets_["cmd"]->render(commandBuffer, cmdVertexBuffer_, cmdIndexBuffer_);
        }

        // mode name
        if (!currModeName_.empty()) {
            renderTargets_["mode"]->render(commandBuffer, modeVertexBuffer_, modeIndexBuffer_);
        }

        // cursor
        // if (editor_->mode_ != Editor::Mode::General) {
            renderTargets_["cursor"]->render(commandBuffer, cursorVertexBuffer_, cursorIndexBuffer_);
        // }

        // Canvas
        renderTargets_["canvas"]->render(commandBuffer, canvasVertexBuffer_, canvasIndexBuffer_);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to end command buffer!");
    }
}

void Vulkan::draw() {
    timer_.tick();
    camera_->setDeltaTime(timer_.deltaMilliseconds());


    uint32_t imageIndex = 0;
    vkAcquireNextImageKHR(device_, swapChain_->swapChain(), UINT64_MAX, imageAvaiableSemaphores_->semaphore(), VK_NULL_HANDLE, &imageIndex);

    vkResetFences(device_, 1, inFlightFences_->fencePtr());

    vkResetCommandBuffer(commandBuffers_->commandBuffer(), 0);

    updateDrawAssets();

    recordCommadBuffer(commandBuffers_->commandBuffer(), imageIndex);

    std::vector<VkSemaphore> waits = {imageAvaiableSemaphores_->semaphore()};
    std::vector<VkPipelineStageFlags> waitStages = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    std::vector<VkCommandBuffer> commandBuffers = {commandBuffers_->commandBuffer()};
    std::vector<VkSemaphore> signals = {renderFinishSemaphores_->semaphore()};

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waits.size());
    submitInfo.pWaitSemaphores = waits.data();
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
    submitInfo.pCommandBuffers = commandBuffers.data();
    submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signals.size());
    submitInfo.pSignalSemaphores = signals.data();

    if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFences_->fence()) != VK_SUCCESS) {
        throw std::runtime_error("failed to queue submit!");
    }

    std::vector<VkSwapchainKHR> swapChains = {swapChain_->swapChain()};

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = static_cast<uint32_t>(swapChains.size());
    presentInfo.pSwapchains = swapChains.data();
    presentInfo.waitSemaphoreCount = static_cast<uint32_t>(signals.size());
    presentInfo.pWaitSemaphores = signals.data();
    presentInfo.pImageIndices = &imageIndex;

    auto result = vkQueuePresentKHR(presentQueue_, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return ;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("faield to present!");
    }

    vkWaitForFences(device_, 1, inFlightFences_->fencePtr(), VK_TRUE, UINT64_MAX);
}

void Vulkan::loadAssets() {
    loadTextures();
    loadChars();
}

void Vulkan::loadTextures() {
    for (auto& textureName :  textureNames_) {
        auto fullPath = "../textures/" + textureName;

        int texWidth, texHeight, texChannels;
        auto pixel = stbi_load(fullPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        auto mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight))) + 1);

        if (!pixel) {
            std::cout << "failed to load texture" << std::endl;
            continue;
        }

        VkDeviceSize size = texWidth * texHeight * 4;

        auto canvasImage = std::make_shared<Image>(physicalDevice_, device_);
        canvasImage->imageType_ = VK_IMAGE_TYPE_2D;
        canvasImage->arrayLayers_ = 1;
        canvasImage->mipLevles_ = mipLevels;
        canvasImage->format_ = VK_FORMAT_R8G8B8A8_SRGB;
        canvasImage->extent_ = {static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1};
        canvasImage->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
        canvasImage->pQueueFamilyIndices_ = queueFamilies_.sets().data();
        canvasImage->sharingMode_ = queueFamilies_.multiple() ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
        canvasImage->tiling_ = VK_IMAGE_TILING_OPTIMAL;
        canvasImage->usage_ = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        canvasImage->memoryProperties_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        canvasImage->viewType_ = VK_IMAGE_VIEW_TYPE_2D;
        canvasImage->samples_ = VK_SAMPLE_COUNT_1_BIT;
        canvasImage->subresourcesRange_ = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1};
        canvasImage->init();
        
        std::shared_ptr<Buffer> staginBuffer = std::make_shared<Buffer>(physicalDevice_, device_);
        staginBuffer->size_ = size;
        staginBuffer->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
        staginBuffer->pQueueFamilyIndices_ = queueFamilies_.sets().data();
        staginBuffer->sharingMode_ = queueFamilies_.multiple() ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
        staginBuffer->usage_ = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        staginBuffer->memoryProperties_ = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        staginBuffer->init();
        
        auto data = staginBuffer->map(size);
        memcpy(data, pixel, size);
        staginBuffer->unMap();

        VkImageSubresourceRange range{1, 0, mipLevels, 0, 1};
        VkBufferImageCopy region{};
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1};
        region.imageSubresource = {1, 0, 0, 1};

        auto cmdBuffer = beginSingleTimeCommands();
            Tools::setImageLayout(cmdBuffer, canvasImage->image(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, range, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            vkCmdCopyBufferToImage(cmdBuffer, staginBuffer->buffer(), canvasImage->image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        endSingleTimeCommands(cmdBuffer, transferQueue_);

        generateMipmaps(canvasImage->image(), VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, mipLevels);

            // Tools::setImageLayout(cmdBuffer, canvasImage->image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, range, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            
        canvasMipLevels_.insert({textureName, mipLevels});
        canvasImages_.insert({textureName, std::move(canvasImage)});
    }
}

void Vulkan::generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(physicalDevice_, imageFormat, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    endSingleTimeCommands(commandBuffer, graphicsQueue_);
}

void Vulkan::loadChars() {
    font_ = std::make_shared<Font>(fontPath_.c_str(), 20);

    uint32_t height = 0;
    for (uint32_t i = 0; i < 128; i++) {
        char c = static_cast<char>(i);
        font_->loadChar(c);
        auto& currentChar = dictionary_[c];

        auto pixel = font_->bitmap().buffer;
        auto texWidth = font_->bitmap().width;
        auto texHeight = font_->bitmap().rows;
        auto offsetX = font_->glyph()->bitmap_left;
        auto offsetY = font_->glyph()->bitmap_top;
        auto advance = font_->glyph()->advance.x / 64;
        VkDeviceSize size = texWidth * texHeight * 1;
        height = std::max(height, static_cast<uint32_t>(texHeight));

        if (pixel == nullptr) {
            texWidth = dictionary_.at(static_cast<char>(0)).width_;
            texHeight = dictionary_.at(static_cast<char>(0)).height_;
            offsetX = dictionary_.at(static_cast<char>(0)).offsetX_;
            offsetY = dictionary_.at(static_cast<char>(0)).offsetY_;
            advance = dictionary_.at(static_cast<char>(0)).advance_;
            size = texWidth * texHeight * 1;
        }

        currentChar.char_ = c;
        currentChar.offsetX_ = offsetX;
        currentChar.offsetY_ = offsetY;
        currentChar.width_ = texWidth;
        currentChar.height_ = texHeight;
        currentChar.advance_ = advance;
        currentChar.color_ = glm::vec3(1.0f, 1.0f, 1.0f);
        currentChar.index_ = i;

        auto& image = currentChar.image_;        
        image = std::make_shared<Image>(physicalDevice_, device_);
        image->imageType_ = VK_IMAGE_TYPE_2D;
        image->arrayLayers_ = 1;
        image->mipLevles_ = 1;
        image->format_ = VK_FORMAT_R8_UNORM;
        image->extent_ = {texWidth, texHeight, 1};
        image->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
        image->pQueueFamilyIndices_ = queueFamilies_.sets().data();
        image->sharingMode_ = queueFamilies_.multiple() ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
        image->tiling_ = VK_IMAGE_TILING_OPTIMAL;
        image->usage_ = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        image->memoryProperties_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        image->viewType_ = VK_IMAGE_VIEW_TYPE_2D;
        image->samples_ = VK_SAMPLE_COUNT_1_BIT;
        image->subresourcesRange_ = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        image->init();

        Buffer staginBuffer(physicalDevice_, device_);
        staginBuffer.size_ = size;
        staginBuffer.queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
        staginBuffer.pQueueFamilyIndices_ = queueFamilies_.sets().data();
        staginBuffer.sharingMode_ = queueFamilies_.multiple() ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
        staginBuffer.usage_ = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        staginBuffer.memoryProperties_ = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        staginBuffer.init();

        auto data = staginBuffer.map(size);
        if (pixel == nullptr) {
            memset(data, 0, size);
        } else {
            memcpy(data, pixel, size);
        }
        staginBuffer.unMap();

        VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkBufferImageCopy region{};
        region.imageExtent = {texWidth, texHeight, 1};
        region.imageSubresource = {1, 0, 0, 1};

        auto cmdBuffer = beginSingleTimeCommands();
            Tools::setImageLayout(cmdBuffer, image->image(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, range, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            vkCmdCopyBufferToImage(cmdBuffer, staginBuffer.buffer(), image->image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
            Tools::setImageLayout(cmdBuffer, image->image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, range, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        endSingleTimeCommands(cmdBuffer, transferQueue_);
    }
    font_->advance_ = dictionary_['a'].advance_;
    font_->lineHeight_ = height;
}

void Vulkan::updateDrawAssets() {
    UniformBufferObject ubo{};
    ubo.proj_ = glm::ortho(-static_cast<float>(swapChain_->width()) / 2.0f, static_cast<float>(swapChain_->width()) / 2.0f, -static_cast<float>(swapChain_->height()) / 2.0f, static_cast<float>(swapChain_->height()) / 2.0f);
    auto data = uniformBuffers_->map(sizeof(ubo));    
    memcpy(data, &ubo, sizeof(ubo));
    
    // text
    {   
        // auto s = Timer::nowMilliseconds();
        if (!editor_->empty()) {
            // std::cout << editor_->limit_.up_ << ", " << editor_->limit_.bottom_ << std::endl;
            std::pair<std::vector<Font::Point>, std::vector<uint32_t>> animationPoints;

            for (auto it = textAnimations_.begin(); it != textAnimations_.end(); ) {
                if (it->finish()) {
                    it->callback();
                    it = textAnimations_.erase(it);
                    continue;
                } else {
                    auto vertices = it->genCurrentState();
                    it->step();
                    std::vector<uint32_t> indices = {0, 1, 2, 2, 1, 3};

                    animationPoints = Font::merge(animationPoints, {vertices, indices});
                    ++it;
                }
            }

            auto limit = editor_->showLimit();

            auto text = std::vector<std::string>(editor_->lines_.begin() + limit.up_, editor_->lines_.begin() + limit.bottom_);
            // for (auto& s : text) {
            //     std::cout << s << std::endl;
            // }
            // auto s = Timer::nowMilliseconds();
            glm::ivec2 xy;
            xy.x = -static_cast<float>(swapChain_->width()) / 2.0f + lineNumber_->lineNumberOffset_ * font_->advance_;
            xy.y = static_cast<float>(swapChain_->height()) / 2.0f - editor_->lineHeight_;

            auto t = font_->genTextLines(xy.x, xy.y, editor_->lineHeight_, text, dictionary_, grammar_.get());
            t = Font::merge(t, animationPoints);
            // std::cout << std::format("generate vertices ms: {}\n", e - s);
            textVertices_ = t.first;
            textIndices_ = t.second;

            // font vertices
            VkDeviceSize size = sizeof(textVertices_[0]) * textVertices_.size();

            textVertexBuffer_->size_ = size;
            textVertexBuffer_->usage_ = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            textVertexBuffer_->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
            textVertexBuffer_->pQueueFamilyIndices_ = queueFamilies_.sets().data();
            textVertexBuffer_->sharingMode_ = queueFamilies_.sharingMode();
            textVertexBuffer_->memoryProperties_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            textVertexBuffer_->init();

            auto staginBuffer = std::make_shared<Buffer>(physicalDevice_, device_);
            staginBuffer->size_ = size;
            staginBuffer->usage_ = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            staginBuffer->sharingMode_ = VK_SHARING_MODE_EXCLUSIVE;
            staginBuffer->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
            staginBuffer->pQueueFamilyIndices_ = queueFamilies_.sets().data();
            staginBuffer->memoryProperties_ = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            staginBuffer->init();

            auto data = staginBuffer->map(size);
            memcpy(data, textVertices_.data(), size);
            staginBuffer->unMap();

            copyBuffer(staginBuffer->buffer(), textVertexBuffer_->buffer(), size);

            // font index
            size = sizeof(textIndices_[0]) * textIndices_.size();

            textIndexBuffer_->size_ = size;
            textIndexBuffer_->usage_ = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            textIndexBuffer_->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
            textIndexBuffer_->pQueueFamilyIndices_ = queueFamilies_.sets().data();
            textIndexBuffer_->sharingMode_ = queueFamilies_.sharingMode();
            textIndexBuffer_->memoryProperties_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            textIndexBuffer_->init();
            textIndexBuffer_->setCount(textIndices_.size());

            staginBuffer = std::make_shared<Buffer>(physicalDevice_, device_);
            staginBuffer->size_ = size;
            staginBuffer->usage_ = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            staginBuffer->sharingMode_ = VK_SHARING_MODE_EXCLUSIVE;
            staginBuffer->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
            staginBuffer->pQueueFamilyIndices_ = queueFamilies_.sets().data();
            staginBuffer->memoryProperties_ = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            staginBuffer->init();

            data = staginBuffer->map(size);
            memcpy(data, textIndices_.data(), size);
            staginBuffer->unMap();

            copyBuffer(staginBuffer->buffer(), textIndexBuffer_->buffer(), size);
        }
        // auto e = Timer::nowMilliseconds();
        // std::cout << std::format("create text buffer ms: {}\n", e - s); 

        if (lineNumber_->wordCount_ > 0) {
            // std::cout << lineNumber_->limit_.up_ << ", " << lineNumber_->limit_.bottom_ << std::endl;
            auto text = std::vector<std::string>(lineNumber_->lines_.begin() + lineNumber_->showLimit().up_, lineNumber_->lines_.begin() + lineNumber_->showLimit().bottom_);
            auto t = font_->genTextLines(-static_cast<float>(swapChain_->width()) / 2.0f, static_cast<float>(swapChain_->height()) / 2.0f - lineNumber_->lineHeight_, editor_->lineHeight_, text, dictionary_, grammar_.get());

            lineNumberVertices_ = t.first;
            lineNumberIndices_ = t.second;

            // linenumber vertices
            VkDeviceSize size = sizeof(lineNumberVertices_[0]) * lineNumberVertices_.size();

            lineNumberVertexBuffer_->size_ = size;
            lineNumberVertexBuffer_->usage_ = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            lineNumberVertexBuffer_->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
            lineNumberVertexBuffer_->pQueueFamilyIndices_ = queueFamilies_.sets().data();
            lineNumberVertexBuffer_->sharingMode_ = queueFamilies_.sharingMode();
            lineNumberVertexBuffer_->memoryProperties_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            lineNumberVertexBuffer_->init();

            auto staginBuffer = std::make_shared<Buffer>(physicalDevice_, device_);
            staginBuffer->size_ = size;
            staginBuffer->usage_ = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            staginBuffer->sharingMode_ = VK_SHARING_MODE_EXCLUSIVE;
            staginBuffer->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
            staginBuffer->pQueueFamilyIndices_ = queueFamilies_.sets().data();
            staginBuffer->memoryProperties_ = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            staginBuffer->init();

            auto data = staginBuffer->map(size);
            memcpy(data, lineNumberVertices_.data(), size);
            staginBuffer->unMap();

            copyBuffer(staginBuffer->buffer(), lineNumberVertexBuffer_->buffer(), size);

            // linenumber index
            size = sizeof(lineNumberIndices_[0]) * lineNumberIndices_.size();

            lineNumberIndexBuffer_->size_ = size;
            lineNumberIndexBuffer_->usage_ = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            lineNumberIndexBuffer_->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
            lineNumberIndexBuffer_->pQueueFamilyIndices_ = queueFamilies_.sets().data();
            lineNumberIndexBuffer_->sharingMode_ = queueFamilies_.sharingMode();
            lineNumberIndexBuffer_->memoryProperties_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            lineNumberIndexBuffer_->init();
            lineNumberIndexBuffer_->setCount(lineNumberIndices_.size());

            staginBuffer = std::make_shared<Buffer>(physicalDevice_, device_);
            staginBuffer->size_ = size;
            staginBuffer->usage_ = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            staginBuffer->sharingMode_ = VK_SHARING_MODE_EXCLUSIVE;
            staginBuffer->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
            staginBuffer->pQueueFamilyIndices_ = queueFamilies_.sets().data();
            staginBuffer->memoryProperties_ = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            staginBuffer->init();

            data = staginBuffer->map(size);
            memcpy(data, lineNumberIndices_.data(), size);
            staginBuffer->unMap();

            copyBuffer(staginBuffer->buffer(), lineNumberIndexBuffer_->buffer(), size);
        }

        // command
        if (editor_->mode_ == Editor::Mode::Command) {
            glm::ivec2 xy;
            xy.x = -static_cast<float>(swapChain_->width()) / 2.0f;
            xy.y = -static_cast<float>(swapChain_->height()) / 2.0f + static_cast<float>(commandLine_->lineHeight_) / 2.0f;
            auto t = font_->genTextLine(xy.x, xy.y, commandLine_->onlyLine_, dictionary_, grammar_.get());
            // std::cout << std::format("generate vertices ms: {}\n", e - s);
            cmdVertices_ = t.first;
            cmdIndices_ = t.second;

            // font vertices
            VkDeviceSize size = sizeof(cmdVertices_[0]) * cmdVertices_.size();

            cmdVertexBuffer_->size_ = size;
            cmdVertexBuffer_->usage_ = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            cmdVertexBuffer_->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
            cmdVertexBuffer_->pQueueFamilyIndices_ = queueFamilies_.sets().data();
            cmdVertexBuffer_->sharingMode_ = queueFamilies_.sharingMode();
            cmdVertexBuffer_->memoryProperties_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            cmdVertexBuffer_->init();

            auto staginBuffer = std::make_shared<Buffer>(physicalDevice_, device_);
            staginBuffer->size_ = size;
            staginBuffer->usage_ = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            staginBuffer->sharingMode_ = VK_SHARING_MODE_EXCLUSIVE;
            staginBuffer->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
            staginBuffer->pQueueFamilyIndices_ = queueFamilies_.sets().data();
            staginBuffer->memoryProperties_ = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            staginBuffer->init();

            auto data = staginBuffer->map(size);
            memcpy(data, cmdVertices_.data(), size);
            staginBuffer->unMap();

            copyBuffer(staginBuffer->buffer(), cmdVertexBuffer_->buffer(), size);

            // font index
            size = sizeof(cmdIndices_[0]) * cmdIndices_.size();

            cmdIndexBuffer_->size_ = size;
            cmdIndexBuffer_->usage_ = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            cmdIndexBuffer_->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
            cmdIndexBuffer_->pQueueFamilyIndices_ = queueFamilies_.sets().data();
            cmdIndexBuffer_->sharingMode_ = queueFamilies_.sharingMode();
            cmdIndexBuffer_->memoryProperties_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            cmdIndexBuffer_->init();
            cmdIndexBuffer_->setCount(cmdIndices_.size());

            staginBuffer = std::make_shared<Buffer>(physicalDevice_, device_);
            staginBuffer->size_ = size;
            staginBuffer->usage_ = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            staginBuffer->sharingMode_ = VK_SHARING_MODE_EXCLUSIVE;
            staginBuffer->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
            staginBuffer->pQueueFamilyIndices_ = queueFamilies_.sets().data();
            staginBuffer->memoryProperties_ = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            staginBuffer->init();

            data = staginBuffer->map(size);
            memcpy(data, cmdIndices_.data(), size);
            staginBuffer->unMap();

            copyBuffer(staginBuffer->buffer(), cmdIndexBuffer_->buffer(), size);
        }

        // mode name
        {
            switch (editor_->mode_) {
                case Editor::Mode::General:
                    currModeName_ = "General";
                    break;
                case Editor::Mode::Command:
                    currModeName_ = "Command";
                    break;
                case Editor::Mode::Insert:
                    currModeName_ = "Insert";
                    break; 
                default:
                    currModeName_ = "Unknown";
            }

            glm::ivec2 xy;
            xy.x = static_cast<float>(swapChain_->width()) / 2.0f - currModeName_.size() * font_->advance_;
            xy.y = -static_cast<float>(swapChain_->height()) / 2.0f + static_cast<float>(commandLine_->lineHeight_) / 2.0f;
            auto t = font_->genTextLine(xy.x, xy.y, currModeName_, dictionary_, nullptr);

            modeVertices_ = t.first;
            modeIndices_ = t.second;

            // font vertices
            VkDeviceSize size = sizeof(modeVertices_[0]) * modeVertices_.size();

            modeVertexBuffer_->size_ = size;
            modeVertexBuffer_->usage_ = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            modeVertexBuffer_->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
            modeVertexBuffer_->pQueueFamilyIndices_ = queueFamilies_.sets().data();
            modeVertexBuffer_->sharingMode_ = queueFamilies_.sharingMode();
            modeVertexBuffer_->memoryProperties_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            modeVertexBuffer_->init();

            auto staginBuffer = std::make_shared<Buffer>(physicalDevice_, device_);
            staginBuffer->size_ = size;
            staginBuffer->usage_ = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            staginBuffer->sharingMode_ = VK_SHARING_MODE_EXCLUSIVE;
            staginBuffer->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
            staginBuffer->pQueueFamilyIndices_ = queueFamilies_.sets().data();
            staginBuffer->memoryProperties_ = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            staginBuffer->init();

            auto data = staginBuffer->map(size);
            memcpy(data, modeVertices_.data(), size);
            staginBuffer->unMap();

            copyBuffer(staginBuffer->buffer(), modeVertexBuffer_->buffer(), size);

            // font index
            size = sizeof(modeIndices_[0]) * modeIndices_.size();

            modeIndexBuffer_->size_ = size;
            modeIndexBuffer_->usage_ = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            modeIndexBuffer_->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
            modeIndexBuffer_->pQueueFamilyIndices_ = queueFamilies_.sets().data();
            modeIndexBuffer_->sharingMode_ = queueFamilies_.sharingMode();
            modeIndexBuffer_->memoryProperties_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            modeIndexBuffer_->init();
            modeIndexBuffer_->setCount(modeIndices_.size());

            staginBuffer = std::make_shared<Buffer>(physicalDevice_, device_);
            staginBuffer->size_ = size;
            staginBuffer->usage_ = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            staginBuffer->sharingMode_ = VK_SHARING_MODE_EXCLUSIVE;
            staginBuffer->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
            staginBuffer->pQueueFamilyIndices_ = queueFamilies_.sets().data();
            staginBuffer->memoryProperties_ = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            staginBuffer->init();

            data = staginBuffer->map(size);
            memcpy(data, modeIndices_.data(), size);
            staginBuffer->unMap();

            copyBuffer(staginBuffer->buffer(), modeIndexBuffer_->buffer(), size);
        }
    }

    // cursor
    {   
        // if (editor_->mode_ != Editor::Mode::General) {
            static glm::vec3 cursorColor(1.0f, 1.0f, 1.0f);
            static auto prevTime = Timer::nowMilliseconds();
            static unsigned long long deltaTime = 0;
            auto currTime = Timer::nowMilliseconds();
            deltaTime += currTime - prevTime;
            prevTime = currTime;
            if (deltaTime >= 500) {
                deltaTime = 0;
                if (cursorColor.x == 1.0f) {
                    cursorColor = glm::vec3(0.0f, 0.0f, 0.0f);
                } else {
                    cursorColor = glm::vec3(1.0f, 1.0f, 1.0f);
                }
            }

            glm::ivec2 xy;
            if (editor_->mode_ == Editor::Mode::Insert || editor_->mode_ == Editor::Mode::General) {
                xy = editor_->cursorRenderPos(lineNumber_->lineNumberOffset_ * font_->advance_, font_->advance_);
            } else {
                xy = commandLine_->cursorRenderPos(font_->advance_);
            }
            auto t = canvas_->vertices(xy.x, xy.y, 2.0f, editor_->lineHeight_, cursorColor);
            cursorVertices_ = t.first;
            cursorIndices_ = t.second;

            VkDeviceSize size = sizeof(cursorVertices_[0]) * cursorVertices_.size();

            cursorVertexBuffer_->size_ = size;
            cursorVertexBuffer_->usage_ = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            cursorVertexBuffer_->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
            cursorVertexBuffer_->pQueueFamilyIndices_ = queueFamilies_.sets().data();
            cursorVertexBuffer_->sharingMode_ = queueFamilies_.sharingMode();
            cursorVertexBuffer_->memoryProperties_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            cursorVertexBuffer_->init();

            auto staginBuffer = std::make_shared<Buffer>(physicalDevice_, device_);
            staginBuffer->size_ = size;
            staginBuffer->usage_ = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            staginBuffer->sharingMode_ = VK_SHARING_MODE_EXCLUSIVE;
            staginBuffer->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
            staginBuffer->pQueueFamilyIndices_ = queueFamilies_.sets().data();
            staginBuffer->memoryProperties_ = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            staginBuffer->init();

            auto data = staginBuffer->map(size);
            memcpy(data, cursorVertices_.data(), size);
            staginBuffer->unMap();

            copyBuffer(staginBuffer->buffer(), cursorVertexBuffer_->buffer(), size);

            // font index
            size = sizeof(cursorIndices_[0]) * cursorIndices_.size();

            cursorIndexBuffer_->size_ = size;
            cursorIndexBuffer_->usage_ = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            cursorIndexBuffer_->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
            cursorIndexBuffer_->pQueueFamilyIndices_ = queueFamilies_.sets().data();
            cursorIndexBuffer_->sharingMode_ = queueFamilies_.sharingMode();
            cursorIndexBuffer_->memoryProperties_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            cursorIndexBuffer_->init();
            cursorIndexBuffer_->setCount(cursorIndices_.size());

            staginBuffer = std::make_shared<Buffer>(physicalDevice_, device_);
            staginBuffer->size_ = size;
            staginBuffer->usage_ = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            staginBuffer->sharingMode_ = VK_SHARING_MODE_EXCLUSIVE;
            staginBuffer->queueFamilyIndexCount_ = static_cast<uint32_t>(queueFamilies_.sets().size());
            staginBuffer->pQueueFamilyIndices_ = queueFamilies_.sets().data();
            staginBuffer->memoryProperties_ = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            staginBuffer->init();

            data = staginBuffer->map(size);
            memcpy(data, cursorIndices_.data(), size);
            staginBuffer->unMap();

            copyBuffer(staginBuffer->buffer(), cursorIndexBuffer_->buffer(), size);
        // }
    }
}   

void Vulkan::recreateSwapChain() {
    int width, height;
    glfwGetWindowSize(windows_, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetWindowSize(windows_, &width, &height);
        glfwPollEvents();
    }

    vkDeviceWaitIdle(device_);

    swapChain_.reset();

    createSwapChain();
    createColorResource();
    createDepthResource();
    createFrameBuffer();   
    createVertex();
    createVertexBuffer();
    createIndexBuffer();

    {
        auto data = canvasUniformBuffer_->map(sizeof(UniformBufferObject));
        UniformBufferObject ubo{};
        ubo.proj_ = glm::ortho(-static_cast<float>(swapChain_->width() / 2.0f), static_cast<float>(swapChain_->width() / 2.0f), -static_cast<float>(swapChain_->height() / 2.0), static_cast<float>(swapChain_->height() / 2.0f));
        memcpy(data, &ubo, sizeof(UniformBufferObject));
        canvasUniformBuffer_->unMap();
    }

    editor_->adjust(swapChain_->width(), swapChain_->height());
    lineNumber_->adjust(*editor_);
    commandLine_->adjust(*editor_);
}



bool Vulkan::checkValidationLayerSupport()  {
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> avaliables(count);
    vkEnumerateInstanceLayerProperties(&count, avaliables.data());
    
    std::unordered_set<const char*> requested(validationLayers_.begin(), validationLayers_.end());

    for (const auto& avaliable : avaliables) {
        requested.erase(avaliable.layerName);
    }
    
    return requested.empty();
}

bool Vulkan::checkDeviceExtensionsSupport(VkPhysicalDevice physicalDevice)  {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> avaliables(count);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, avaliables.data());

    std::unordered_set<std::string> requested(deviceExtensions_.begin(), deviceExtensions_.end());
    for (const auto& avaliable : avaliables) {
        requested.erase(avaliable.extensionName);
    }

    return requested.empty();
}

bool Vulkan::deviceSuitable(VkPhysicalDevice physicalDevice) {
    queueFamilies_ = Tools::QueueFamilyIndices::findQueueFamilies(physicalDevice, surface_);
    bool extensionSupport = checkDeviceExtensionsSupport(physicalDevice);
    bool swapChainAdequate = false;
    if (extensionSupport) {
        auto swapChainSupport = Tools::SwapChainSupportDetail::querySwapChainSupport(physicalDevice, surface_);
        swapChainAdequate = !swapChainSupport.formats_.empty() && !swapChainSupport.presentModes_.empty();
    }

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(physicalDevice, &features);

    return queueFamilies_.compeleted() && extensionSupport && swapChainAdequate && features.samplerAnisotropy;
}

VkSampleCountFlagBits Vulkan::getMaxUsableSampleCount()  {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice_, &properties);

    auto count = properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts;
    if (count & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (count & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (count & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (count & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (count & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (count & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

VkImageView Vulkan::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels)  {
    VkImageView imageView;
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.format = format;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;

    if (vkCreateImageView(device_, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to creat image view!");
    }

    return imageView;
}

VkFormat Vulkan::findDepthFormat() {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, 
        VK_IMAGE_TILING_OPTIMAL, 
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT  
    );
}

VkFormat Vulkan::findSupportedFormat(const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags features) {
    VkFormatProperties properties;
    for (const auto& format : formats) {
        vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &properties);
        if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find support format!");
}

uint32_t Vulkan::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);
    
    for (size_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & 1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void Vulkan::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    auto commandBuffer = beginSingleTimeCommands();
    
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer, transferQueue_);
}

VkCommandBuffer Vulkan::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandPool = commandPool_->commanddPool();
    allocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device_, &allocateInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void Vulkan::endSingleTimeCommands(VkCommandBuffer commandBuffer, VkQueue queue) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device_, commandPool_->commanddPool(), 1, &commandBuffer);
}

void Vulkan::updateTexture() {
    createCanvasDescriptorSet();
}

void Vulkan::input(int key, int scancode, int action, int mods) {
    auto currTime = Timer::nowMilliseconds();
    if (action == GLFW_RELEASE) {
        keyboard_->release(key);
    } else if (action == GLFW_PRESS) {
        keyboard_->press(key, currTime);
        processInput(key, scancode, mods);
    } else if (keyboard_->processKey(key, currTime)) {
        processInput(key, scancode, mods);
    }
}

void Vulkan::processInput(int key, int scancode, int mods) {
    switch (editor_->mode_) {
        case Editor::Mode::General:
            inputGeneral(key, scancode, mods);
            break;
        case Editor::Mode::Insert:
            inputInsert(key, scancode, mods);
            break;
        case Editor::Mode::Command:
            inputCommand(key, scancode, mods);
            break;
    }
}

void Vulkan::inputGeneral(int key, int scancode, int mods) {
    if (key == ';' && mods == GLFW_MOD_SHIFT) {
        editor_->mode_ = Editor::Mode::Command;
        return ;
    }

    if (key == 'I') {
        editor_->mode_ = Editor::Mode::Insert;
        return ;
    }

    editor_->moveCursor(static_cast<Editor::Direction>(key));
    lineNumber_->adjust(*editor_);
}

void Vulkan::inputInsert(int key, int scancode, int mods) {
    if (key == GLFW_KEY_ESCAPE) {
        editor_->mode_ = Editor::Mode::General;
        return ;
    }

    if (mods == GLFW_MOD_CONTROL) {
        if (key == 'C') {
            editor_->copyLine();
        }
        if (key == 'V') {
            editor_->paste();
        }
        if (key == 'X') {
            editor_->rmCopyLine();
        }
        if (key == GLFW_KEY_BACKSPACE) {
            editor_->rmSpaceOrWord();
        }
        if (key == GLFW_KEY_RIGHT) {
            editor_->moveRight();
        }
        if (key == GLFW_KEY_LEFT) {
            editor_->moveLeft();
        }
        if (key == GLFW_KEY_ENTER) {
            editor_->newLine();
        }

        lineNumber_->adjust(*editor_);
    } else {
        char c;
        if (key == GLFW_KEY_BACKSPACE) {
            editor_->backspace();
        } else if (key == GLFW_KEY_CAPS_LOCK) {
            capsLock_ ^= 1;
        } else if (key == GLFW_KEY_ENTER) {
            editor_->enter();
        } else if (key >= 0 && key <= 127) {
            if (mods == GLFW_MOD_SHIFT) {
                c = Tools::charToShiftChar(Tools::keyToChar(key));
            } else {
                if (!capsLock_ && key >= 'A' && key <= 'Z') {
                    key += 32; 
                }
                c = Tools::keyToChar(key);
            }
            if (key != 340) {
                {
                    glm::ivec2 leftDownPosition = {-static_cast<int32_t>(swapChain_->width()) / 2, -static_cast<int32_t>(swapChain_->height()) / 2};
                    glm::ivec2 endPosition = editor_->nextCharPosition(lineNumber_->lineNumberOffset_ * font_->advance_, font_->advance_);

                    auto begin = Font::genOneChar(leftDownPosition.x, leftDownPosition.y, editor_->lineHeight_, c, dictionary_);
                    auto end = Font::genOneChar(endPosition.x, endPosition.y, editor_->lineHeight_, c, dictionary_);
                    
                    auto animation = Animation<std::vector<Font::Point>>(begin, end, 100);
                    // animation.setFinishCallback([&, c]() {
                    //     editor_->insertChar(c);
                    // });

                    animation.addStage(begin, end, 100, nullptr);
                    textAnimations_.push_back(animation);
                }
                editor_->insertChar(c);
            }
        } else if (key == GLFW_KEY_TAB) {
            editor_->insertStr("    ");
        } else {
            editor_->moveCursor(static_cast<Editor::Direction>(key));
        }
    }

    lineNumber_->adjust(*editor_);
}

void Vulkan::inputCommand(int key, int scancode, int mods) {
    if (key == GLFW_KEY_ESCAPE) {
        commandLine_->clear();
        editor_->mode_ = Editor::Mode::General;
        return ;
    }
    
    char c;
    if (key == GLFW_KEY_BACKSPACE) {
        commandLine_->backspace();
    } else if (key == GLFW_KEY_CAPS_LOCK) {
        capsLock_ ^= 1;
    } else if (key == GLFW_KEY_ENTER) {
        auto cmd = commandLine_->enter();
        processCmd(cmd);
    } else if (key >= 0 && key <= 127) {
        if (mods == GLFW_MOD_SHIFT) {
            c = Tools::charToShiftChar(Tools::keyToChar(key));
        } else {
            if (!capsLock_ && key >= 'A' && key <= 'Z') {
                key += 32; 
            }
            c = Tools::keyToChar(key);
        }
        if (key != 340) {
            commandLine_->insertChar(c);
        }
    } else if (key == GLFW_KEY_TAB) {
        commandLine_->insertStr("    ");
    } else {
        commandLine_->moveCursor(static_cast<Editor::Direction>(key));
    }
}

void Vulkan::processCmd(std::string command) {
    command = Tools::rmFrontSpace(command);
    std::string cmd, arg;
    size_t i = 0;
    for ( ; i < command.size(); i++) {
        if (command[i] == ' ') {
            i++;
            break;
        }
        cmd += command[i];
    }

    if (i < command.size()) {
        arg = {command.begin() + i, command.end()};
    }
    arg = Tools::rmSpace(arg);

    if (cmd == "go") {
        auto number = std::stoi(arg) - 1;
        if (number >= 0 && number < editor_->lines_.size()) {
            editor_->setCursor({0, number});
            lineNumber_->adjust(*editor_);
            commandLine_->clear();
            
            editor_->setMode(Editor::Mode::General);
        }
    }

    if (cmd == "sys") {
        commandLine_->exectue(arg);
        commandLine_->clear();
    }

    if (cmd == "save") {
        if (!arg.empty()) {
            if (editor_->save(arg)) {
                commandLine_->clear();
            }
        } else {
            if (editor_->save()) {
                commandLine_->clear();
            }
        }
    }

    if (cmd == "find") {
        auto xy = editor_->searchStr(arg);
        if (xy.x != -1) {
            editor_->setCursor(xy);
            editor_->mode_ =Editor::Mode::Insert;
            lineNumber_->adjust(*editor_);
            commandLine_->clear();
        }
    }

    if (cmd == "open") {
        editor_->init(arg);
        lineNumber_->adjust(*editor_);
        commandLine_->clear();
    }

    if (cmd == "load") {
        if (canvasImages_.find(arg) != canvasImages_.end()) {
            canvasTextureName_ = arg;
            updateTexture();
            commandLine_->clear();
        }
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL Vulkan::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
        VkDebugUtilsMessageTypeFlagsEXT messageType, 
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, 
        void* pUserData) {

    std::cerr << "[[Validation Layer]]: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}
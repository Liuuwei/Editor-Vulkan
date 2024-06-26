#pragma once

#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Buffer.h"
#include "Fence.h"
#include "Grammar.h"
#include "Image.h"
#include "LineNumber.h"
#include "PipelineLayout.h"
#include "Plane.h"
#include "RenderPass.h"
#include "Sampler.h"
#include "Semaphore.h"
#include "Swapchain.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

#include "Tools.h"
#include "DescriptorSetLayout.h"
#include "RenderPass.h"
#include "Pipeline.h"
#include "Swapchain.h"
#include "FrameBuffer.h"
#include "CommandPool.h"
#include "CommandBuffer.h"
#include "DescriptorPool.h"
#include "Sampler.h"
#include "Camera.h"
#include "Timer.h"
#include "Font.h"
#include "Editor.h"
#include "Keyboard.h"
#include "CommandLine.h"
#include "../include/RenderTarget.h"
#include "../include/Animation.h"

class Vulkan {
public:
    Vulkan(const std::string& title, uint32_t width, uint32_t height);

    void run();
private:
    void initWindow();
    void initVulkan();

    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicDevice();
    void createSwapChain();
    void createRenderPass();
    void createUniformBuffers();
    void createSamplers();
    void createDescriptorPool();
    void createDescriptorSetLayout();
    void createDescriptorSet();
    void createVertex();
    void createGraphicsPipelines();
    void createColorResource();
    void createDepthResource();
    void createFrameBuffer();
    void createCommandPool();
    void createCommandBuffers();
    void createVertexBuffer();
    void createIndexBuffer();
    void createSyncObjects();
    void recordCommadBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void draw();
    void loadAssets();
    void updateDrawAssets();
    void recreateSwapChain();
    void createEditor();
    void initOther();

private:
    bool checkValidationLayerSupport() ;
    bool checkDeviceExtensionsSupport(VkPhysicalDevice physicalDevice);
    bool deviceSuitable(VkPhysicalDevice);
    void loadTextures();
    void loadChars();
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
    VkSampleCountFlagBits getMaxUsableSampleCount();
    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags features);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer, VkQueue queue);
    void fillColor(uint32_t index);

    void createCanvasPipeline();
    void createTextPipeline();
    void createCursorPipeline();

    void createTextDescriptorPool();
    void createCanvasDescriptorPool();

    void createTextDescriptorSetLayout();
    void createCanvasDescriptorSetLayout();

    void createTextDescriptorSet();
    void createCanvasDescriptorSet();
    void createCursorDescriptorSet();
    void mergeResource();

    void processText();
    void updateTexture();

    void input(int key, int scancode, int action, int mods);
    void processInput(int key, int scancode, int mods);
    void inputGeneral(int key, int scancode, int mods);
    void inputInsert(int key, int scancode, int mods);
    void inputCommand(int key, int scandcode, int mods);
    void processCmd(std::string cmd);

    void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
    
    GLFWwindow* windows_;
    uint32_t width_;
    uint32_t height_;
    std::string title_;

    VkInstance instance_;
    VkDebugUtilsMessengerEXT debugMessenger_;
    VkSurfaceKHR surface_;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_;
    std::shared_ptr<RenderPass> renderPass_;
    VkQueue graphicsQueue_;
    VkQueue presentQueue_;
    VkQueue transferQueue_;

    std::function<void(VkImage*)> vkImageDelete;

    std::shared_ptr<SwapChain> swapChain_;

    std::shared_ptr<Sampler> canvasSampler_;

    std::shared_ptr<DescriptorPool> canvasDescriptorPool_;
    std::shared_ptr<DescriptorSetLayout> canvasDescriptorSetLayout_;
    VkDescriptorSet canvasDescriptorSets_ = VK_NULL_HANDLE;

    std::shared_ptr<PipelineLayout> canvasPipelineLayout_;
    std::shared_ptr<Pipeline> canvasPipeline_;

    std::shared_ptr<Image> colorImage_;

    std::shared_ptr<Image> depthImage_;

    std::vector<std::shared_ptr<FrameBuffer>> frameBuffers_;

    std::shared_ptr<CommandPool> commandPool_;
    std::shared_ptr<CommandBuffer> commandBuffers_;

    std::shared_ptr<Fence> inFlightFences_;
    std::shared_ptr<Semaphore> imageAvaiableSemaphores_;
    std::shared_ptr<Semaphore> renderFinishSemaphores_;

    std::unordered_map<std::string, uint32_t> canvasMipLevels_;
    std::unordered_map<std::string, std::shared_ptr<Sampler>> canvasSamplers_;
    std::unordered_map<std::string, std::shared_ptr<Image>> canvasImages_;
    std::vector<std::string> textureNames_ = {"texture1.jpg", "texture2.jpg", "texture3.jpg", "texture4.jpg", "texture5.jpg"};
    std::string canvasTextureName_ = textureNames_.front();

    Tools::QueueFamilyIndices queueFamilies_;
    VkSampleCountFlagBits msaaSamples_ = VK_SAMPLE_COUNT_1_BIT;

    const std::vector<const char*> validationLayers_ = {"VK_LAYER_KHRONOS_validation"};
    const std::vector<const char*> deviceExtensions_ = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    std::shared_ptr<Buffer> uniformBuffers_;

    std::shared_ptr<Camera> camera_;
    Timer timer_;

    struct Character {
        Character() {}
        std::shared_ptr<Image> image_ = nullptr;
        uint32_t width_ = 0;
        uint32_t height_ = 0;
        uint32_t x_ = 0;
    };

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
        VkDebugUtilsMessageTypeFlagsEXT messageType, 
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, 
        void* pUserData
    );

    bool resized_ = false;

    static void frameBufferResizedCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<Vulkan*>(glfwGetWindowUserPointer(window));
        app->resized_ = true;
    }

    std::shared_ptr<Plane> canvas_;
    std::vector<Plane::Point> canvasVertices_;
    std::vector<uint32_t> canvasIndices_;
    std::shared_ptr<Buffer> canvasVertexBuffer_;
    std::shared_ptr<Buffer> canvasIndexBuffer_;
    std::shared_ptr<Buffer> canvasUniformBuffer_;

    std::shared_ptr<Font> font_;
    const std::string fontPath_ = "../fonts/jbMono.ttf";
    std::unordered_map<char, Font::Character> dictionary_;
    std::shared_ptr<PipelineLayout> fontPipelineLayout_;
    std::shared_ptr<Pipeline> fontPipeline_;
    std::shared_ptr<DescriptorPool> fontDescriptorPool_;
    std::shared_ptr<DescriptorSetLayout> fontDescriptorSetLayout_;
    VkDescriptorSet fontDescriptorSet_;
    std::vector<Font::Point> textVertices_;
    std::vector<uint32_t> textIndices_;
    std::shared_ptr<Buffer> textVertexBuffer_;
    std::shared_ptr<Buffer> textIndexBuffer_;

    std::vector<Animation<std::vector<Font::Point>>> textAnimations_;

    
    std::shared_ptr<LineNumber> lineNumber_;
    std::vector<Font::Point> lineNumberVertices_;
    std::vector<uint32_t> lineNumberIndices_;
    std::shared_ptr<Buffer> lineNumberVertexBuffer_;
    std::shared_ptr<Buffer> lineNumberIndexBuffer_;

    std::shared_ptr<Editor> editor_;
    std::shared_ptr<PipelineLayout> cursorPipelineLayout_;
    std::shared_ptr<Pipeline> cursorPipeline_;
    VkDescriptorSet cursorDescriptorSet_ = VK_NULL_HANDLE;
    std::shared_ptr<Buffer> cursorVertexBuffer_;
    std::shared_ptr<Buffer> cursorIndexBuffer_;
    std::vector<Plane::Point> cursorVertices_;
    std::vector<uint32_t> cursorIndices_;
    glm::vec3 cursorColor_{};

    std::shared_ptr<CommandLine> commandLine_;
    std::vector<Font::Point> cmdVertices_;
    std::vector<uint32_t> cmdIndices_;
    std::shared_ptr<Buffer> cmdVertexBuffer_;
    std::shared_ptr<Buffer> cmdIndexBuffer_;

    std::string currModeName_ = "General";
    std::vector<Font::Point> modeVertices_;
    std::vector<uint32_t> modeIndices_;
    std::shared_ptr<Buffer> modeVertexBuffer_;
    std::shared_ptr<Buffer> modeIndexBuffer_;

    std::shared_ptr<Keyboard> keyboard_;
    std::shared_ptr<Grammar> grammar_;

    int inputText_ = 0;
    int capsLock_ = 0;
    std::string text_;

    std::unordered_map<std::string, std::shared_ptr<RenderTarget>> renderTargets_;

    enum Color {
        Write, 
        Red, 
        Green, 
        Blue, 
        Black, 
    };

    const glm::vec3 write3_ = glm::vec3(1.0f, 1.0f, 1.0f);
    const glm::vec3 red3_ = glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 green3_ = glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 blue3_ = glm::vec3(0.0f, 0.0f, 1.0f);
    const glm::vec3 black3_ = glm::vec3(0.0f, 0.0f, 0.0f);
    
    Color color_ = Write;

    struct UniformBufferObject {
        alignas(16) glm::mat4 model_;
        alignas(16) glm::mat4 view_;
        alignas(16) glm::mat4 proj_;
    };
};

template<>
struct std::hash<std::pair<int, int>> {
    size_t operator() (const std::pair<int, int>& pair) const {
        return std::hash<int>()(pair.first) ^ std::hash<int>()(pair.second);
    }
};
#pragma once

#include "../include/Pipeline.h"
#include "../include/Buffer.h"
#include "vulkan/vulkan_core.h"

#include <memory>

class RenderTarget {
public:
    RenderTarget(const std::shared_ptr<Pipeline>& pipeline);

    void render(VkCommandBuffer, const std::shared_ptr<Buffer>& vertexBuffer, const std::shared_ptr<Buffer>& indexBuffer);
    void continueRender(VkCommandBuffer, const std::shared_ptr<Buffer>& vertexBuffer, const std::shared_ptr<Buffer>& indexBuffer);

private:
    std::shared_ptr<Pipeline> pipeline_;
};
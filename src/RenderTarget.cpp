#include "../include/RenderTarget.h"
#include "Buffer.h"
#include "Pipeline.h"
#include "vulkan/vulkan_core.h"
#include <memory>

RenderTarget::RenderTarget(const std::shared_ptr<Pipeline>& pipeline) : pipeline_(pipeline) {

}

void RenderTarget::render(VkCommandBuffer cmdBuffer, const std::shared_ptr<Buffer>& vertexBuffer, const std::shared_ptr<Buffer>& indexBuffer) {
    auto descriptorSets = pipeline_->descriptorSets();

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_->pipeline());
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_->pipelineLayout(), 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

    VkDeviceSize offsets[] = {0};
    std::vector<VkBuffer> vertexBuffers = {vertexBuffer->buffer()};
    auto count = indexBuffer->count();

    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers.data(), offsets);
    vkCmdBindIndexBuffer(cmdBuffer, indexBuffer->buffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmdBuffer, count, 1, 0, 0, 0);
}

void RenderTarget::continueRender(VkCommandBuffer cmdBuffer, const std::shared_ptr<Buffer>& vertexBuffer, const std::shared_ptr<Buffer>& indexBuffer) {
    VkDeviceSize offsets[] = {0};
    std::vector<VkBuffer> vertexBuffers = {vertexBuffer->buffer()};
    auto count = indexBuffer->count();

    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers.data(), offsets);
    vkCmdBindIndexBuffer(cmdBuffer, indexBuffer->buffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmdBuffer, count, 1, 0, 0, 0);
}
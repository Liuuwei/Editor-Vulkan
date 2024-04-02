#pragma once

#include "vulkan/vulkan_core.h"
#include <vulkan/vulkan.h>

#include <vector>
#include <string>
#include <unordered_map>

class  Pipeline {
public:
    Pipeline(VkDevice device);
    ~Pipeline();

    void init();
    VkPipeline pipeline() const { return pipeline_; }
    VkPipelineLayout pipelineLayout() const { return layout_; }
    std::vector<VkDescriptorSet> descriptorSets() const;
public:
    VkDevice device_;
    VkPipeline pipeline_;
    std::unordered_map<std::string, VkDescriptorSet> descriptorSets_;
    VkPipelineCreateFlags                            flags_{};
    uint32_t                                         stageCount_{};
    VkPipelineShaderStageCreateInfo*           pStages_{};
    VkPipelineVertexInputStateCreateInfo*      pVertexInputState_{};
    VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState_{};
    VkPipelineTessellationStateCreateInfo*     pTessellationState_{};
    VkPipelineViewportStateCreateInfo*         pViewportState_{};
    VkPipelineRasterizationStateCreateInfo*    pRasterizationState_{};
    VkPipelineMultisampleStateCreateInfo*      pMultisampleState_{};
    VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState_{};
    VkPipelineColorBlendStateCreateInfo*       pColorBlendState_{};
    VkPipelineDynamicStateCreateInfo*          pDynamicState_{};
    VkPipelineLayout                                 layout_{};
    VkRenderPass                                     renderPass_{};
    uint32_t                                         subpass_{};
    VkPipeline                                       basePipelineHandle_{};
    int32_t                                          basePipelineIndex_{};
};
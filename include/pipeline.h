#pragma once
#include <vulkan/vulkan_core.h>
#include <vector>
#include <string>
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include "device.h"
#include "swapchain.h"
#include "renderpass.h"

struct CullPushConstants {
    glm::vec4 planes[6];
    glm::vec4 centroidAndRadius;
    uint32_t  totalCount;
    uint32_t  _pad[3];
};

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 normal;
};

class VulkanPipeline
{
public:
  void createGraphicsPipeline(const VulkanDevice& device,
                              const VulkanRenderPass& renderPass,
                              const char* vertSpvPath = "shaders/vert.spv",
                              const char* fragSpvPath = "shaders/frag.spv");
  void createComputePipeline(const VulkanDevice& device,
                             const char* spvPath = "shaders/cull.spv");

  void destroy(const VulkanDevice& device);

  VkPipeline            getPipeline()                  const { return graphicsPipeline; }
  VkPipelineLayout      getLayout()                    const { return pipelineLayout; }
  VkDescriptorSetLayout getDescriptorSetLayout()       const { return descriptorSetLayout; }
  VkPipeline            getComputePipeline()           const { return computePipeline; }
  VkPipelineLayout      getComputeLayout()             const { return computePipelineLayout; }
  VkDescriptorSetLayout getComputeDescriptorSetLayout() const { return computeDescriptorSetLayout; }

private:
  VkShaderModule createShaderModule(const VulkanDevice& device, const std::vector<char>& code) const;

  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
  VkPipelineLayout      pipelineLayout      = VK_NULL_HANDLE;
  VkPipeline            graphicsPipeline    = VK_NULL_HANDLE;

  VkDescriptorSetLayout computeDescriptorSetLayout = VK_NULL_HANDLE;
  VkPipelineLayout      computePipelineLayout      = VK_NULL_HANDLE;
  VkPipeline            computePipeline            = VK_NULL_HANDLE;

};

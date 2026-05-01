#pragma once
#include <vulkan/vulkan_core.h>
#include <vector>
#include <string>
#include <array>
#include <glm/glm.hpp>
#include "device.h"
#include "swapchain.h"
#include "renderpass.h"

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

  void destroy(const VulkanDevice& device);
  VkPipeline            getPipeline()           const { return graphicsPipeline; }
  VkPipelineLayout      getLayout()             const { return pipelineLayout; }
  VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }

private:
  VkShaderModule createShaderModule(const VulkanDevice& device, const std::vector<char>& code) const;

  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
  VkPipelineLayout      pipelineLayout      = VK_NULL_HANDLE;
  VkPipeline            graphicsPipeline    = VK_NULL_HANDLE;

};

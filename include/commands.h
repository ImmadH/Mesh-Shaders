#pragma once
#include <vulkan/vulkan_core.h>
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include "device.h"
#include "swapchain.h"
#include "renderpass.h"
#include "pipeline.h"
#include "mesh.h"

struct CullDispatchInfo {
    VkPipeline         pipeline;
    VkPipelineLayout   layout;
    VkDescriptorSet    descriptorSet;
    VkBuffer           outputBuffer;
    CullPushConstants  pc;
};

class VulkanCommands
{
public:
  void createCommandPool(const VulkanDevice& device);
  void allocateCommandBuffers(const VulkanDevice& device, uint32_t count);

  void record(uint32_t i,
              const VulkanSwapchain& swapchain,
              const VulkanRenderPass& renderPass,
              const VulkanPipeline& pipeline,
              VkFramebuffer framebuffer,
              const MeshRegistry& registry,
              VkDescriptorSet descriptorSet,
              VkBuffer indirectBuffer,
              const CullDispatchInfo& cull,
              glm::mat4 vp,
              std::function<void(VkCommandBuffer)> imguiDraw = nullptr);

  void destroy(const VulkanDevice& device);

  VkCommandPool getCommandPool() const { return commandPool; }
  const std::vector<VkCommandBuffer>& getCommandBuffers() const { return commandBuffers; }

private:
  VkCommandPool commandPool = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> commandBuffers;
};

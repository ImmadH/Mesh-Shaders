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

struct MeshPushConstants {
    glm::mat4 vp;             // offset 0   (64 bytes)
    glm::vec3 meshCenter;     // offset 64
    float     meshRadius;     // offset 76
    glm::vec3 cameraPos;      // offset 80
    uint32_t  meshletCount;   // offset 92
    uint32_t  renderMode;     // offset 96  0=Solid 1=Clusters 2=LODs 3=Triangles
    float     cotHalfFovH;    // offset 100  (screenHeight/2) / tan(fovY/2) — for error projection
    float     lodThreshold;   // offset 104  screen-pixel threshold for LOD switch
    uint32_t  forceLod;       // offset 108  0=auto, N=force LOD level N-1
};                            // sizeof = 112 bytes

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
                              const char* taskSpvPath = "shaders/task.spv",
                              const char* meshSpvPath = "shaders/mesh.spv",
                              const char* fragSpvPath = "shaders/frag.spv");

  void destroy(const VulkanDevice& device);

  VkPipeline            getPipeline()            const { return graphicsPipeline; }
  VkPipelineLayout      getLayout()              const { return pipelineLayout; }
  VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }

private:
  VkShaderModule createShaderModule(const VulkanDevice& device, const std::vector<char>& code) const;

  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
  VkPipelineLayout      pipelineLayout      = VK_NULL_HANDLE;
  VkPipeline            graphicsPipeline    = VK_NULL_HANDLE;

};

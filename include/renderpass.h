#pragma once 
#include "device.h"
#include <vulkan/vulkan_core.h>
class VulkanRenderPass
{
public:
  void createRenderPass(VulkanDevice& device, VkFormat swapChainImageFormat, VkFormat depthFormat);
  void destroy(VulkanDevice& device);
  VkRenderPass getRenderPass() const {return renderPass;}

private:
  VkRenderPass renderPass = VK_NULL_HANDLE;
};

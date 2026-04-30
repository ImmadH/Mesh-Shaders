#pragma once
#include "instance.h"
#include "vk_types.h"
#include <vulkan/vulkan_core.h>
SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
class VulkanDevice 
{
public:
  void create(VulkanInstance& instance, VkSurfaceKHR surface);
  void destroy();
  VkDevice getDevice() const {return device;}
  VkPhysicalDevice getPhysicalDevice() const {return physicalDevice;}
  VkQueue getGraphicsQueue() const {return graphicsQueue;}
  VkQueue getPresentQueue() const {return presentQueue;}
  QueueFamilyIndices getQueueFamilyIndices() const {return queueFI;}

private:
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;  
  VkDevice device = VK_NULL_HANDLE;
  VkQueue graphicsQueue = VK_NULL_HANDLE;;
  VkQueue presentQueue = VK_NULL_HANDLE;
  QueueFamilyIndices queueFI;
};



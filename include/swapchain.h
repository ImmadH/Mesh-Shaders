#pragma once
#include <vulkan/vulkan_core.h>
#include  "device.h"
#include "GLFW/glfw3.h"
#include "vk_types.h"
#include <vector>


class VulkanSwapchain 
{
public:
  void createSwapChain(VulkanDevice& device, VkSurfaceKHR surface, GLFWwindow* window);
  void createImageViews(const VulkanDevice& device);
  void destroy(VulkanDevice& device);
  void cleanSwapChain(VulkanDevice& device);
  
  //utilities from vktut
  VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
  VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) const;
  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities,GLFWwindow* window) const;

  VkSwapchainKHR getSwapChain() const {return swapChain;}
  VkFormat  getImageFormat() const {return swapChainImageFormat;}
  VkExtent2D getExtent() const { return swapChainExtent; }
  const std::vector<VkImageView>& getImageViews() const {return swapChainImageViews;}
  const std::vector<VkImage>& getImages() const {return swapChainImages;}



private:
  VkSwapchainKHR swapChain = VK_NULL_HANDLE;
  std::vector<VkImage> swapChainImages;
  std::vector<VkImageView> swapChainImageViews;
  VkFormat swapChainImageFormat = VK_FORMAT_UNDEFINED;
  VkExtent2D swapChainExtent = {};

};

#include "swapchain.h"
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <limits>
#include <vulkan/vulkan_core.h>
//utilities
VkSurfaceFormatKHR VulkanSwapchain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const
{
  for (const auto& availableFormat : formats) 
  {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
    {
      return availableFormat;
    }
  }

  return formats[0];
}

VkPresentModeKHR VulkanSwapchain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) const
{
  for (const auto& availablePresentMode : modes) 
  {
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
    {
      return availablePresentMode;
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities,GLFWwindow* window) const
{
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) 
  {
    return capabilities.currentExtent;
  } 
  else 
  {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    VkExtent2D actualExtent = {
          static_cast<uint32_t>(width),
          static_cast<uint32_t>(height)
    };

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
  }
}

void VulkanSwapchain::createSwapChain(VulkanDevice& device, VkSurfaceKHR surface, GLFWwindow* window)
{
  VkPhysicalDevice physDevice = device.getPhysicalDevice();
  SwapChainSupportDetails support = querySwapChainSupport(physDevice, surface);

  VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(support.formats);
  VkPresentModeKHR presentMode = chooseSwapPresentMode(support.presentModes);
  VkExtent2D extent = chooseSwapExtent(support.capabilities, window);

  uint32_t imageCount = support.capabilities.minImageCount + 1;
  if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) 
  {
    imageCount = support.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = surface;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  QueueFamilyIndices indices =  device.getQueueFamilyIndices();
      uint32_t queueFamilyIndices[2] = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
  };

  if (indices.graphicsFamily != indices.presentFamily) 
  {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } 
  else 
  {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  createInfo.preTransform = support.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;

  createInfo.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(device.getDevice(), &createInfo, nullptr, &swapChain) != VK_SUCCESS) 
  {
    throw std::runtime_error("failed to create swap chain!");
  }

  std::cout << "Created Swapchain\n";

  vkGetSwapchainImagesKHR(device.getDevice(), swapChain, &imageCount, nullptr);
  swapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(device.getDevice(), swapChain, &imageCount, swapChainImages.data());

  swapChainImageFormat = surfaceFormat.format;
  swapChainExtent = extent;
}

void VulkanSwapchain::createImageViews(const VulkanDevice& device)
{
  swapChainImageViews.resize(swapChainImages.size());
  for (size_t i = 0; i < swapChainImages.size(); i++) 
  {
      VkImageViewCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      createInfo.image = swapChainImages[i];
      createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      createInfo.format = swapChainImageFormat;
      createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      createInfo.subresourceRange.baseMipLevel = 0;
      createInfo.subresourceRange.levelCount = 1;
      createInfo.subresourceRange.baseArrayLayer = 0;
      createInfo.subresourceRange.layerCount = 1;

      if (vkCreateImageView(device.getDevice(), &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) 
      {
          throw std::runtime_error("failed to create image views!");
      }
  }
}

void VulkanSwapchain::destroy(VulkanDevice& device)
{
  for (auto view : swapChainImageViews)
  {
    if (view) vkDestroyImageView(device.getDevice(), view, nullptr);
  }
  swapChainImageViews.clear();

  if (swapChain)
  {
    vkDestroySwapchainKHR(device.getDevice(), swapChain, nullptr);
    swapChain = VK_NULL_HANDLE;
  }

  swapChainImages.clear();
  swapChainImageFormat = VK_FORMAT_UNDEFINED;
  swapChainExtent = {};
  std::cerr << "Swapchain Destroyed\n";
}

void VulkanSwapchain::cleanSwapChain(VulkanDevice& device)
{
  //make sure to destroy framebuffers before calling this
  for (auto framebuffer : swapChainImageViews)
  {
      vkDestroyImageView(device.getDevice(), framebuffer, nullptr);
  }
  swapChainImageViews.clear();
  swapChainImages.clear();
  vkDestroySwapchainKHR(device.getDevice(), swapChain, nullptr);
  swapChain = VK_NULL_HANDLE;
}

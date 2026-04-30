#pragma once
#include <stdlib.h>
#include <iostream>
#include "instance.h"
#include "device.h"
#include "swapchain.h"
#include "renderpass.h"
#include "pipeline.h"
#include "commands.h"
#include "sync.h"
#include "mesh.h"
#include "camera.h"
#include "GLFW/glfw3.h"
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

class VulkanApp
{
public:
  void run();
  void createFrameBuffers();
  void destroyFrameBuffers();
  void recreateSwapChain();
  void drawFrame();

  static void framebufferResizeCallback(GLFWwindow* window, int, int);
  static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
  static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

private:
  void initWindow();
  void initVulkan();
  void mainLoop();
  void cleanup();
  void createDepthResources();
  void destroyDepthResources();

  GLFWwindow*  window            = nullptr;
  VkSurfaceKHR surface           = VK_NULL_HANDLE;
  const uint32_t WIDTH           = 800;
  const uint32_t HEIGHT          = 600;
  uint32_t       currentFrame    = 0;
  bool           framebufferResized = false;

  double lastMouseX    = 0.0;
  double lastMouseY    = 0.0;
  bool   firstMouse    = true;
  bool   mouseLocked    = false;
  bool   f2WasPressed   = false;
  double lastFrameTime = 0.0;

  std::vector<VkFramebuffer> swapChainFramebuffers;

  VkImage       depthImage      = VK_NULL_HANDLE;
  VmaAllocation depthAllocation = VK_NULL_HANDLE;
  VkImageView   depthImageView  = VK_NULL_HANDLE;

  VulkanInstance   instance;
  VulkanDevice     device;
  VmaAllocator     allocator = VK_NULL_HANDLE;
  VulkanSwapchain  swapchain;
  VulkanRenderPass renderPass;
  VulkanPipeline   pipeline;
  VulkanMesh       mesh;
  VulkanCommands   commands;
  VulkanSync       sync;
};

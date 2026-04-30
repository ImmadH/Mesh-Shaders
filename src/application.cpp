#include "application.h"
#include <stdexcept>
#include <iostream>
#include <vulkan/vulkan_core.h>
#include <glm/gtc/matrix_transform.hpp>

static constexpr const char* MESH_PATH    = "models/bunny/scene.gltf";
static constexpr VkFormat    DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;

static const glm::mat4 MODEL_MATRIX = glm::rotate(glm::mat4(1.0f),
                                                   glm::radians(-90.0f),
                                                   glm::vec3(1, 0, 0));

void VulkanApp::run()
{
  initWindow();
  initVulkan();
  mainLoop();
  cleanup();
}

void VulkanApp::initWindow()
{
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window = glfwCreateWindow(WIDTH, HEIGHT, "Meshlet Fun", nullptr, nullptr);
  glfwSetWindowUserPointer(window, this);
  glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
  glfwSetCursorPosCallback(window, cursorPosCallback);
  glfwSetMouseButtonCallback(window, mouseButtonCallback);
  if (glfwRawMouseMotionSupported())
    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
}

void VulkanApp::framebufferResizeCallback(GLFWwindow* window, int, int)
{
  auto app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
  app->framebufferResized = true;
}

void VulkanApp::mouseButtonCallback(GLFWwindow* window, int button, int action, int)
{
  auto app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && !app->mouseLocked) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    app->mouseLocked = true;
    app->firstMouse  = true; 
  }
}

void VulkanApp::cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
  auto app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
  if (!app->mouseLocked) return;
  if (app->firstMouse) {
    app->lastMouseX = xpos;
    app->lastMouseY = ypos;
    app->firstMouse = false;
    return;
  }
  float dx = (float)(xpos - app->lastMouseX);
  float dy = (float)(ypos - app->lastMouseY);
  app->lastMouseX = xpos;
  app->lastMouseY = ypos;
  Camera::processMouse(dx, dy);
}

void VulkanApp::initVulkan()
{
  instance.create(enableValidation);

  if (glfwCreateWindowSurface(instance.getInstance(), window, nullptr, &surface) != VK_SUCCESS)
    throw std::runtime_error("failed to create window surface!");
  std::cout << "Created Window Surface\n";

  device.create(instance, surface);

  VmaAllocatorCreateInfo allocatorInfo{};
  allocatorInfo.physicalDevice   = device.getPhysicalDevice();
  allocatorInfo.device           = device.getDevice();
  allocatorInfo.instance         = instance.getInstance();
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
  vmaCreateAllocator(&allocatorInfo, &allocator);

  swapchain.createSwapChain(device, surface, window);
  swapchain.createImageViews(device);
  renderPass.createRenderPass(device, swapchain.getImageFormat(), DEPTH_FORMAT);
  pipeline.createGraphicsPipeline(device, swapchain, renderPass);

  createDepthResources();
  createFrameBuffers();

  registry.init(allocator);
  instanceBuffer.init(allocator);
  mesh.create(registry, MESH_PATH);

  Camera::init({0.0f, 0.05f, 0.5f}, -90.0f, -5.0f);

  createDescriptors();

  commands.createCommandPool(device);
  commands.allocateCommandBuffers(device, (uint32_t)swapChainFramebuffers.size());
  sync.createSyncObjects(device);

  lastFrameTime = glfwGetTime();
}

void VulkanApp::createDescriptors()
{
  VkDescriptorPoolSize poolSize{};
  poolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  poolSize.descriptorCount = 1;

  VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
  poolInfo.maxSets       = 1;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes    = &poolSize;
  if (vkCreateDescriptorPool(device.getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
    throw std::runtime_error("failed to create descriptor pool!");

  VkDescriptorSetLayout layout = pipeline.getDescriptorSetLayout();
  VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
  allocInfo.descriptorPool     = descriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts        = &layout;
  if (vkAllocateDescriptorSets(device.getDevice(), &allocInfo, &descriptorSet) != VK_SUCCESS)
    throw std::runtime_error("failed to allocate descriptor set!");

  VkDescriptorBufferInfo bufInfo{};
  bufInfo.buffer = instanceBuffer.getBuffer();
  bufInfo.offset = 0;
  bufInfo.range  = VK_WHOLE_SIZE;

  VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
  write.dstSet          = descriptorSet;
  write.dstBinding      = 0;
  write.descriptorCount = 1;
  write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  write.pBufferInfo     = &bufInfo;
  vkUpdateDescriptorSets(device.getDevice(), 1, &write, 0, nullptr);
}

void VulkanApp::createDepthResources()
{
  VkExtent2D ext = swapchain.getExtent();

  VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
  imageInfo.imageType   = VK_IMAGE_TYPE_2D;
  imageInfo.format      = DEPTH_FORMAT;
  imageInfo.extent      = { ext.width, ext.height, 1 };
  imageInfo.mipLevels   = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.usage       = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

  VmaAllocationCreateInfo allocInfo{};
  allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  if (vmaCreateImage(allocator, &imageInfo, &allocInfo, &depthImage, &depthAllocation, nullptr) != VK_SUCCESS)
    throw std::runtime_error("failed to create depth image!");

  VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
  viewInfo.image                           = depthImage;
  viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format                          = DEPTH_FORMAT;
  viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
  viewInfo.subresourceRange.baseMipLevel   = 0;
  viewInfo.subresourceRange.levelCount     = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount     = 1;

  if (vkCreateImageView(device.getDevice(), &viewInfo, nullptr, &depthImageView) != VK_SUCCESS)
    throw std::runtime_error("failed to create depth image view!");
}

void VulkanApp::destroyDepthResources()
{
  if (depthImageView) {
    vkDestroyImageView(device.getDevice(), depthImageView, nullptr);
    depthImageView = VK_NULL_HANDLE;
  }
  if (depthImage) {
    vmaDestroyImage(allocator, depthImage, depthAllocation);
    depthImage      = VK_NULL_HANDLE;
    depthAllocation = VK_NULL_HANDLE;
  }
}

void VulkanApp::createFrameBuffers()
{
  const auto& views  = swapchain.getImageViews();
  const auto  extent = swapchain.getExtent();
  swapChainFramebuffers.resize(views.size());

  for (size_t i = 0; i < views.size(); ++i)
  {
    VkImageView attachments[] = { views[i], depthImageView };

    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass      = renderPass.getRenderPass();
    fbInfo.attachmentCount = 2;
    fbInfo.pAttachments    = attachments;
    fbInfo.width           = extent.width;
    fbInfo.height          = extent.height;
    fbInfo.layers          = 1;

    if (vkCreateFramebuffer(device.getDevice(), &fbInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
      throw std::runtime_error("failed to create framebuffer!");
  }
}

void VulkanApp::destroyFrameBuffers()
{
  for (auto fb : swapChainFramebuffers)
    if (fb) vkDestroyFramebuffer(device.getDevice(), fb, nullptr);
  swapChainFramebuffers.clear();
}

void VulkanApp::recreateSwapChain()
{
  int width = 0, height = 0;
  glfwGetFramebufferSize(window, &width, &height);
  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(window, &width, &height);
    glfwWaitEvents();
  }
  vkDeviceWaitIdle(device.getDevice());

  destroyFrameBuffers();
  destroyDepthResources();
  swapchain.cleanSwapChain(device);

  swapchain.createSwapChain(device, surface, window);
  swapchain.createImageViews(device);
  createDepthResources();
  createFrameBuffers();

  commands.destroy(device);
  commands.createCommandPool(device);
  commands.allocateCommandBuffers(device, (uint32_t)swapChainFramebuffers.size());
}

void VulkanApp::mainLoop()
{
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
      glfwSetWindowShouldClose(window, GLFW_TRUE);

    bool f2Down = glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS;
    if (f2Down && !f2WasPressed) {
      if (mouseLocked) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        mouseLocked = false;
      } else {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        mouseLocked = true;
      }
      firstMouse = true;
    }
    f2WasPressed = f2Down;
    drawFrame();
  }
  vkDeviceWaitIdle(device.getDevice());
}

void VulkanApp::drawFrame()
{
  double now = glfwGetTime();
  float  dt  = (float)(now - lastFrameTime);
  lastFrameTime = now;

  Camera::processKeyboard(window, dt);

  VkFence frameFence = sync.getInFlightFence()[currentFrame];
  vkWaitForFences(device.getDevice(), 1, &frameFence, VK_TRUE, UINT64_MAX);
  vkResetFences(device.getDevice(), 1, &frameFence);

  uint32_t imageIndex = 0;
  VkResult result = vkAcquireNextImageKHR(device.getDevice(), swapchain.getSwapChain(),
                                          UINT64_MAX,
                                          sync.getImageAvailableSemaphore()[currentFrame],
                                          VK_NULL_HANDLE, &imageIndex);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    framebufferResized = false;
    recreateSwapChain();
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }

  auto ext     = swapchain.getExtent();
  float aspect = (float)ext.width / (float)ext.height;

  instanceBuffer.reset();

  glm::vec4 planes[6];
  getFrustumPlanes(Camera::getMVP(aspect), planes);

  float spacing = 0.3f;
  for (int i = 0; i < 100; i++)
    for (int j = 0; j < 100; j++) {
      glm::mat4 t     = glm::translate(glm::mat4(1.0f), glm::vec3(i * spacing, 0.0f, j * spacing));
      glm::mat4 model = t * MODEL_MATRIX;
      glm::vec3 worldCenter = glm::vec3(model * glm::vec4(mesh.getCentroid(), 1.0f));
      if (isVisible(planes, worldCenter, mesh.getRadius()))
        instanceBuffer.push(model);
    }
  std::cout << "visible: " << instanceBuffer.getCount() << " / 10000\n";

  commands.record(imageIndex, swapchain, renderPass, pipeline,
                  swapChainFramebuffers[imageIndex], registry, mesh,
                  descriptorSet, instanceBuffer.getCount(),
                  Camera::getMVP(aspect));

  VkSemaphore          waitSems[]   = { sync.imageAvailable(currentFrame) };
  VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
  VkSemaphore          signalSems[] = { sync.renderFinished(currentFrame) };
  const auto& cbs = commands.getCommandBuffers();

  VkSubmitInfo submitInfo{};
  submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount   = 1;
  submitInfo.pWaitSemaphores      = waitSems;
  submitInfo.pWaitDstStageMask    = waitStages;
  submitInfo.commandBufferCount   = 1;
  submitInfo.pCommandBuffers      = &cbs[imageIndex];
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores    = signalSems;

  if (vkQueueSubmit(device.getGraphicsQueue(), 1, &submitInfo, frameFence) != VK_SUCCESS)
    throw std::runtime_error("failed to submit draw command buffer!");

  VkSwapchainKHR scs[] = { swapchain.getSwapChain() };

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores    = signalSems;
  presentInfo.swapchainCount     = 1;
  presentInfo.pSwapchains        = scs;
  presentInfo.pImageIndices      = &imageIndex;

  result = vkQueuePresentKHR(device.getPresentQueue(), &presentInfo);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    framebufferResized = false;
    recreateSwapChain();
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("failed to present swap chain image!");
  }

  currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanApp::cleanup()
{
  std::cout << '\n';
  destroyFrameBuffers();
  destroyDepthResources();
  sync.destroy(device);
  commands.destroy(device);
  if (descriptorPool) {
    vkDestroyDescriptorPool(device.getDevice(), descriptorPool, nullptr);
    descriptorPool = VK_NULL_HANDLE;
  }
  instanceBuffer.destroy(allocator);
  registry.destroy(allocator);
  vmaDestroyAllocator(allocator);
  pipeline.destroy(device);
  renderPass.destroy(device);
  swapchain.destroy(device);
  device.destroy();
  vkDestroySurfaceKHR(instance.getInstance(), surface, nullptr);
  std::cerr << "Window Surface Destroyed\n";
  instance.destroy();
  glfwDestroyWindow(window);
  glfwTerminate();
}

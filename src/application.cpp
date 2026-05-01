#include "application.h"
#include <stdexcept>
#include <iostream>
#include <vulkan/vulkan_core.h>
#include <glm/gtc/matrix_transform.hpp>
#include "imgui.h"

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
  if (glfwRawMouseMotionSupported())
    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
}

void VulkanApp::framebufferResizeCallback(GLFWwindow* window, int, int)
{
  auto app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
  app->framebufferResized = true;
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
  pipeline.createGraphicsPipeline(device, renderPass);

  createDepthResources();
  createFrameBuffers();

  registry.init(allocator);
  allInstances.init(allocator);
  visibleInstances.init(allocator);
  indirectDrawBuffer.init(allocator);
  mesh.create(registry, MESH_PATH);

  // Pre-build all 10k model matrices once — GPU culling reads from this every frame
  constexpr float spacing = 0.3f;
  for (int i = 0; i < 100; i++)
    for (int j = 0; j < 100; j++) {
      glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(i * spacing, 0.0f, j * spacing));
      allInstances.push(t * MODEL_MATRIX);
    }

  // Bake static draw params into indirect buffer once; instanceCount reset each frame
  indirectDrawBuffer.write(mesh.getIndexCount(), 0,
                           mesh.getFirstIndex(), (int32_t)mesh.getFirstVertex());

  pipeline.createComputePipeline(device);

  Camera::init({0.0f, 0.05f, 0.5f}, -90.0f, -5.0f);

  createDescriptors();

  commands.createCommandPool(device);
  commands.allocateCommandBuffers(device, (uint32_t)swapChainFramebuffers.size());
  sync.createSyncObjects(device, (uint32_t)swapchain.getImageViews().size());

  imgui.init(window,
             instance.getInstance(),
             device.getPhysicalDevice(),
             device.getDevice(),
             device.getQueueFamilyIndices().graphicsFamily.value(),
             device.getGraphicsQueue(),
             (uint32_t)swapchain.getImageViews().size(),
             renderPass.getRenderPass());

  lastFrameTime = glfwGetTime();
}

void VulkanApp::createDescriptors()
{
  // --- graphics descriptor set: visibleInstances at binding 0 ---
  VkDescriptorPoolSize gfxPoolSize{};
  gfxPoolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  gfxPoolSize.descriptorCount = 1;

  VkDescriptorPoolCreateInfo gfxPoolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
  gfxPoolInfo.maxSets       = 1;
  gfxPoolInfo.poolSizeCount = 1;
  gfxPoolInfo.pPoolSizes    = &gfxPoolSize;
  if (vkCreateDescriptorPool(device.getDevice(), &gfxPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
    throw std::runtime_error("failed to create graphics descriptor pool!");

  VkDescriptorSetLayout gfxLayout = pipeline.getDescriptorSetLayout();
  VkDescriptorSetAllocateInfo gfxAlloc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
  gfxAlloc.descriptorPool     = descriptorPool;
  gfxAlloc.descriptorSetCount = 1;
  gfxAlloc.pSetLayouts        = &gfxLayout;
  if (vkAllocateDescriptorSets(device.getDevice(), &gfxAlloc, &descriptorSet) != VK_SUCCESS)
    throw std::runtime_error("failed to allocate graphics descriptor set!");

  VkDescriptorBufferInfo visibleBufInfo{};
  visibleBufInfo.buffer = visibleInstances.getBuffer();
  visibleBufInfo.offset = 0;
  visibleBufInfo.range  = VK_WHOLE_SIZE;

  VkWriteDescriptorSet gfxWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
  gfxWrite.dstSet          = descriptorSet;
  gfxWrite.dstBinding      = 0;
  gfxWrite.descriptorCount = 1;
  gfxWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  gfxWrite.pBufferInfo     = &visibleBufInfo;
  vkUpdateDescriptorSets(device.getDevice(), 1, &gfxWrite, 0, nullptr);

  // --- compute descriptor set: allInstances(0), visibleInstances(1), indirectCmd(2) ---
  VkDescriptorPoolSize compPoolSize{};
  compPoolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  compPoolSize.descriptorCount = 3;

  VkDescriptorPoolCreateInfo compPoolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
  compPoolInfo.maxSets       = 1;
  compPoolInfo.poolSizeCount = 1;
  compPoolInfo.pPoolSizes    = &compPoolSize;
  if (vkCreateDescriptorPool(device.getDevice(), &compPoolInfo, nullptr, &computeDescriptorPool) != VK_SUCCESS)
    throw std::runtime_error("failed to create compute descriptor pool!");

  VkDescriptorSetLayout compLayout = pipeline.getComputeDescriptorSetLayout();
  VkDescriptorSetAllocateInfo compAlloc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
  compAlloc.descriptorPool     = computeDescriptorPool;
  compAlloc.descriptorSetCount = 1;
  compAlloc.pSetLayouts        = &compLayout;
  if (vkAllocateDescriptorSets(device.getDevice(), &compAlloc, &computeDescriptorSet) != VK_SUCCESS)
    throw std::runtime_error("failed to allocate compute descriptor set!");

  VkDescriptorBufferInfo bufs[3] = {};
  bufs[0].buffer = allInstances.getBuffer();     bufs[0].range = VK_WHOLE_SIZE;
  bufs[1].buffer = visibleInstances.getBuffer(); bufs[1].range = VK_WHOLE_SIZE;
  bufs[2].buffer = indirectDrawBuffer.getBuffer(); bufs[2].range = VK_WHOLE_SIZE;

  VkWriteDescriptorSet compWrites[3] = {};
  for (int i = 0; i < 3; i++) {
    compWrites[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    compWrites[i].dstSet          = computeDescriptorSet;
    compWrites[i].dstBinding      = (uint32_t)i;
    compWrites[i].descriptorCount = 1;
    compWrites[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    compWrites[i].pBufferInfo     = &bufs[i];
  }
  vkUpdateDescriptorSets(device.getDevice(), 3, compWrites, 0, nullptr);
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

  indirectDrawBuffer.resetInstanceCount();

  glm::vec4 planes[6];
  getFrustumPlanes(Camera::getMVP(aspect), planes);

  CullDispatchInfo cull{};
  cull.pipeline        = pipeline.getComputePipeline();
  cull.layout          = pipeline.getComputeLayout();
  cull.descriptorSet   = computeDescriptorSet;
  cull.outputBuffer    = visibleInstances.getBuffer();
  memcpy(cull.pc.planes, planes, sizeof(planes));
  cull.pc.centroidAndRadius = glm::vec4(mesh.getCentroid(), mesh.getRadius());
  cull.pc.totalCount        = 10000;

  imgui.profilerUpdate(dt);
  imgui.beginFrame();
  ImGui::Begin("Debug");
  ImGui::Text("Instances: 10000 (GPU culled)");
  ImGui::Checkbox("Profile Window", &showProfiler);
  ImGui::End();
  if (showProfiler) imgui.drawProfileWindow(showProfiler);
  imgui.endFrame();

  commands.record(imageIndex, swapchain, renderPass, pipeline,
                  swapChainFramebuffers[imageIndex], registry,
                  descriptorSet, indirectDrawBuffer.getBuffer(), cull,
                  Camera::getMVP(aspect),
                  [this](VkCommandBuffer cmd) { imgui.render(cmd); });

  VkSemaphore          waitSems[]   = { sync.imageAvailable(currentFrame) };
  VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
  VkSemaphore          signalSems[] = { sync.renderFinished(imageIndex) };
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
  imgui.shutdown();
  destroyFrameBuffers();
  destroyDepthResources();
  sync.destroy(device);
  commands.destroy(device);
  if (computeDescriptorPool) {
    vkDestroyDescriptorPool(device.getDevice(), computeDescriptorPool, nullptr);
    computeDescriptorPool = VK_NULL_HANDLE;
  }
  if (descriptorPool) {
    vkDestroyDescriptorPool(device.getDevice(), descriptorPool, nullptr);
    descriptorPool = VK_NULL_HANDLE;
  }
  allInstances.destroy(allocator);
  visibleInstances.destroy(allocator);
  indirectDrawBuffer.destroy(allocator);
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

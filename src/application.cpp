#include "application.h"
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <vector>
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
  mesh.create(registry, allocator, MESH_PATH);

  // Poisson disk placement with random full-3D rotation — sea of bunnies.
  // Multiple starting seeds create natural clumping; minDist prevents clipping.
  {
    // LCG-based RNG (deterministic)
    uint32_t rngState = 0xdeadbeef;
    auto rngF = [&]() -> float {
      rngState = rngState * 1664525u + 1013904223u;
      return float(rngState >> 8) / float(1 << 24);
    };
    auto rngR = [&](float lo, float hi) { return lo + (hi - lo) * rngF(); };

    // Bridson's Poisson disk sampling in 2D (XZ plane)
    const float minDist  = 0.20f;           // tighter packing; slight overlap OK for rotated bunnies
    const float cellSz   = minDist * 0.707f;
    const int   side     = 180;             // 180 × cellSz ≈ 25 m
    const float half     = side * cellSz * 0.5f;
    const int   kCands   = 28;              // candidates per active point

    std::vector<int>       grid(side * side, -1);
    std::vector<glm::vec2> pts;
    std::vector<int>       active;
    pts.reserve(10000);
    active.reserve(10000);

    auto toCell = [&](float v) -> int { return (int)((v + half) / cellSz); };

    auto tryAdd = [&](glm::vec2 p) -> bool {
      if (std::abs(p.x) >= half || std::abs(p.y) >= half) return false;
      int cx = toCell(p.x), cz = toCell(p.y);
      for (int di = -2; di <= 2; di++) for (int dj = -2; dj <= 2; dj++) {
        int nx = cx+di, nz = cz+dj;
        if (nx<0||nx>=side||nz<0||nz>=side) continue;
        int idx = grid[nz*side+nx];
        if (idx < 0) continue;
        glm::vec2 d = p - pts[idx];
        if (d.x*d.x + d.y*d.y < minDist*minDist) return false;
      }
      grid[cz*side+cx] = (int)pts.size();
      active.push_back((int)pts.size());
      pts.push_back(p);
      return true;
    };

    // Seed with 60 random cluster centres — more seeds = fewer large voids between clusters
    for (int c = 0; c < 60; c++)
      tryAdd({ rngR(-half*0.75f, half*0.75f), rngR(-half*0.75f, half*0.75f) });

    while ((int)pts.size() < 10000 && !active.empty()) {
      // Pick a random active point and try kCands candidates around it
      int ai = (int)(rngF() * (float)active.size()) % (int)active.size();
      glm::vec2 src = pts[active[ai]];
      bool found = false;
      for (int k = 0; k < kCands && !found; k++) {
        float a = rngF() * 6.2832f;
        float r = minDist * (1.0f + rngF());  // [minDist, 2*minDist]
        found = tryAdd(src + r * glm::vec2(std::cos(a), std::sin(a)));
      }
      if (!found) {
        // Swap-and-pop to keep erase O(1)
        active[ai] = active.back();
        active.pop_back();
      }
    }

    // Build instance matrices
    for (auto& p : pts) {
      // Tiny Y jitter so bunnies aren't all exactly coplanar
      glm::vec3 pos = { p.x, rngR(-0.03f, 0.03f), p.y };

      // Fully random 3D rotation: random axis + random angle
      glm::vec3 axis = glm::normalize(glm::vec3(
        rngR(-1.f, 1.f), rngR(-1.f, 1.f), rngR(-1.f, 1.f)));
      float angle = rngR(0.f, 6.2832f);

      glm::mat4 T = glm::translate(glm::mat4(1.f), pos);
      glm::mat4 R = glm::rotate(glm::mat4(1.f), angle, axis);
      allInstances.push(T * R * MODEL_MATRIX);
    }
  }

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
  // 6 storage buffers: meshlet descs, meshlet verts, meshlet tris, vertex data, instances, lod groups
  VkDescriptorPoolSize poolSize{};
  poolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  poolSize.descriptorCount = 6;

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

  VkDescriptorBufferInfo bufs[6] = {};
  bufs[0].buffer = mesh.getMeshletBuffer();         bufs[0].range = VK_WHOLE_SIZE;
  bufs[1].buffer = mesh.getMeshletVertexBuffer();   bufs[1].range = VK_WHOLE_SIZE;
  bufs[2].buffer = mesh.getMeshletTriangleBuffer(); bufs[2].range = VK_WHOLE_SIZE;
  bufs[3].buffer = registry.getVertexBuffer();      bufs[3].range = VK_WHOLE_SIZE;
  bufs[4].buffer = allInstances.getBuffer();        bufs[4].range = VK_WHOLE_SIZE;
  bufs[5].buffer = mesh.getLodGroupBuffer();        bufs[5].range = VK_WHOLE_SIZE;

  VkWriteDescriptorSet writes[6] = {};
  for (int i = 0; i < 6; i++) {
    writes[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[i].dstSet          = descriptorSet;
    writes[i].dstBinding      = (uint32_t)i;
    writes[i].descriptorCount = 1;
    writes[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[i].pBufferInfo     = &bufs[i];
  }
  vkUpdateDescriptorSets(device.getDevice(), 6, writes, 0, nullptr);
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

  float halfFovRad = glm::radians(Camera::getFovY() * 0.5f);

  MeshPushConstants pc{};
  pc.vp            = Camera::getMVP(aspect);
  pc.meshCenter    = mesh.getCentroid();
  pc.meshRadius    = mesh.getRadius();
  pc.cameraPos     = Camera::getPosition();
  pc.meshletCount  = mesh.getMeshletCount();
  pc.renderMode    = (uint32_t)renderMode;
  pc.cotHalfFovH   = (ext.height * 0.5f) / std::tan(halfFovRad);
  pc.lodThreshold  = debug_settings.lod_error_threshold;
  pc.forceLod      = debug_settings.force_lod;

  imgui.profilerUpdate(dt);
  imgui.beginFrame();
  ImGui::Begin("Debug");
  ImGui::Text("Instances: 10000 | Meshlets/mesh: %u", mesh.getMeshletCount());
  static const char* renderModes[] = { "Solid", "Clusters", "LODs", "Triangles" };
  ImGui::Combo("Render Mode", &renderMode, renderModes, 4);
  ImGui::Separator();
  ImGui::Text("LOD Settings");
  ImGui::SliderFloat("Error Threshold (px)", &debug_settings.lod_error_threshold, 0.1f, 10.0f, "%.1f");
  ImGui::SliderInt("Force LOD",
                   reinterpret_cast<int*>(&debug_settings.force_lod),
                   0, 8,
                   debug_settings.force_lod == 0 ? "Auto" : "%d");
  ImGui::Separator();
  ImGui::Checkbox("Profile Window", &showProfiler);
  ImGui::End();
  if (showProfiler) imgui.drawProfileWindow(showProfiler);
  imgui.endFrame();

  commands.record(imageIndex, swapchain, renderPass, pipeline,
                  swapChainFramebuffers[imageIndex],
                  descriptorSet, pc, 10000,
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
  if (descriptorPool) {
    vkDestroyDescriptorPool(device.getDevice(), descriptorPool, nullptr);
    descriptorPool = VK_NULL_HANDLE;
  }
  allInstances.destroy(allocator);
  mesh.destroy(allocator);
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

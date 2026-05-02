#include "commands.h"
#include <stdexcept>
#include <array>
#include <iostream>
#include "sync.h"

void VulkanCommands::createCommandPool(const VulkanDevice& device)
{
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = device.getQueueFamilyIndices().graphicsFamily.value();
  poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  if (vkCreateCommandPool(device.getDevice(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
    throw std::runtime_error("failed to create command pool!");

  pfnDrawMeshTasks = (PFN_vkCmdDrawMeshTasksEXT)vkGetDeviceProcAddr(device.getDevice(), "vkCmdDrawMeshTasksEXT");
  if (!pfnDrawMeshTasks)
    throw std::runtime_error("vkCmdDrawMeshTasksEXT not available — GPU may not support VK_EXT_mesh_shader");

  std::cout << "Created Command Pool\n";
}

void VulkanCommands::allocateCommandBuffers(const VulkanDevice& device, uint32_t count)
{
  commandBuffers.resize(count);

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool        = commandPool;
  allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = count;

  if (vkAllocateCommandBuffers(device.getDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS)
    throw std::runtime_error("failed to allocate command buffers!");
}

void VulkanCommands::record(uint32_t i,
                             const VulkanSwapchain& swapchain,
                             const VulkanRenderPass& renderPass,
                             const VulkanPipeline& pipeline,
                             VkFramebuffer framebuffer,
                             VkDescriptorSet descriptorSet,
                             const MeshPushConstants& pc,
                             uint32_t instanceCount,
                             std::function<void(VkCommandBuffer)> imguiDraw)
{
  VkCommandBuffer cmd = commandBuffers[i];
  vkResetCommandBuffer(cmd, 0);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS)
    throw std::runtime_error("failed to begin recording command buffer!");

  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color        = { {0.f, 0.f, 0.f, 1.f} };
  clearValues[1].depthStencil = { 1.f, 0 };

  VkRenderPassBeginInfo rpBegin{};
  rpBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rpBegin.renderPass        = renderPass.getRenderPass();
  rpBegin.framebuffer       = framebuffer;
  rpBegin.renderArea.offset = {0, 0};
  rpBegin.renderArea.extent = swapchain.getExtent();
  rpBegin.clearValueCount   = (uint32_t)clearValues.size();
  rpBegin.pClearValues      = clearValues.data();

  vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline.getLayout(), 0, 1, &descriptorSet, 0, nullptr);
  vkCmdPushConstants(cmd, pipeline.getLayout(),
                     VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
                     0, sizeof(MeshPushConstants), &pc);

  VkViewport viewport{};
  viewport.width    = (float)swapchain.getExtent().width;
  viewport.height   = (float)swapchain.getExtent().height;
  viewport.minDepth = 0.f;
  viewport.maxDepth = 1.f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.extent = swapchain.getExtent();
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  pfnDrawMeshTasks(cmd, instanceCount, 1, 1);

  if (imguiDraw)
    imguiDraw(cmd);

  vkCmdEndRenderPass(cmd);

  if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
    throw std::runtime_error("failed to record command buffer!");
}

void VulkanCommands::destroy(const VulkanDevice& device)
{
  if (commandPool) {
    vkDestroyCommandPool(device.getDevice(), commandPool, nullptr);
    commandPool = VK_NULL_HANDLE;
  }
  commandBuffers.clear();
  std::cerr << "Command Pool Destroyed\n";
}

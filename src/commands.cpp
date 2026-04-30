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
                             const VulkanMesh& mesh,
                             glm::mat4 mvp)
{
  vkResetCommandBuffer(commandBuffers[i], 0);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS)
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

  vkCmdBeginRenderPass(commandBuffers[i], &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());

  vkCmdPushConstants(commandBuffers[i], pipeline.getLayout(),
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mvp);

  VkBuffer     vbs[]     = { mesh.getVertexBuffer() };
  VkDeviceSize offsets[] = { 0 };
  vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vbs, offsets);
  vkCmdBindIndexBuffer(commandBuffers[i], mesh.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

  VkViewport viewport{};
  viewport.x        = 0.f;
  viewport.y        = 0.f;
  viewport.width    = (float)swapchain.getExtent().width;
  viewport.height   = (float)swapchain.getExtent().height;
  viewport.minDepth = 0.f;
  viewport.maxDepth = 1.f;
  vkCmdSetViewport(commandBuffers[i], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = swapchain.getExtent();
  vkCmdSetScissor(commandBuffers[i], 0, 1, &scissor);

  vkCmdDrawIndexed(commandBuffers[i], mesh.getIndexCount(), 1, 0, 0, 0);

  vkCmdEndRenderPass(commandBuffers[i]);

  if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
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

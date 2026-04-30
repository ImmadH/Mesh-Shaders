#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>
#include "device.h"
const int MAX_FRAMES_IN_FLIGHT = 2;
class VulkanSync
{
public:
  void createSyncObjects(const VulkanDevice& device);

  void destroy(const VulkanDevice& device);
  const std::vector<VkSemaphore>& getImageAvailableSemaphore() const { return imageAvailableSemaphore; }
  const std::vector<VkSemaphore>& getRenderFinishedSemaphore() const { return renderFinishedSemaphore; }
  const std::vector<VkFence>&  getInFlightFence()  const { return inFlightFence; }

  VkSemaphore imageAvailable(uint32_t frame) const {return imageAvailableSemaphore[frame];}
  VkSemaphore renderFinished(uint32_t frame) const {return renderFinishedSemaphore[frame];}
  VkFence     inFlight(uint32_t frame) const {return inFlightFence[frame];}
  

private:
  std::vector<VkSemaphore> imageAvailableSemaphore;
  std::vector<VkSemaphore> renderFinishedSemaphore;
  std::vector<VkFence> inFlightFence;
};

#pragma once
#include <vulkan/vulkan_core.h>

struct GLFWwindow;

class ImGuiManager
{
public:
    bool init(GLFWwindow* window,
              VkInstance instance,
              VkPhysicalDevice physicalDevice,
              VkDevice device,
              uint32_t queueFamily,
              VkQueue queue,
              uint32_t imageCount,
              VkRenderPass renderPass);
    void shutdown();

    void beginFrame();
    void endFrame();
    void render(VkCommandBuffer cmd);

private:
    void uploadFonts(uint32_t queueFamily);

    VkDevice         m_device = VK_NULL_HANDLE;
    VkQueue          m_queue  = VK_NULL_HANDLE;
    VkDescriptorPool m_pool   = VK_NULL_HANDLE;
};

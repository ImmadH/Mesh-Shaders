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

    void profilerUpdate(float dt);
    void drawProfileWindow(bool& show);

private:
    void uploadFonts(uint32_t queueFamily);

    VkDevice         m_device = VK_NULL_HANDLE;
    VkQueue          m_queue  = VK_NULL_HANDLE;
    VkDescriptorPool m_pool   = VK_NULL_HANDLE;

    static constexpr int k_historySize = 300;
    float m_frameTimes[k_historySize] = {};
    float m_fpsHistory[k_historySize] = {};
    int   m_frameTimeHead = 0;
    int   m_frameCount    = 0;
    float m_dFps          = 0.0f;
    float m_dFrameMs      = 0.0f;
    float m_dAvg          = 0.0f;
    float m_refreshAccum  = 0.0f;
};

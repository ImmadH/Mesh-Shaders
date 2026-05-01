#include "imgui_manager.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <stdexcept>


bool ImGuiManager::init(GLFWwindow* window,
                         VkInstance instance,
                         VkPhysicalDevice physicalDevice,
                         VkDevice device,
                         uint32_t queueFamily,
                         VkQueue queue,
                         uint32_t imageCount,
                         VkRenderPass renderPass)
{
    m_device = device;
    m_queue  = queue;

    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 16;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets       = 16;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_pool) != VK_SUCCESS)
        return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    float xscale = 1.0f, yscale = 1.0f;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    float scale = xscale;

    ImFontConfig fontCfg{};
    fontCfg.SizePixels = roundf(13.0f * scale);
    ImGui::GetIO().Fonts->AddFontDefault(&fontCfg);
    ImGui::GetStyle().ScaleAllSizes(scale);

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion                  = VK_API_VERSION_1_3;
    initInfo.Instance                    = instance;
    initInfo.PhysicalDevice              = physicalDevice;
    initInfo.Device                      = device;
    initInfo.QueueFamily                 = queueFamily;
    initInfo.Queue                       = queue;
    initInfo.DescriptorPool              = m_pool;
    initInfo.MinImageCount               = imageCount;
    initInfo.ImageCount                  = imageCount;
    initInfo.PipelineInfoMain.RenderPass = renderPass;
    initInfo.CheckVkResultFn             = nullptr;

    if (!ImGui_ImplVulkan_Init(&initInfo))
        return false;

    uploadFonts(queueFamily);

    return true;
}

void ImGuiManager::uploadFonts(uint32_t queueFamily)
{
    VkCommandPool   pool{};
    VkCommandBuffer cb{};

    VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = queueFamily;
    vkCreateCommandPool(m_device, &poolInfo, nullptr, &pool);

    VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.commandPool        = pool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(m_device, &allocInfo, &cb);

    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &beginInfo);
    vkEndCommandBuffer(cb);

    VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cb;
    vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);

    vkFreeCommandBuffers(m_device, pool, 1, &cb);
    vkDestroyCommandPool(m_device, pool, nullptr);
}

void ImGuiManager::shutdown()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (m_pool) {
        vkDestroyDescriptorPool(m_device, m_pool, nullptr);
        m_pool = VK_NULL_HANDLE;
    }
}

void ImGuiManager::beginFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::endFrame()
{
    ImGui::Render();
}

void ImGuiManager::render(VkCommandBuffer cmd)
{
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

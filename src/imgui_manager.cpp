#include "imgui_manager.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
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

void ImGuiManager::profilerUpdate(float dt)
{
    float frameMs = dt * 1000.0f;
    m_frameTimes[m_frameTimeHead] = frameMs;
    m_fpsHistory[m_frameTimeHead] = dt > 0.0f ? 1.0f / dt : 0.0f;
    m_frameTimeHead = (m_frameTimeHead + 1) % k_historySize;
    if (m_frameCount < k_historySize) ++m_frameCount;

    m_refreshAccum += dt;
    if (m_refreshAccum >= 0.3f)
    {
        float sum = 0.0f;
        for (int i = 0; i < m_frameCount; i++) sum += m_frameTimes[i];
        m_dAvg        = m_frameCount > 0 ? sum / m_frameCount : 0.0f;
        m_dFps        = dt > 0.0f ? 1.0f / dt : 0.0f;
        m_dFrameMs    = frameMs;
        m_refreshAccum = 0.0f;
    }
}

void ImGuiManager::drawProfileWindow(bool& show)
{
    ImGui::SetNextWindowSize(ImVec2(460, 370), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(300, 30),   ImGuiCond_FirstUseEver);
    ImGui::Begin("Profile", &show);

    ImGui::Text("FPS:");        ImGui::SameLine(220); ImGui::Text("%.1f",    m_dFps);
    ImGui::Text("Frame Time:"); ImGui::SameLine(220); ImGui::Text("%.2f ms", m_dFrameMs);
    ImGui::Separator();
    float avgFps = m_dAvg > 0.0f ? 1000.0f / m_dAvg : 0.0f;
    ImGui::Text("Avg  %.2f ms", m_dAvg); ImGui::SameLine(300); ImGui::Text("%.1f fps", avgFps);
    ImGui::Spacing();

    ImDrawList* dl     = ImGui::GetWindowDrawList();
    float       fontSz = ImGui::GetFontSize();
    float       availW = ImGui::GetContentRegionAvail().x;
    float       graphW = availW - fontSz * 3.2f - 4.0f;

    // FPS line graph
    ImGui::TextDisabled("Frame Rate");
    {
        float  graphH = 140.0f;
        ImVec2 p0     = ImGui::GetCursorScreenPos();
        ImVec2 p1     = ImVec2(p0.x + graphW, p0.y + graphH);

        dl->AddRectFilled(p0, p1, IM_COL32(12, 12, 18, 255));

        static constexpr float k_fpsScale  = 300.0f;
        static constexpr float k_gridFps[] = { 60.f, 120.f, 180.f, 240.f };
        for (float g : k_gridFps)
        {
            float gy = p1.y - (g / k_fpsScale) * graphH;
            dl->AddLine(ImVec2(p0.x, gy), ImVec2(p1.x, gy), IM_COL32(45, 45, 60, 255));
            char buf[8]; snprintf(buf, sizeof(buf), "%d", (int)g);
            dl->AddText(ImVec2(p1.x + 4.f, gy - fontSz * 0.5f), IM_COL32(120, 120, 150, 255), buf);
        }

        float xStep = graphW / (float)(k_historySize - 1);
        for (int i = 0; i < k_historySize - 1; i++)
        {
            int   ia = (m_frameTimeHead + i)     % k_historySize;
            int   ib = (m_frameTimeHead + i + 1) % k_historySize;
            float fa = std::min(m_fpsHistory[ia] / k_fpsScale, 1.0f);
            float fb = std::min(m_fpsHistory[ib] / k_fpsScale, 1.0f);
            float x0 = p0.x + i       * xStep;
            float x1 = p0.x + (i + 1) * xStep;
            float y0 = p1.y - fa * graphH;
            float y1 = p1.y - fb * graphH;
            dl->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(70, 150, 255, 230), 1.5f);
        }

        dl->AddRect(p0, p1, IM_COL32(50, 50, 70, 255));
        ImGui::Dummy(ImVec2(availW, graphH));
        ImGui::Spacing();
    }

    // Frame-time bar graph
    ImGui::TextDisabled("Frame Time (ms)");
    {
        float  insetH   = 60.0f;
        ImVec2 p0       = ImGui::GetCursorScreenPos();
        ImVec2 p1       = ImVec2(p0.x + graphW, p0.y + insetH);
        float  ftScale  = 50.0f;
        float  targetMs = 16.667f;

        dl->AddRectFilled(p0, p1, IM_COL32(12, 12, 18, 255));

        float refY = p1.y - (targetMs / ftScale) * insetH;
        dl->AddLine(ImVec2(p0.x, refY), ImVec2(p1.x, refY), IM_COL32(70, 200, 70, 90));
        dl->AddText(ImVec2(p1.x + 4.f, refY - fontSz * 0.5f), IM_COL32(70, 200, 70, 200), "16ms");

        float barW = graphW / (float)k_historySize;
        for (int i = 0; i < k_historySize; i++)
        {
            int   idx = (m_frameTimeHead + i) % k_historySize;
            float ms  = m_frameTimes[idx];
            float t   = std::min(ms / ftScale, 1.0f);
            float x0  = p0.x + i * barW;
            float x1  = x0 + std::max(barW - 1.0f, 1.0f);
            float y0  = p1.y - t * insetH;
            ImU32 col = ms > targetMs * 2.0f ? IM_COL32(255, 70, 70, 210) : IM_COL32(80, 150, 255, 180);
            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, p1.y), col);
        }

        dl->AddRect(p0, p1, IM_COL32(50, 50, 70, 255));
        ImGui::Dummy(ImVec2(availW, insetH));
    }

    ImGui::End();
}

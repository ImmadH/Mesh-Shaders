#pragma once
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>
#include <string>
#include "pipeline.h"

class VulkanMesh
{
public:
    void create(VmaAllocator allocator, const std::string& path);
    void destroy(VmaAllocator allocator);

    VkBuffer getVertexBuffer() const { return vertexBuffer; }
    VkBuffer getIndexBuffer()  const { return indexBuffer; }
    uint32_t getIndexCount()   const { return indexCount; }

private:
    VkBuffer      vertexBuffer     = VK_NULL_HANDLE;
    VmaAllocation vertexAllocation = VK_NULL_HANDLE;
    VkBuffer      indexBuffer      = VK_NULL_HANDLE;
    VmaAllocation indexAllocation  = VK_NULL_HANDLE;
    uint32_t      indexCount       = 0;
};

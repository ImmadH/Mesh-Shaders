#pragma once
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "pipeline.h"

// MeshRegistry

class MeshRegistry
{
public:
    void init(VmaAllocator allocator, uint32_t maxVerts = 1 << 20, uint32_t maxIndices = 1 << 22);
    void destroy(VmaAllocator allocator);

    struct Upload { uint32_t firstVertex; uint32_t firstIndex; };
    Upload upload(const std::vector<Vertex>& verts, const std::vector<uint32_t>& idxs);

    VkBuffer getVertexBuffer() const { return vertexBuffer; }
    VkBuffer getIndexBuffer()  const { return indexBuffer; }

private:
    VkBuffer      vertexBuffer     = VK_NULL_HANDLE;
    VmaAllocation vertexAllocation = VK_NULL_HANDLE;
    VkBuffer      indexBuffer      = VK_NULL_HANDLE;
    VmaAllocation indexAllocation  = VK_NULL_HANDLE;
    void*         vertexMapped     = nullptr;
    void*         indexMapped      = nullptr;
    uint32_t      nextVertex       = 0;
    uint32_t      nextIndex        = 0;
};

// InstanceBuffer

class InstanceBuffer
{
public:
    void     init(VmaAllocator allocator, uint32_t maxInstances = 1 << 16);
    void     destroy(VmaAllocator allocator);
    uint32_t push(const glm::mat4& transform);
    void     reset();

    VkBuffer getBuffer() const { return buffer; }
    uint32_t getCount()  const { return count; }

private:
    VkBuffer      buffer     = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    glm::mat4*    mapped     = nullptr;
    uint32_t      count      = 0;
};

// VulkanMesh

class VulkanMesh
{
public:
    void create(MeshRegistry& registry, const std::string& path);

    uint32_t  getFirstVertex() const { return firstVertex; }
    uint32_t  getFirstIndex()  const { return firstIndex; }
    uint32_t  getIndexCount()  const { return indexCount; }
    glm::vec3 getCentroid()    const { return centroid; }
    float     getRadius()      const { return radius; }

private:
    uint32_t  firstVertex = 0;
    uint32_t  firstIndex  = 0;
    uint32_t  indexCount  = 0;
    glm::vec3 centroid    = {};
    float     radius      = 0.f;
};

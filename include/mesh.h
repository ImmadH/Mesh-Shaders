#pragma once
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "pipeline.h"

struct MeshletDesc {
    uint32_t  vertexOffset;    // 0
    uint32_t  triangleOffset;  // 4
    uint32_t  vertexCount;     // 8
    uint32_t  triangleCount;   // 12
    glm::vec3 center;          // 16  per-meshlet sphere center (culling)
    float     radius;          // 28
    glm::vec3 coneAxis;        // 32
    float     coneCutoff;      // 44
    int32_t   groupId;         // 48  index into LodGroupData array (this meshlet's group)
    int32_t   parentGroupId;   // 52  finer group simplified to produce this (-1 = original)
    uint32_t  lodLevel;        // 56
    uint32_t  _pad;            // 60
};                             // 64 bytes total

// One entry per LOD group produced by clodBuild.
// group.error = error that would result from simplifying this group one level further
//               (FLT_MAX for terminal/root groups that cannot be coarsened further).
// For LOD selection: use group[meshlet.groupId].error for the "going coarser" check,
//                    and group[meshlet.parentGroupId].error for the "my own error" check.
struct LodGroupData {
    glm::vec3 center;    // 0   bounding sphere center
    float     radius;    // 12
    float     error;     // 16  simplified.error from clodBuild
    float     _pad[3];   // 20  → 32 bytes total (16-byte aligned for std430 vec3)
};

// MeshRegistry

class MeshRegistry
{
public:
    void init(VmaAllocator allocator, uint32_t maxVerts = 1 << 20);
    void destroy(VmaAllocator allocator);

    void upload(const std::vector<Vertex>& verts);

    VkBuffer getVertexBuffer() const { return vertexBuffer; }

private:
    VkBuffer      vertexBuffer     = VK_NULL_HANDLE;
    VmaAllocation vertexAllocation = VK_NULL_HANDLE;
    void*         vertexMapped     = nullptr;
    uint32_t      nextVertex       = 0;
};

// InstanceBuffer

class InstanceBuffer
{
public:
    void     init(VmaAllocator allocator, uint32_t maxInstances = 1 << 16);
    void     destroy(VmaAllocator allocator);
    uint32_t push(const glm::mat4& transform);

    VkBuffer getBuffer() const { return buffer; }

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
    void create(MeshRegistry& registry, VmaAllocator allocator, const std::string& path);
    void destroy(VmaAllocator allocator);

    glm::vec3 getCentroid()      const { return centroid; }
    float     getRadius()        const { return radius; }
    uint32_t  getMeshletCount()  const { return meshletCount; }

    VkBuffer getMeshletBuffer()          const { return meshletBuffer; }
    VkBuffer getMeshletVertexBuffer()    const { return meshletVertexBuffer; }
    VkBuffer getMeshletTriangleBuffer()  const { return meshletTriangleBuffer; }
    VkBuffer getLodGroupBuffer()         const { return lodGroupBuffer; }

private:
    void uploadBuffer(VmaAllocator allocator, const void* data, VkDeviceSize size,
                      VkBufferUsageFlags usage, VkBuffer& buf, VmaAllocation& alloc);

    glm::vec3 centroid     = {};
    float     radius       = 0.f;

    uint32_t      meshletCount          = 0;
    VkBuffer      meshletBuffer         = VK_NULL_HANDLE;
    VmaAllocation meshletAllocation     = VK_NULL_HANDLE;
    VkBuffer      meshletVertexBuffer   = VK_NULL_HANDLE;
    VmaAllocation meshletVertexAlloc    = VK_NULL_HANDLE;
    VkBuffer      meshletTriangleBuffer = VK_NULL_HANDLE;
    VmaAllocation meshletTriangleAlloc  = VK_NULL_HANDLE;
    VkBuffer      lodGroupBuffer        = VK_NULL_HANDLE;
    VmaAllocation lodGroupAlloc         = VK_NULL_HANDLE;
};

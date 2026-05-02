#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <meshoptimizer.h>  // must precede CLUSTERLOD_IMPLEMENTATION

#define CLUSTERLOD_IMPLEMENTATION
#include "../vendor/meshoptimizer/demo/clusterlod.h"

#include "mesh.h"
#include <cstring>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>

static constexpr size_t kMaxMeshletVerts     = 64;
static constexpr size_t kMaxMeshletTriangles = 126;

// MeshRegistry

void MeshRegistry::init(VmaAllocator allocator, uint32_t maxVerts)
{
    VkBufferCreateInfo bufInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufInfo.size  = sizeof(Vertex) * maxVerts;
    bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo info;
    if (vmaCreateBuffer(allocator, &bufInfo, &allocInfo, &vertexBuffer, &vertexAllocation, &info) != VK_SUCCESS)
        throw std::runtime_error("MeshRegistry: vmaCreateBuffer failed");
    vertexMapped = info.pMappedData;

    std::cout << "MeshRegistry: " << (sizeof(Vertex) * maxVerts >> 20) << "MB vertex\n";
}

void MeshRegistry::upload(const std::vector<Vertex>& verts)
{
    memcpy((Vertex*)vertexMapped + nextVertex, verts.data(), sizeof(Vertex) * verts.size());
    nextVertex += (uint32_t)verts.size();
}

void MeshRegistry::destroy(VmaAllocator allocator)
{
    if (vertexBuffer) vmaDestroyBuffer(allocator, vertexBuffer, vertexAllocation);
    vertexBuffer = VK_NULL_HANDLE;
    std::cerr << "MeshRegistry Destroyed\n";
}

// InstanceBuffer

void InstanceBuffer::init(VmaAllocator allocator, uint32_t maxInstances)
{
    VkBufferCreateInfo bufInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufInfo.size  = sizeof(glm::mat4) * maxInstances;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo info;
    if (vmaCreateBuffer(allocator, &bufInfo, &allocInfo, &buffer, &allocation, &info) != VK_SUCCESS)
        throw std::runtime_error("InstanceBuffer: vmaCreateBuffer failed");
    mapped = (glm::mat4*)info.pMappedData;
}

uint32_t InstanceBuffer::push(const glm::mat4& transform)
{
    mapped[count] = transform;
    return count++;
}

void InstanceBuffer::destroy(VmaAllocator allocator)
{
    if (buffer) {
        vmaDestroyBuffer(allocator, buffer, allocation);
        buffer = VK_NULL_HANDLE;
    }
    std::cerr << "InstanceBuffer Destroyed\n";
}

// VulkanMesh

void VulkanMesh::uploadBuffer(VmaAllocator allocator, const void* data, VkDeviceSize size,
                               VkBufferUsageFlags usage, VkBuffer& buf, VmaAllocation& alloc)
{
    VkBufferCreateInfo bufInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufInfo.size  = size;
    bufInfo.usage = usage;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo info;
    if (vmaCreateBuffer(allocator, &bufInfo, &allocInfo, &buf, &alloc, &info) != VK_SUCCESS)
        throw std::runtime_error("VulkanMesh: vmaCreateBuffer failed");
    memcpy(info.pMappedData, data, size);
}

void VulkanMesh::create(MeshRegistry& registry, VmaAllocator allocator, const std::string& path)
{
    cgltf_options opts{};
    cgltf_data* gltf = nullptr;

    if (cgltf_parse_file(&opts, path.c_str(), &gltf) != cgltf_result_success)
        throw std::runtime_error("cgltf: failed to parse " + path);
    if (cgltf_load_buffers(&opts, gltf, path.c_str()) != cgltf_result_success) {
        cgltf_free(gltf);
        throw std::runtime_error("cgltf: failed to load buffers for " + path);
    }

    std::vector<Vertex>   verts;
    std::vector<uint32_t> idxs;

    for (size_t i = 0; i < gltf->meshes_count; ++i) {
        const cgltf_mesh& m = gltf->meshes[i];
        for (size_t j = 0; j < m.primitives_count; ++j) {
            const cgltf_primitive& prim = m.primitives[j];

            const cgltf_accessor* pos  = nullptr;
            const cgltf_accessor* norm = nullptr;
            for (size_t k = 0; k < prim.attributes_count; ++k) {
                switch (prim.attributes[k].type) {
                    case cgltf_attribute_type_position: pos  = prim.attributes[k].data; break;
                    case cgltf_attribute_type_normal:   norm = prim.attributes[k].data; break;
                    default: break;
                }
            }
            if (!pos) continue;

            uint32_t base = (uint32_t)verts.size();
            verts.resize(base + pos->count);

            for (size_t k = 0; k < pos->count; ++k) {
                float p[3] = {}, n[3] = {0.f, 1.f, 0.f};
                cgltf_accessor_read_float(pos,  k, p, 3);
                if (norm) cgltf_accessor_read_float(norm, k, n, 3);
                verts[base + k] = { {p[0], p[1], p[2]}, {n[0], n[1], n[2]} };
            }

            if (prim.indices) {
                idxs.reserve(idxs.size() + prim.indices->count);
                for (size_t k = 0; k < prim.indices->count; ++k)
                    idxs.push_back(base + (uint32_t)cgltf_accessor_read_index(prim.indices, k));
            } else {
                for (uint32_t k = 0; k < (uint32_t)pos->count; ++k)
                    idxs.push_back(base + k);
            }
        }
    }
    cgltf_free(gltf);

    if (verts.empty())
        throw std::runtime_error("no geometry found in " + path);

    centroid = {};
    for (const auto& v : verts) centroid += v.pos;
    centroid /= (float)verts.size();

    radius = 0.f;
    for (const auto& v : verts)
        radius = glm::max(radius, glm::length(v.pos - centroid));

    registry.upload(verts);

    // --- LOD DAG build via clusterlod ---
    std::vector<unsigned int> uindices(idxs.begin(), idxs.end());

    clodConfig config = clodDefaultConfig(kMaxMeshletTriangles);
    config.max_vertices = kMaxMeshletVerts;

    clodMesh cmesh{};
    cmesh.indices                  = uindices.data();
    cmesh.index_count              = uindices.size();
    cmesh.vertex_count             = verts.size();
    cmesh.vertex_positions         = &verts[0].pos.x;
    cmesh.vertex_positions_stride  = sizeof(Vertex);
    cmesh.vertex_attributes        = nullptr;
    cmesh.vertex_attributes_stride = 0;
    cmesh.attribute_weights        = nullptr;
    cmesh.attribute_count          = 0;
    cmesh.vertex_lock              = nullptr;
    cmesh.attribute_protect_mask   = 0;

    std::vector<MeshletDesc>  descs;
    std::vector<uint32_t>     allMV;
    std::vector<uint32_t>     allMT;
    std::vector<LodGroupData> lodGroups;

    uint32_t currentMV = 0;
    uint32_t currentMT = 0;

    clodBuild(config, cmesh,
        [&](const clodGroup& group, const clodCluster* clusters, size_t cluster_count) -> int
        {
            int group_id = (int)lodGroups.size();

            LodGroupData lg{};
            lg.center = { group.simplified.center[0],
                           group.simplified.center[1],
                           group.simplified.center[2] };
            lg.radius = group.simplified.radius;
            lg.error  = group.simplified.error;
            lodGroups.push_back(lg);

            for (size_t i = 0; i < cluster_count; ++i)
            {
                const clodCluster& cluster = clusters[i];

                // Precise culling bounds (center, radius, cone)
                meshopt_Bounds b = meshopt_computeClusterBounds(
                    cluster.indices, cluster.index_count,
                    &verts[0].pos.x, verts.size(), sizeof(Vertex));

                MeshletDesc d{};
                d.center     = { b.center[0], b.center[1], b.center[2] };
                d.radius     = b.radius;
                d.coneAxis   = { b.cone_axis[0], b.cone_axis[1], b.cone_axis[2] };
                d.coneCutoff = b.cone_cutoff;

                d.groupId       = group_id;
                d.parentGroupId = cluster.refined;
                d.lodLevel      = (uint32_t)group.depth;

                // Meshlet-local indices
                size_t tri_count = cluster.index_count / 3;
                std::vector<unsigned int>  local_verts(cluster.vertex_count);
                std::vector<unsigned char> local_tris(cluster.index_count);
                size_t unique_verts = clodLocalIndices(
                    local_verts.data(), local_tris.data(),
                    cluster.indices, cluster.index_count);

                d.vertexOffset   = currentMV;
                d.triangleOffset = currentMT;
                d.vertexCount    = (uint32_t)unique_verts;
                d.triangleCount  = (uint32_t)tri_count;

                for (size_t v = 0; v < unique_verts; ++v)
                    allMV.push_back(local_verts[v]);
                currentMV += (uint32_t)unique_verts;

                for (size_t t = 0; t < cluster.index_count; ++t)
                    allMT.push_back(local_tris[t]);
                currentMT += (uint32_t)cluster.index_count;

                descs.push_back(d);
            }

            return group_id;
        });

    meshletCount = (uint32_t)descs.size();

    uploadBuffer(allocator, descs.data(),    sizeof(MeshletDesc)  * descs.size(),
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, meshletBuffer,         meshletAllocation);
    uploadBuffer(allocator, allMV.data(),    sizeof(uint32_t)     * allMV.size(),
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, meshletVertexBuffer,   meshletVertexAlloc);
    uploadBuffer(allocator, allMT.data(),    sizeof(uint32_t)     * allMT.size(),
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, meshletTriangleBuffer, meshletTriangleAlloc);
    uploadBuffer(allocator, lodGroups.data(), sizeof(LodGroupData) * lodGroups.size(),
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, lodGroupBuffer,         lodGroupAlloc);

    std::cout << "Meshlets: " << descs.size()
              << " | LOD groups: " << lodGroups.size()
              << " | MV: " << allMV.size()
              << " | MT: " << allMT.size() / 3 << " tris\n";
}

void VulkanMesh::destroy(VmaAllocator allocator)
{
    if (lodGroupBuffer)        vmaDestroyBuffer(allocator, lodGroupBuffer,        lodGroupAlloc);
    if (meshletTriangleBuffer) vmaDestroyBuffer(allocator, meshletTriangleBuffer, meshletTriangleAlloc);
    if (meshletVertexBuffer)   vmaDestroyBuffer(allocator, meshletVertexBuffer,   meshletVertexAlloc);
    if (meshletBuffer)         vmaDestroyBuffer(allocator, meshletBuffer,          meshletAllocation);
    lodGroupBuffer        = VK_NULL_HANDLE;
    meshletTriangleBuffer = VK_NULL_HANDLE;
    meshletVertexBuffer   = VK_NULL_HANDLE;
    meshletBuffer         = VK_NULL_HANDLE;
}

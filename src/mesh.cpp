#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include "mesh.h"
#include <cstring>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

// MeshRegistry

void MeshRegistry::init(VmaAllocator allocator, uint32_t maxVerts, uint32_t maxIndices)
{
    auto alloc = [&](VkDeviceSize size, VkBufferUsageFlags usage,
                     VkBuffer& buf, VmaAllocation& mem, void*& mapped)
    {
        VkBufferCreateInfo bufInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufInfo.size  = size;
        bufInfo.usage = usage;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo info;
        if (vmaCreateBuffer(allocator, &bufInfo, &allocInfo, &buf, &mem, &info) != VK_SUCCESS)
            throw std::runtime_error("MeshRegistry: vmaCreateBuffer failed");
        mapped = info.pMappedData;
    };

    alloc(sizeof(Vertex)   * maxVerts,   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
          vertexBuffer, vertexAllocation, vertexMapped);
    alloc(sizeof(uint32_t) * maxIndices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
          indexBuffer,  indexAllocation,  indexMapped);

    std::cout << "MeshRegistry: " << (sizeof(Vertex) * maxVerts >> 20)
              << "MB vertex + " << (sizeof(uint32_t) * maxIndices >> 20)
              << "MB index\n";
}

MeshRegistry::Upload MeshRegistry::upload(const std::vector<Vertex>& verts,
                                           const std::vector<uint32_t>& idxs)
{
    Upload out{ nextVertex, nextIndex };

    memcpy((Vertex*)vertexMapped   + nextVertex, verts.data(), sizeof(Vertex)   * verts.size());
    memcpy((uint32_t*)indexMapped  + nextIndex,  idxs.data(),  sizeof(uint32_t) * idxs.size());

    nextVertex += (uint32_t)verts.size();
    nextIndex  += (uint32_t)idxs.size();
    return out;
}

void MeshRegistry::destroy(VmaAllocator allocator)
{
    if (indexBuffer)  vmaDestroyBuffer(allocator, indexBuffer,  indexAllocation);
    if (vertexBuffer) vmaDestroyBuffer(allocator, vertexBuffer, vertexAllocation);
    indexBuffer  = VK_NULL_HANDLE;
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

void InstanceBuffer::reset()
{
    count = 0;
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

void VulkanMesh::create(MeshRegistry& registry, const std::string& path)
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

    auto [fv, fi] = registry.upload(verts, idxs);
    firstVertex = fv;
    firstIndex  = fi;
    indexCount  = (uint32_t)idxs.size();
}

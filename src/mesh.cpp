#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include "mesh.h"
#include <cstring>
#include <stdexcept>
#include <vector>
#include <iostream>

void VulkanMesh::create(VmaAllocator allocator, const std::string& path)
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

    for (size_t mi = 0; mi < gltf->meshes_count; ++mi) {
        const cgltf_mesh& m = gltf->meshes[mi];
        for (size_t pi = 0; pi < m.primitives_count; ++pi) {
            const cgltf_primitive& prim = m.primitives[pi];

            const cgltf_accessor* posAcc  = nullptr;
            const cgltf_accessor* normAcc = nullptr;
            for (size_t ai = 0; ai < prim.attributes_count; ++ai) {
                switch (prim.attributes[ai].type) {
                    case cgltf_attribute_type_position: posAcc  = prim.attributes[ai].data; break;
                    case cgltf_attribute_type_normal:   normAcc = prim.attributes[ai].data; break;
                    default: break;
                }
            }
            if (!posAcc) continue;

            uint32_t base = (uint32_t)verts.size();
            verts.resize(base + posAcc->count);

            for (size_t vi = 0; vi < posAcc->count; ++vi) {
                float p[3] = {}, n[3] = {0.f, 1.f, 0.f};
                cgltf_accessor_read_float(posAcc, vi, p, 3);
                if (normAcc) cgltf_accessor_read_float(normAcc, vi, n, 3);
                verts[base + vi] = { {p[0], p[1], p[2]}, {n[0], n[1], n[2]} };
            }

            if (prim.indices) {
                idxs.reserve(idxs.size() + prim.indices->count);
                for (size_t ii = 0; ii < prim.indices->count; ++ii)
                    idxs.push_back(base + (uint32_t)cgltf_accessor_read_index(prim.indices, ii));
            } else {
                for (uint32_t vi = 0; vi < (uint32_t)posAcc->count; ++vi)
                    idxs.push_back(base + vi);
            }
        }
    }
    cgltf_free(gltf);

    if (verts.empty())
        throw std::runtime_error("no geometry found in " + path);

    indexCount = (uint32_t)idxs.size();

    auto upload = [&](VkDeviceSize size, VkBufferUsageFlags usage,
                      VkBuffer& buf, VmaAllocation& alloc, const void* src)
    {
        VkBufferCreateInfo bi{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bi.size  = size;
        bi.usage = usage;
        VmaAllocationCreateInfo ai{};
        ai.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        if (vmaCreateBuffer(allocator, &bi, &ai, &buf, &alloc, nullptr) != VK_SUCCESS)
            throw std::runtime_error("vmaCreateBuffer failed");
        void* mapped;
        vmaMapMemory(allocator, alloc, &mapped);
        memcpy(mapped, src, (size_t)size);
        vmaUnmapMemory(allocator, alloc);
    };

    upload(sizeof(Vertex)   * verts.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
           vertexBuffer, vertexAllocation, verts.data());
    upload(sizeof(uint32_t) * idxs.size(),  VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
           indexBuffer,  indexAllocation,  idxs.data());

}

void VulkanMesh::destroy(VmaAllocator allocator)
{
    if (indexBuffer) {
        vmaDestroyBuffer(allocator, indexBuffer, indexAllocation);
        indexBuffer     = VK_NULL_HANDLE;
        indexAllocation = VK_NULL_HANDLE;
    }
    if (vertexBuffer) {
        vmaDestroyBuffer(allocator, vertexBuffer, vertexAllocation);
        vertexBuffer     = VK_NULL_HANDLE;
        vertexAllocation = VK_NULL_HANDLE;
    }
    std::cerr << "Mesh Buffers Destroyed\n";
}

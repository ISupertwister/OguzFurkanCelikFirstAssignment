#include "GLTFLoader.h"
#include "tiny_gltf.h"
#include <cassert>
#include <cstring>

static bool ReadAccessorFloat(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    std::vector<float>& out)
{
    const tinygltf::BufferView& bv = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buf = model.buffers[bv.buffer];

    const unsigned char* data = buf.data.data() + bv.byteOffset + accessor.byteOffset;

    // Only supports tightly packed float accessors for assignment.
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) return false;

    int compCount = 0;
    if (accessor.type == TINYGLTF_TYPE_VEC2) compCount = 2;
    else if (accessor.type == TINYGLTF_TYPE_VEC3) compCount = 3;
    else if (accessor.type == TINYGLTF_TYPE_VEC4) compCount = 4;
    else return false;

    out.resize(accessor.count * compCount);

    const size_t stride = bv.byteStride ? bv.byteStride : sizeof(float) * compCount;
    for (size_t i = 0; i < accessor.count; ++i)
    {
        std::memcpy(&out[i * compCount], data + i * stride, sizeof(float) * compCount);
    }

    return true;
}

static bool ReadIndicesU32(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    std::vector<uint32_t>& out)
{
    const tinygltf::BufferView& bv = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buf = model.buffers[bv.buffer];
    const unsigned char* data = buf.data.data() + bv.byteOffset + accessor.byteOffset;

    out.resize(accessor.count);

    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
    {
        const uint16_t* src = reinterpret_cast<const uint16_t*>(data);
        for (size_t i = 0; i < accessor.count; ++i) out[i] = (uint32_t)src[i];
        return true;
    }
    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
    {
        const uint32_t* src = reinterpret_cast<const uint32_t*>(data);
        for (size_t i = 0; i < accessor.count; ++i) out[i] = src[i];
        return true;
    }

    return false;
}

namespace GLTFLoader
{
    bool LoadFirstMesh(
        const std::string& path,
        std::vector<DXModelMesh::Vertex>& outVertices,
        std::vector<uint32_t>& outIndices)
    {
        tinygltf::TinyGLTF loader;
        tinygltf::Model model;
        std::string err, warn;

        bool ok = false;
        const bool isGLB = path.size() >= 4 && (path.substr(path.size() - 4) == ".glb");
        if (isGLB) ok = loader.LoadBinaryFromFile(&model, &err, &warn, path);
        else ok = loader.LoadASCIIFromFile(&model, &err, &warn, path);

        if (!ok) return false;
        if (model.meshes.empty()) return false;
        if (model.meshes[0].primitives.empty()) return false;

        const tinygltf::Primitive& prim = model.meshes[0].primitives[0];

        auto itPos = prim.attributes.find("POSITION");
        auto itNrm = prim.attributes.find("NORMAL");
        auto itUv = prim.attributes.find("TEXCOORD_0");

        if (itPos == prim.attributes.end()) return false;
        if (itNrm == prim.attributes.end()) return false;
        if (itUv == prim.attributes.end()) return false;
        if (prim.indices < 0) return false;

        std::vector<float> pos, nrm, uv;
        if (!ReadAccessorFloat(model, model.accessors[itPos->second], pos)) return false;
        if (!ReadAccessorFloat(model, model.accessors[itNrm->second], nrm)) return false;
        if (!ReadAccessorFloat(model, model.accessors[itUv->second], uv))  return false;

        if (!ReadIndicesU32(model, model.accessors[prim.indices], outIndices)) return false;

        const size_t vCount = model.accessors[itPos->second].count;
        outVertices.resize(vCount);

        for (size_t i = 0; i < vCount; ++i)
        {
            DXModelMesh::Vertex v{};
            v.position = { pos[i * 3 + 0], pos[i * 3 + 1], pos[i * 3 + 2] };
            v.normal = { nrm[i * 3 + 0], nrm[i * 3 + 1], nrm[i * 3 + 2] };
            v.uv = { uv[i * 2 + 0],  uv[i * 2 + 1] };
            outVertices[i] = v;
        }

        return true;
    }
}

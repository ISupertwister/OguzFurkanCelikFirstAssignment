#pragma once
#include <string>
#include <vector>
#include "DXModelMesh.h"

// Minimal glTF loader for assignment: loads first mesh/primitive with POSITION/NORMAL/TEXCOORD_0.
namespace GLTFLoader
{
    bool LoadFirstMesh(
        const std::string& path,
        std::vector<DXModelMesh::Vertex>& outVertices,
        std::vector<uint32_t>& outIndices);
}

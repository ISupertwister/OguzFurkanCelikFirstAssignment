#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>

// Simple mesh class that owns a vertex buffer for a textured quad.
class DXMesh
{
public:
    struct Vertex
    {
        DirectX::XMFLOAT3 position; // Vertex position in object space.
        DirectX::XMFLOAT3 color;    // Vertex color for gradient.
        DirectX::XMFLOAT2 uv;       // Texture coordinates.
    };

    DXMesh() = default;

    // Creates a simple 2x2 quad (two triangles) in an upload heap.
    bool InitializeQuad(ID3D12Device* device);

    // Releases GPU resources.
    void Destroy();

    // Bind the VB and issue a DrawInstanced call.
    void Draw(ID3D12GraphicsCommandList* cmdList) const;

    // Accessors (useful for debug if needed).
    const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return m_vbView; }
    UINT GetVertexCount() const { return m_vertexCount; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer{};
    D3D12_VERTEX_BUFFER_VIEW               m_vbView{};
    UINT                                   m_vertexCount = 0;
};

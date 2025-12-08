#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <cstdint>

// Forward declaration
class DXDevice;

// Simple mesh: vertex buffer for a colored/UV triangle or quad.
class DXMesh
{
public:
    DXMesh() noexcept = default;
    ~DXMesh() = default;

    // Comment in English: Create a simple triangle mesh (pos/color/uv).
    bool InitializeTriangle(DXDevice* device) noexcept;

    // Comment in English: Accessors used by the renderer.
    D3D12_VERTEX_BUFFER_VIEW GetVBView() const noexcept { return m_vbView; }
    UINT GetVertexCount() const noexcept { return m_vertexCount; }

private:
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 color;
        DirectX::XMFLOAT2 uv;
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer{};
    D3D12_VERTEX_BUFFER_VIEW               m_vbView{};
    UINT                                   m_vertexCount{ 0 };
};

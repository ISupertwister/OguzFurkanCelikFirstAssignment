#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <vector>

// Indexed mesh for model rendering (position/normal/uv).
class DXModelMesh
{
public:
    struct Vertex
    {
        DirectX::XMFLOAT3 position; // Vertex position in object space.
        DirectX::XMFLOAT3 normal;   // Vertex normal in object space.
        DirectX::XMFLOAT2 uv;       // Texture coordinates.
    };

public:
    bool Initialize(
        ID3D12Device* device,
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices);

    void Destroy();

    void Bind(ID3D12GraphicsCommandList* cmdList) const;
    void Draw(ID3D12GraphicsCommandList* cmdList) const;

    UINT GetIndexCount() const { return m_indexCount; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer{};
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer{};

    D3D12_VERTEX_BUFFER_VIEW m_vbView{};
    D3D12_INDEX_BUFFER_VIEW  m_ibView{};

    UINT m_vertexCount = 0;
    UINT m_indexCount = 0;
};

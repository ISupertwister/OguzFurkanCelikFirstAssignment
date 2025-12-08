#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <cstdint>

class DXDevice; // forward

class DXMesh
{
public:
    DXMesh() noexcept = default;
    ~DXMesh() = default;

    //Initialize a basic triangle mesh in an upload heap.
    bool InitializeTriangle(DXDevice* device) noexcept;

    //Bind VB and issue a draw call.
    void Draw(ID3D12GraphicsCommandList* cmdList) noexcept;

    UINT GetVertexCount() const noexcept { return m_vertexCount; }

private:
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 color;
        DirectX::XMFLOAT2 uv;
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW               m_vbView{};
    UINT                                   m_vertexCount{ 0 };
};

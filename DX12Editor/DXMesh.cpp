#include "DXMesh.h"
#include "DXDevice.h"
#include "d3dx12.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// Initialize a simple colored/UV triangle into an upload heap.
bool DXMesh::InitializeTriangle(DXDevice* device) noexcept
{
    if (!device) return false;
    ID3D12Device* dev = device->GetDevice();
    if (!dev) return false;

    const Vertex verts[3] =
    {
        { XMFLOAT3(0.0f,  0.5f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(0.5f, 0.0f) },
        { XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
    };

    m_vertexCount = 3;
    const UINT vbSize = sizeof(verts);

    // Upload heap for vertex buffer
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC buf{};
    buf.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buf.Alignment = 0;
    buf.Width = vbSize;
    buf.Height = 1;
    buf.DepthOrArraySize = 1;
    buf.MipLevels = 1;
    buf.Format = DXGI_FORMAT_UNKNOWN;
    buf.SampleDesc.Count = 1;
    buf.SampleDesc.Quality = 0;
    buf.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    buf.Flags = D3D12_RESOURCE_FLAG_NONE;

    if (FAILED(dev->CreateCommittedResource(
        &heap,
        D3D12_HEAP_FLAG_NONE,
        &buf,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer))))
    {
        return false;
    }

    // Upload vertex data
    void* mapped = nullptr;
    D3D12_RANGE noRead{ 0, 0 };
    m_vertexBuffer->Map(0, &noRead, &mapped);
    std::memcpy(mapped, verts, vbSize);
    m_vertexBuffer->Unmap(0, nullptr);

    m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vbView.SizeInBytes = vbSize;
    m_vbView.StrideInBytes = sizeof(Vertex);

    return true;
}

// Bind the mesh VB and draw it.
void DXMesh::Draw(ID3D12GraphicsCommandList* cmdList) noexcept
{
    if (!cmdList || !m_vertexBuffer) return;

    cmdList->IASetVertexBuffers(0, 1, &m_vbView);
    cmdList->DrawInstanced(m_vertexCount, 1, 0, 0);
}

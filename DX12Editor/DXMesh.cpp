#include "DXMesh.h"
#include "DXDevice.h"
#include "d3dx12.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// Comment in English: Initialize a simple colored/UV triangle in an upload heap.
bool DXMesh::InitializeTriangle(DXDevice* device) noexcept
{
    if (!device) return false;
    ID3D12Device* dev = device->GetDevice();
    if (!dev) return false;

    const Vertex verts[3] =
    {
        { XMFLOAT3(0.0f,  0.5f, 0.0f), XMFLOAT3(1, 0, 0), XMFLOAT2(0.5f, 0.0f) },
        { XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT3(0, 1, 0), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT3(0, 0, 1), XMFLOAT2(0.0f, 1.0f) },
    };

    m_vertexCount = static_cast<UINT>(_countof(verts));
    const UINT vbSize = sizeof(verts);

    // Upload heap for vertex buffer
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);

    if (FAILED(dev->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer))))
    {
        return false;
    }

    // Upload vertex data
    void* mapped = nullptr;
    D3D12_RANGE noRead{ 0, 0 };
    if (FAILED(m_vertexBuffer->Map(0, &noRead, &mapped)))
        return false;

    std::memcpy(mapped, verts, vbSize);
    m_vertexBuffer->Unmap(0, nullptr);

    // Fill VB view
    m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vbView.SizeInBytes = vbSize;
    m_vbView.StrideInBytes = sizeof(Vertex);

    return true;
}

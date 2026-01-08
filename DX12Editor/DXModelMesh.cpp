#include "DXModelMesh.h"
#include "d3dx12.h"
#include <cassert>
#include <cstring>

using Microsoft::WRL::ComPtr;

bool DXModelMesh::Initialize(
    ID3D12Device* device,
    const std::vector<Vertex>& vertices,
    const std::vector<uint32_t>& indices)
{
    assert(device);
    if (vertices.empty() || indices.empty()) return false;

    m_vertexCount = (UINT)vertices.size();
    m_indexCount = (UINT)indices.size();

    const UINT vbSize = (UINT)(sizeof(Vertex) * vertices.size());
    const UINT ibSize = (UINT)(sizeof(uint32_t) * indices.size());

    // Upload heap (fastest to implement; acceptable for assignment).
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

    // Vertex buffer
    CD3DX12_RESOURCE_DESC vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);
    if (FAILED(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &vbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer))))
        return false;

    void* vbMapped = nullptr;
    CD3DX12_RANGE readRange(0, 0);
    if (FAILED(m_vertexBuffer->Map(0, &readRange, &vbMapped)))
        return false;
    std::memcpy(vbMapped, vertices.data(), vbSize);
    m_vertexBuffer->Unmap(0, nullptr);

    m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vbView.SizeInBytes = vbSize;
    m_vbView.StrideInBytes = sizeof(Vertex);

    // Index buffer
    CD3DX12_RESOURCE_DESC ibDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);
    if (FAILED(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &ibDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_indexBuffer))))
        return false;

    void* ibMapped = nullptr;
    if (FAILED(m_indexBuffer->Map(0, &readRange, &ibMapped)))
        return false;
    std::memcpy(ibMapped, indices.data(), ibSize);
    m_indexBuffer->Unmap(0, nullptr);

    m_ibView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_ibView.SizeInBytes = ibSize;
    m_ibView.Format = DXGI_FORMAT_R32_UINT;

    return true;
}

void DXModelMesh::Destroy()
{
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
    m_vertexCount = 0;
    m_indexCount = 0;
    m_vbView = {};
    m_ibView = {};
}

void DXModelMesh::Bind(ID3D12GraphicsCommandList* cmdList) const
{
    cmdList->IASetVertexBuffers(0, 1, &m_vbView);
    cmdList->IASetIndexBuffer(&m_ibView);
}

void DXModelMesh::Draw(ID3D12GraphicsCommandList* cmdList) const
{
    Bind(cmdList);
    cmdList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
}

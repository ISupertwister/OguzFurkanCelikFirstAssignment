#include "DXMesh.h"
#include "d3dx12.h"
#include <cstring> // for std::memcpy

using namespace DirectX;
using Microsoft::WRL::ComPtr;

bool DXMesh::InitializeQuad(ID3D12Device* device)
{
    Destroy();

    if (!device)
        return false;

    // Comment in English: 2x2 quad centered at origin, in the XY plane.
    // Z = 0; we will place/rotate it with a world matrix in the renderer.
    Vertex vertices[] =
    {
        //  position               color                uv
        { XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }, // top-left
        { XMFLOAT3(1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }, // top-right
        { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) }, // bottom-left

        { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) }, // bottom-left
        { XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) }, // bottom-right
        { XMFLOAT3(1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }, // top-right
    };

    const UINT vbSize = static_cast<UINT>(sizeof(vertices));
    m_vertexCount = 6;

    // Comment in English: Create an upload-heap vertex buffer.
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC   bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);

    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer)
    );

    if (FAILED(hr))
    {
        Destroy();
        return false;
    }

    // Comment in English: Copy CPU vertex data into the upload buffer.
    UINT8* mappedData = nullptr;
    CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on CPU.

    hr = m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedData));
    if (FAILED(hr))
    {
        Destroy();
        return false;
    }

    std::memcpy(mappedData, vertices, vbSize);
    m_vertexBuffer->Unmap(0, nullptr);

    // Comment in English: Fill vertex buffer view.
    m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vbView.StrideInBytes = sizeof(Vertex);
    m_vbView.SizeInBytes = vbSize;

    return true;
}

void DXMesh::Destroy()
{
    m_vertexBuffer.Reset();
    m_vbView = {};
    m_vertexCount = 0;
}

void DXMesh::Draw(ID3D12GraphicsCommandList* cmdList) const
{
    if (!cmdList || !m_vertexBuffer)
        return;

    cmdList->IASetVertexBuffers(0, 1, &m_vbView);
    cmdList->DrawInstanced(m_vertexCount, 1, 0, 0);
}

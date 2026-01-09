#include "RenderTarget.h"
#include "DXDevice.h"
#include "d3dx12.h"
#include <cassert>

using Microsoft::WRL::ComPtr;

bool RenderTarget::Initialize(
    DXDevice* device,
    UINT width,
    UINT height,
    DXGI_FORMAT colorFormat,
    DXGI_FORMAT depthFormat,
    ID3D12DescriptorHeap* imguiSrvHeap,
    UINT imguiSrvIndex)
{
    assert(device);
    m_device = device;
    m_width = width;
    m_height = height;
    m_colorFormat = colorFormat;
    m_depthFormat = depthFormat;
    m_imguiSrvHeap = imguiSrvHeap;
    m_imguiSrvIndex = imguiSrvIndex;

    return CreateResources();
}

void RenderTarget::Release()
{
    m_color.Reset();
    m_depth.Reset();
    m_rtvHeap.Reset();
    m_dsvHeap.Reset();
    m_device = nullptr;
    m_imguiSrvHeap = nullptr;
    m_width = m_height = 0;
}

void RenderTarget::Resize(UINT width, UINT height)
{
    if (width == 0 || height == 0) return;
    if (width == m_width && height == m_height) return;

    m_width = width;
    m_height = height;

    m_color.Reset();
    m_depth.Reset();
    CreateResources();
}

bool RenderTarget::CreateResources()
{
    auto* dev = m_device->GetDevice();
    if (!dev) return false;

    // ---------- RTV heap ----------
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
    rtvDesc.NumDescriptors = 1;
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    if (FAILED(dev->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&m_rtvHeap))))
        return false;

    m_rtvCpu = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

    // ---------- DSV heap ----------
    D3D12_DESCRIPTOR_HEAP_DESC dsvDesc{};
    dsvDesc.NumDescriptors = 1;
    dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    if (FAILED(dev->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&m_dsvHeap))))
        return false;

    m_dsvCpu = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

    // ---------- Color texture resource ----------
    D3D12_RESOURCE_DESC colorTex{};
    colorTex.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    colorTex.Alignment = 0;
    colorTex.Width = m_width;
    colorTex.Height = m_height;
    colorTex.DepthOrArraySize = 1;
    colorTex.MipLevels = 1;
    colorTex.Format = m_colorFormat;
    colorTex.SampleDesc.Count = 1;
    colorTex.SampleDesc.Quality = 0;
    colorTex.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    colorTex.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE clearColor{};
    clearColor.Format = m_colorFormat;
    clearColor.Color[0] = 0.05f;
    clearColor.Color[1] = 0.05f;
    clearColor.Color[2] = 0.10f;
    clearColor.Color[3] = 1.0f;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    if (FAILED(dev->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &colorTex,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, // start as SRV-safe
        &clearColor,
        IID_PPV_ARGS(&m_color))))
        return false;

    dev->CreateRenderTargetView(m_color.Get(), nullptr, m_rtvCpu);

    // ---------- Depth texture ----------
    D3D12_RESOURCE_DESC depthTex = colorTex;
    depthTex.Format = m_depthFormat;
    depthTex.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearDepth{};
    clearDepth.Format = m_depthFormat;
    clearDepth.DepthStencil.Depth = 1.0f;
    clearDepth.DepthStencil.Stencil = 0;

    if (FAILED(dev->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthTex,
        D3D12_RESOURCE_STATE_COMMON,
        &clearDepth,
        IID_PPV_ARGS(&m_depth))))
        return false;

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvView{};
    dsvView.Format = m_depthFormat;
    dsvView.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvView.Flags = D3D12_DSV_FLAG_NONE;
    dev->CreateDepthStencilView(m_depth.Get(), &dsvView, m_dsvCpu);

    // ---------- SRV in ImGui heap ----------
    assert(m_imguiSrvHeap);

    UINT inc = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpu = m_imguiSrvHeap->GetCPUDescriptorHandleForHeapStart();
    srvCpu.ptr += SIZE_T(m_imguiSrvIndex) * SIZE_T(inc);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = m_colorFormat;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    dev->CreateShaderResourceView(m_color.Get(), &srvDesc, srvCpu);

    // GPU handle
    m_srvGpu = m_imguiSrvHeap->GetGPUDescriptorHandleForHeapStart();
    m_srvGpu.ptr += UINT64(m_imguiSrvIndex) * UINT64(inc);

    return true;
}

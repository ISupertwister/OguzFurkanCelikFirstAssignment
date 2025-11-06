#include <filesystem>
#include "DXRenderer.h"
#include "DXDevice.h"
#include <cassert>
#include <cstdio>   // file load
#include <cstring>  // memcpy

using Microsoft::WRL::ComPtr;
using namespace DirectX;

bool DXRenderer::Initialize(HWND hwnd, DXDevice* device, UINT width, UINT height) noexcept {
    assert(hwnd && device);

    m_device = device;
    m_width = width;
    m_height = height;

    if (!CreateCommandQueue())            return false;
    if (!CreateSwapChain(hwnd, width, height)) return false;
    if (!CreateRTVDescriptorHeap())       return false;
    if (!CreateRenderTargets())           return false;

    if (FAILED(m_device->GetDevice()->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmdAlloc))))
        return false;

    if (FAILED(m_device->GetDevice()->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&m_cmdList))))
        return false;

    m_cmdList->Close(); // start closed

    if (FAILED(m_device->GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence))))
        return false;
    m_fenceValue = 1;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) return false;

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Viewport / scissor
    m_viewport = { 0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f };
    m_scissor = { 0, 0, int(width), int(height) };

    // Triangle pipeline
    if (!CreateRootSignature())   return false;
    if (!CreatePipelineState())   return false;
    if (!CreateConstantBuffer())  return false;
    if (!CreateTriangleVB())      return false;

    return true;
}

void DXRenderer::Render() noexcept {
    // Reset allocator & cmd list
    m_cmdAlloc->Reset();
    m_cmdList->Reset(m_cmdAlloc.Get(), nullptr);

    const UINT bb = m_swapChain->GetCurrentBackBufferIndex();
    ID3D12Resource* backBuffer = m_renderTargets[bb].Get();

    // Transition PRESENT/COMMON -> RENDER_TARGET
    D3D12_RESOURCE_BARRIER toRT{};
    toRT.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toRT.Transition.pResource = backBuffer;
    toRT.Transition.StateBefore = m_firstFrame ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_PRESENT;
    toRT.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toRT.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_cmdList->ResourceBarrier(1, &toRT);

    // RTV handle
    D3D12_CPU_DESCRIPTOR_HANDLE rtvStart = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = { rtvStart.ptr + SIZE_T(bb) * SIZE_T(m_rtvDescriptorSize) };

    // Clear
    m_cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    const float clearColor[4] = { 0.08f, 0.10f, 0.20f, 1.0f };
    m_cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

    // Draw triangle
    m_cmdList->RSSetViewports(1, &m_viewport);
    m_cmdList->RSSetScissorRects(1, &m_scissor);
    m_cmdList->SetGraphicsRootSignature(m_rootSig.Get());
    m_cmdList->SetPipelineState(m_pso.Get());
    m_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_cmdList->IASetVertexBuffers(0, 1, &m_vbView);
    m_cmdList->DrawInstanced(3, 1, 0, 0);

    // Back to PRESENT
    D3D12_RESOURCE_BARRIER toPresent = toRT;
    std::swap(toPresent.Transition.StateBefore, toPresent.Transition.StateAfter);
    m_cmdList->ResourceBarrier(1, &toPresent);

    // Submit
    m_cmdList->Close();
    ID3D12CommandList* lists[] = { m_cmdList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);
    m_swapChain->Present(1, 0);

    m_firstFrame = false;

    // Simple sync
    const UINT64 fenceToWait = m_fenceValue++;
    m_commandQueue->Signal(m_fence.Get(), fenceToWait);
    if (m_fence->GetCompletedValue() < fenceToWait) {
        m_fence->SetEventOnCompletion(fenceToWait, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void DXRenderer::Resize(UINT width, UINT height) noexcept {
    if (!m_swapChain) return;
    if (width == 0 || height == 0) return; // minimized

    WaitForGpu();

    for (auto& rt : m_renderTargets) rt.Reset();

    m_width = width;
    m_height = height;

    m_swapChain->ResizeBuffers(kBufferCount, width, height, m_backbufferFormat, 0);
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    CreateRenderTargets();

    m_viewport = { 0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f };
    m_scissor = { 0, 0, int(width), int(height) };

    m_firstFrame = true;
}

bool DXRenderer::CreateCommandQueue() noexcept {
    D3D12_COMMAND_QUEUE_DESC desc{};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    return SUCCEEDED(m_device->GetDevice()->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_commandQueue)));
}

bool DXRenderer::CreateSwapChain(HWND hwnd, UINT width, UINT height) noexcept {
    DXGI_SWAP_CHAIN_DESC1 sc{};
    sc.Width = width;
    sc.Height = height;
    sc.Format = m_backbufferFormat;
    sc.SampleDesc = { 1, 0 };
    sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc.BufferCount = kBufferCount;
    sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sc.Scaling = DXGI_SCALING_STRETCH;
    sc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    ComPtr<IDXGISwapChain1> sc1;
    if (FAILED(m_device->GetFactory()->CreateSwapChainForHwnd(
        m_commandQueue.Get(), hwnd, &sc, nullptr, nullptr, &sc1)))
        return false;

    m_device->GetFactory()->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    sc1.As(&m_swapChain);

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    return true;
}

bool DXRenderer::CreateRTVDescriptorHeap() noexcept {
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    desc.NumDescriptors = kBufferCount;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    if (FAILED(m_device->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtvHeap))))
        return false;

    m_rtvDescriptorSize =
        m_device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    return true;
}

bool DXRenderer::CreateRenderTargets() noexcept {
    m_renderTargets.resize(kBufferCount);

    D3D12_CPU_DESCRIPTOR_HANDLE start = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < kBufferCount; ++i) {
        if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]))))
            return false;

        D3D12_CPU_DESCRIPTOR_HANDLE handle{ start.ptr + SIZE_T(i) * SIZE_T(m_rtvDescriptorSize) };
        m_device->GetDevice()->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, handle);
    }
    return true;
}

bool DXRenderer::CreateRootSignature() noexcept {
    // 1 x CBV range (b0..b0)
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0; // b0
    range.RegisterSpace = 0;
    range.OffsetInDescriptorsFromTableStart = 0;

    // Root parameter: a single descriptor table that holds our CBV
    D3D12_ROOT_DESCRIPTOR_TABLE table{};
    table.NumDescriptorRanges = 1;
    table.pDescriptorRanges = &range;

    D3D12_ROOT_PARAMETER param{};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param.DescriptorTable = table;
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // used in VS

    D3D12_ROOT_SIGNATURE_DESC rs{};
    rs.NumParameters = 1;
    rs.pParameters = &param;
    rs.NumStaticSamplers = 0;
    rs.pStaticSamplers = nullptr;
    rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> blob, err;
    if (FAILED(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err)))
        return false;

    return SUCCEEDED(m_device->GetDevice()->CreateRootSignature(
        0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_rootSig)));
}

bool DXRenderer::CreatePipelineState() noexcept {
    auto shaderPath = [](const wchar_t* file) -> std::wstring {
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::filesystem::path p(exe);
        p = p.parent_path() / L"Shaders" / file;  // <exe>\Shaders\file
        return p.wstring();
        };
    // Load compiled shaders from ./Shaders
    std::vector<uint8_t> vs, ps;
    if (!LoadFileBinary(shaderPath(L"ColorVS.cso").c_str(), vs)) return false;
    if (!LoadFileBinary(shaderPath(L"ColorPS.cso").c_str(), ps)) return false;

    D3D12_SHADER_BYTECODE VS{ vs.data(), (UINT)vs.size() };
    D3D12_SHADER_BYTECODE PS{ ps.data(), (UINT)ps.size() };

    // Input layout
    D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,                           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)sizeof(XMFLOAT3),      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // Defaults without d3dx12 helpers
    D3D12_RASTERIZER_DESC rast{};
    rast.FillMode = D3D12_FILL_MODE_SOLID;
    rast.CullMode = D3D12_CULL_MODE_BACK;
    rast.FrontCounterClockwise = FALSE;
    rast.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rast.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rast.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rast.DepthClipEnable = TRUE;
    rast.MultisampleEnable = FALSE;
    rast.AntialiasedLineEnable = FALSE;
    rast.ForcedSampleCount = 0;
    rast.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    D3D12_BLEND_DESC blend{};
    blend.AlphaToCoverageEnable = FALSE;
    blend.IndependentBlendEnable = FALSE;
    auto& rt0 = blend.RenderTarget[0];
    rt0.BlendEnable = FALSE;
    rt0.LogicOpEnable = FALSE;
    rt0.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = m_rootSig.Get();
    pso.VS = VS;
    pso.PS = PS;
    pso.InputLayout = { layout, _countof(layout) };
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.RasterizerState = rast;
    pso.BlendState = blend;
    pso.DepthStencilState.DepthEnable = FALSE;
    pso.DepthStencilState.StencilEnable = FALSE;
    pso.SampleMask = UINT_MAX;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = m_backbufferFormat;
    pso.SampleDesc = { 1, 0 };

    if (FAILED(m_device->GetDevice()->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_pso))))
        return false;

    return true;
}

bool DXRenderer::CreateTriangleVB() noexcept {
    const Vertex verts[3] = {
        { XMFLOAT3(0.0f,  0.5f, 0.0f),  XMFLOAT3(1,0,0) },
        { XMFLOAT3(0.5f, -0.5f, 0.0f),  XMFLOAT3(0,1,0) },
        { XMFLOAT3(-0.5f, -0.5f, 0.0f),  XMFLOAT3(0,0,1) },
    };
    const UINT vbSize = sizeof(verts);

    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC buf{};
    buf.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buf.Width = vbSize;
    buf.Height = 1;
    buf.DepthOrArraySize = 1;
    buf.MipLevels = 1;
    buf.SampleDesc = { 1,0 };
    buf.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(m_device->GetDevice()->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &buf,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_vertexBuffer))))
        return false;

    void* mapped = nullptr;
    D3D12_RANGE noRead{ 0,0 };
    m_vertexBuffer->Map(0, &noRead, &mapped);
    memcpy(mapped, verts, vbSize);
    m_vertexBuffer->Unmap(0, nullptr);

    m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vbView.SizeInBytes = vbSize;
    m_vbView.StrideInBytes = sizeof(Vertex);
    return true;
}

bool DXRenderer::LoadFileBinary(const wchar_t* path, std::vector<uint8_t>& data) noexcept {
    FILE* f = nullptr;
    _wfopen_s(&f, path, L"rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    data.resize(size_t(sz));
    if (sz > 0) fread(data.data(), 1, size_t(sz), f);
    fclose(f);
    return true;
}

void DXRenderer::WaitForGpu() noexcept {
    if (!m_commandQueue || !m_fence) return;
    const UINT64 fenceToWait = ++m_fenceValue;
    m_commandQueue->Signal(m_fence.Get(), fenceToWait);
    if (m_fence->GetCompletedValue() < fenceToWait) {
        m_fence->SetEventOnCompletion(fenceToWait, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}
bool DXRenderer::CreateConstantBuffer() noexcept
{
    // 256-byte aligned size
    m_cbSize = (sizeof(CbMvp) + 255) & ~255u;

    // Upload heap buffer
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC   buf{};  buf.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buf.Width = m_cbSize; buf.Height = 1; buf.DepthOrArraySize = 1; buf.MipLevels = 1;
    buf.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; buf.SampleDesc = { 1,0 };

    if (FAILED(m_device->GetDevice()->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &buf,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_cbUpload))))
        return false;

    // Persistent map (write every frame)
    D3D12_RANGE noRead{ 0,0 };
    if (FAILED(m_cbUpload->Map(0, &noRead, reinterpret_cast<void**>(&m_cbMapped))))
        return false;

    // Shader-visible CBV heap (1 descriptor)
    D3D12_DESCRIPTOR_HEAP_DESC h{};
    h.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    h.NumDescriptors = 1;
    h.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(m_device->GetDevice()->CreateDescriptorHeap(&h, IID_PPV_ARGS(&m_cbvHeap))))
        return false;

    // Create the CBV (points to upload buffer)
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv{};
    cbv.BufferLocation = m_cbUpload->GetGPUVirtualAddress();
    cbv.SizeInBytes = m_cbSize;
    m_device->GetDevice()->CreateConstantBufferView(
        &cbv, m_cbvHeap->GetCPUDescriptorHandleForHeapStart());

    return true;
}




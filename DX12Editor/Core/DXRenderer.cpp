#include <windows.h>
#include <filesystem>
#include <cassert>
#include <cstdio>
#include <cstring>

#include "DXRenderer.h"
#include "DXDevice.h"
#include <d3dx12.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// --------------------------------------------------------
// Initialization
// --------------------------------------------------------
bool DXRenderer::Initialize(HWND hwnd, DXDevice* device, UINT width, UINT height) noexcept {
    assert(hwnd && device);

    m_hwnd = hwnd;
    m_device = device;
    m_width = width;
    m_height = height;

    // Core GPU objects
    if (!CreateCommandQueue())                 return false;
    if (!CreateSwapChain(hwnd, width, height)) return false;
    if (!CreateRTVDescriptorHeap())            return false;
    if (!CreateRenderTargets())                return false;
    if (!CreateDepthResources())               return false;

    // Command allocator + list
    if (FAILED(m_device->GetDevice()->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmdAlloc))))
        return false;

    if (FAILED(m_device->GetDevice()->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_cmdAlloc.Get(),
        nullptr,
        IID_PPV_ARGS(&m_cmdList))))
        return false;

    // Start closed, we will Reset() before recording commands
    m_cmdList->Close();

    // Fence / sync
    if (FAILED(m_device->GetDevice()->CreateFence(
        0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence))))
        return false;

    m_fenceValue = 1;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent)
        return false;

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Viewport / scissor
    m_viewport = { 0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f };
    m_scissor = { 0, 0, int(width), int(height) };

    // Graphics pipeline and resources
    if (!CreateRootSignature())      return false;
    if (!CreatePipelineState())      return false;
    if (!CreateConstantBuffer())     return false;
    if (!CreateTriangleVB())         return false;
    if (!CreateCheckerTextureSRV())  return false;

    return true;
}

// --------------------------------------------------------
// Frame rendering
// --------------------------------------------------------
void DXRenderer::Render() noexcept {
    // Time update
    m_timer.Tick();
    double dt = m_timer.Delta();
    if (dt > 0.1) dt = 0.1; // clamp spikes a bit
    const float angularSpeed = 1.2f;
    m_time += float(dt) * angularSpeed;

    // Reset allocator & command list
    m_cmdAlloc->Reset();
    m_cmdList->Reset(m_cmdAlloc.Get(), nullptr);

    const UINT bb = m_swapChain->GetCurrentBackBufferIndex();
    ID3D12Resource* backBuffer = m_renderTargets[bb].Get();

    // Transition from COMMON/PRESENT -> RENDER_TARGET
    D3D12_RESOURCE_STATES beforeState =
        m_firstFrame ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_PRESENT;

    auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffer, beforeState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_cmdList->ResourceBarrier(1, &toRT);

    // RTV / DSV handles
    D3D12_CPU_DESCRIPTOR_HANDLE rtvStart =
        m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{
        rtvStart.ptr + SIZE_T(bb) * SIZE_T(m_rtvDescriptorSize)
    };
    D3D12_CPU_DESCRIPTOR_HANDLE dsv =
        m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

    // Clear
    const float clearColor[4] = { 0.08f, 0.10f, 0.20f, 1.0f };
    m_cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    m_cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    m_cmdList->ClearDepthStencilView(
        dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Bind descriptor heap (CBV + SRV in a single heap)
    ID3D12DescriptorHeap* heaps[] = { m_cbvHeap.Get() };
    m_cmdList->SetDescriptorHeaps(1, heaps);

    // Camera / matrices
    float aspect =
        (m_height == 0) ? 1.0f : float(m_width) / float(m_height);

    XMMATRIX V = XMMatrixLookAtLH(
        XMVectorSet(0, 0, -3, 0),
        XMVectorZero(),
        XMVectorSet(0, 1, 0, 0));

    XMMATRIX P = XMMatrixPerspectiveFovLH(
        XM_PIDIV4, aspect, 0.1f, 100.0f);

    XMMATRIX M = XMMatrixRotationZ(m_time);

    XMMATRIX MVP = M * V * P;
    XMMATRIX MVPt = XMMatrixTranspose(MVP); // column-major expectation

    CbMvp cb{};
    XMStoreFloat4x4(&cb.mvp, MVPt);
    std::memcpy(m_cbMapped, &cb, sizeof(CbMvp));

    // Set fixed pipeline state
    m_cmdList->RSSetViewports(1, &m_viewport);
    m_cmdList->RSSetScissorRects(1, &m_scissor);
    m_cmdList->SetGraphicsRootSignature(m_rootSig.Get());
    m_cmdList->SetPipelineState(m_pso.Get());
    m_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_cmdList->IASetVertexBuffers(0, 1, &m_vbView);

    // Root parameters: 0 = CBV table, 1 = SRV table
    auto gpuStart = m_cbvHeap->GetGPUDescriptorHandleForHeapStart();
    m_cmdList->SetGraphicsRootDescriptorTable(0, gpuStart); // CBV @ index 0

    UINT inc = m_device->GetDevice()->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_GPU_DESCRIPTOR_HANDLE gpuSrv{
        gpuStart.ptr + SIZE_T(inc) // index 1
    };
    m_cmdList->SetGraphicsRootDescriptorTable(1, gpuSrv); // SRV @ index 1

    // Draw (6 vertices = 2 triangles = textured quad)
    m_cmdList->DrawInstanced(6, 1, 0, 0);

    // Back to PRESENT
    auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffer,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    m_cmdList->ResourceBarrier(1, &toPresent);

    // Execute
    m_cmdList->Close();
    ID3D12CommandList* lists[] = { m_cmdList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);
    m_swapChain->Present(1, 0);

    m_firstFrame = false;

    // Simple GPU sync
    const UINT64 fenceToWait = m_fenceValue++;
    m_commandQueue->Signal(m_fence.Get(), fenceToWait);
    if (m_fence->GetCompletedValue() < fenceToWait) {
        m_fence->SetEventOnCompletion(fenceToWait, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Optional window title update
    if (m_hwnd) {
        double fps;
        if (m_timer.SampleFps(0.5, fps)) {
            wchar_t title[256];
            swprintf_s(
                title,
                L"DX12 Editor  |  FPS: %.0f (%.2f ms)",
                fps,
                fps > 0.0 ? 1000.0 / fps : 0.0);
            SetWindowTextW(m_hwnd, title);
        }
    }
}

// --------------------------------------------------------
// Resize
// --------------------------------------------------------
void DXRenderer::Resize(UINT width, UINT height) noexcept {
    if (!m_swapChain) return;
    if (width == 0 || height == 0) return; // minimized

    WaitForGpu();

    for (auto& rt : m_renderTargets)
        rt.Reset();
    m_depth.Reset();

    m_width = width;
    m_height = height;

    m_swapChain->ResizeBuffers(
        kBufferCount, width, height, m_backbufferFormat, 0);
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    CreateRenderTargets();
    CreateDepthResources();

    m_viewport = { 0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f };
    m_scissor = { 0, 0, int(width), int(height) };

    m_firstFrame = true;
}

// --------------------------------------------------------
// Helpers (queue / swapchain / RTVs)
// --------------------------------------------------------
bool DXRenderer::CreateCommandQueue() noexcept {
    D3D12_COMMAND_QUEUE_DESC desc{};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    return SUCCEEDED(m_device->GetDevice()->CreateCommandQueue(
        &desc, IID_PPV_ARGS(&m_commandQueue)));
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

    m_device->GetFactory()->MakeWindowAssociation(
        hwnd, DXGI_MWA_NO_ALT_ENTER);

    sc1.As(&m_swapChain);
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    return true;
}

bool DXRenderer::CreateRTVDescriptorHeap() noexcept {
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    desc.NumDescriptors = kBufferCount;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    if (FAILED(m_device->GetDevice()->CreateDescriptorHeap(
        &desc, IID_PPV_ARGS(&m_rtvHeap))))
        return false;

    m_rtvDescriptorSize =
        m_device->GetDevice()->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    return true;
}

bool DXRenderer::CreateRenderTargets() noexcept {
    m_renderTargets.resize(kBufferCount);

    D3D12_CPU_DESCRIPTOR_HANDLE start =
        m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < kBufferCount; ++i) {
        if (FAILED(m_swapChain->GetBuffer(
            i, IID_PPV_ARGS(&m_renderTargets[i]))))
            return false;

        D3D12_CPU_DESCRIPTOR_HANDLE handle{
            start.ptr + SIZE_T(i) * SIZE_T(m_rtvDescriptorSize)
        };

        m_device->GetDevice()->CreateRenderTargetView(
            m_renderTargets[i].Get(), nullptr, handle);
    }
    return true;
}

// --------------------------------------------------------
// Depth buffer (DSV)
// --------------------------------------------------------
bool DXRenderer::CreateDepthResources() noexcept {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = m_width;
    desc.Height = m_height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = m_depthFormat; // DXGI_FORMAT_D32_FLOAT
    desc.SampleDesc = { 1, 0 };
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clear{};
    clear.Format = m_depthFormat;
    clear.DepthStencil.Depth = 1.0f;
    clear.DepthStencil.Stencil = 0;

    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;

    if (FAILED(m_device->GetDevice()->CreateCommittedResource(
        &heap,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clear,
        IID_PPV_ARGS(&m_depth))))
        return false;

    D3D12_DESCRIPTOR_HEAP_DESC dh{};
    dh.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dh.NumDescriptors = 1;
    dh.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    if (FAILED(m_device->GetDevice()->CreateDescriptorHeap(
        &dh, IID_PPV_ARGS(&m_dsvHeap))))
        return false;

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
    dsv.Format = m_depthFormat;
    dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv.Flags = D3D12_DSV_FLAG_NONE;

    m_device->GetDevice()->CreateDepthStencilView(
        m_depth.Get(),
        &dsv,
        m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

    return true;
}

// --------------------------------------------------------
// Root signature & PSO
// --------------------------------------------------------
bool DXRenderer::CreateRootSignature() noexcept {
    // 1) CBV table (b0)
    D3D12_DESCRIPTOR_RANGE rngCBV{};
    rngCBV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    rngCBV.NumDescriptors = 1;
    rngCBV.BaseShaderRegister = 0; // b0
    rngCBV.RegisterSpace = 0;
    rngCBV.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_DESCRIPTOR_TABLE tblCBV{};
    tblCBV.NumDescriptorRanges = 1;
    tblCBV.pDescriptorRanges = &rngCBV;

    D3D12_ROOT_PARAMETER paramCBV{};
    paramCBV.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    paramCBV.DescriptorTable = tblCBV;
    paramCBV.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // 2) SRV table (t0)
    D3D12_DESCRIPTOR_RANGE rngSRV{};
    rngSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    rngSRV.NumDescriptors = 1;
    rngSRV.BaseShaderRegister = 0; // t0
    rngSRV.RegisterSpace = 0;
    rngSRV.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_DESCRIPTOR_TABLE tblSRV{};
    tblSRV.NumDescriptorRanges = 1;
    tblSRV.pDescriptorRanges = &rngSRV;

    D3D12_ROOT_PARAMETER paramSRV{};
    paramSRV.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    paramSRV.DescriptorTable = tblSRV;
    paramSRV.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_PARAMETER paramsRS[2] = { paramCBV, paramSRV };

    // Static sampler s0
    D3D12_STATIC_SAMPLER_DESC samp{};
    samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samp.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    samp.MaxLOD = D3D12_FLOAT32_MAX;
    samp.ShaderRegister = 0; // s0
    samp.RegisterSpace = 0;
    samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rs{};
    rs.NumParameters = 2;
    rs.pParameters = paramsRS;
    rs.NumStaticSamplers = 1;
    rs.pStaticSamplers = &samp;
    rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> blob, err;
    if (FAILED(D3D12SerializeRootSignature(
        &rs,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &blob,
        &err)))
        return false;

    if (FAILED(m_device->GetDevice()->CreateRootSignature(
        0,
        blob->GetBufferPointer(),
        blob->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSig))))
        return false;

    return true;
}

bool DXRenderer::CreatePipelineState() noexcept {
    auto shaderPath = [](const wchar_t* file) -> std::wstring {
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::filesystem::path p(exe);
        p = p.parent_path() / L"Shaders" / file;
        return p.wstring();
        };

    std::vector<uint8_t> vs, ps;
    if (!LoadFileBinary(shaderPath(L"ColorVS.cso").c_str(), vs)) return false;
    if (!LoadFileBinary(shaderPath(L"ColorPS.cso").c_str(), ps)) return false;

    D3D12_SHADER_BYTECODE VS{ vs.data(), (UINT)vs.size() };
    D3D12_SHADER_BYTECODE PS{ ps.data(), (UINT)ps.size() };

    D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
          (UINT)sizeof(XMFLOAT3),
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0,
          (UINT)(sizeof(XMFLOAT3) * 2),
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

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

    D3D12_DEPTH_STENCIL_DESC dsd{};
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    dsd.StencilEnable = FALSE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = m_rootSig.Get();
    pso.VS = VS;
    pso.PS = PS;
    pso.InputLayout = { layout, _countof(layout) };
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.RasterizerState = rast;
    pso.BlendState = blend;
    pso.DepthStencilState = dsd;
    pso.SampleMask = UINT_MAX;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = m_backbufferFormat;
    pso.DSVFormat = m_depthFormat;
    pso.SampleDesc = { 1, 0 };

    if (FAILED(m_device->GetDevice()->CreateGraphicsPipelineState(
        &pso, IID_PPV_ARGS(&m_pso))))
        return false;

    return true;
}

// --------------------------------------------------------
// Geometry (textured quad as 2 triangles)
// --------------------------------------------------------
bool DXRenderer::CreateTriangleVB() noexcept {
    const Vertex verts[6] = {
        // 1st triangle
        { XMFLOAT3(-0.5f,  0.5f, 0.0f), XMFLOAT3(1,0,0), XMFLOAT2(0.0f, 0.0f) }, // top-left
        { XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT3(0,1,0), XMFLOAT2(1.0f, 1.0f) }, // bottom-right
        { XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT3(0,0,1), XMFLOAT2(0.0f, 1.0f) }, // bottom-left

        // 2nd triangle
        { XMFLOAT3(-0.5f,  0.5f, 0.0f), XMFLOAT3(1,0,0), XMFLOAT2(0.0f, 0.0f) }, // top-left
        { XMFLOAT3(0.5f,  0.5f, 0.0f), XMFLOAT3(0,1,1), XMFLOAT2(1.0f, 0.0f) }, // top-right
        { XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT3(0,1,0), XMFLOAT2(1.0f, 1.0f) }, // bottom-right
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
    buf.SampleDesc = { 1, 0 };
    buf.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    buf.Flags = D3D12_RESOURCE_FLAG_NONE;

    if (FAILED(m_device->GetDevice()->CreateCommittedResource(
        &heap,
        D3D12_HEAP_FLAG_NONE,
        &buf,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer))))
        return false;

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

// --------------------------------------------------------
// Constant buffer + descriptor heap (CBV + SRV)
// --------------------------------------------------------
bool DXRenderer::CreateConstantBuffer() noexcept {
    m_cbSize = (sizeof(CbMvp) + 255) & ~255u;

    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC buf{};
    buf.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buf.Width = m_cbSize;
    buf.Height = 1;
    buf.DepthOrArraySize = 1;
    buf.MipLevels = 1;
    buf.SampleDesc = { 1, 0 };
    buf.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    buf.Flags = D3D12_RESOURCE_FLAG_NONE;

    if (FAILED(m_device->GetDevice()->CreateCommittedResource(
        &heap,
        D3D12_HEAP_FLAG_NONE,
        &buf,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_cbUpload))))
        return false;

    D3D12_RANGE noRead{ 0, 0 };
    if (FAILED(m_cbUpload->Map(
        0,
        &noRead,
        reinterpret_cast<void**>(&m_cbMapped))))
        return false;

    // Descriptor heap: 2 slots => [0] CBV, [1] SRV
    D3D12_DESCRIPTOR_HEAP_DESC h{};
    h.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    h.NumDescriptors = 2;
    h.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    if (FAILED(m_device->GetDevice()->CreateDescriptorHeap(
        &h, IID_PPV_ARGS(&m_cbvHeap))))
        return false;

    // CBV at index 0
    D3D12_CPU_DESCRIPTOR_HANDLE cpuStart =
        m_cbvHeap->GetCPUDescriptorHandleForHeapStart();

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv{};
    cbv.BufferLocation = m_cbUpload->GetGPUVirtualAddress();
    cbv.SizeInBytes = m_cbSize;

    m_device->GetDevice()->CreateConstantBufferView(
        &cbv, cpuStart);

    return true;
}

// --------------------------------------------------------
// Procedural checker texture + SRV at heap[1]
// --------------------------------------------------------
bool DXRenderer::CreateCheckerTextureSRV() noexcept {
    const UINT W = 256;
    const UINT H = 256;

    std::vector<uint32_t> pixels(W * H);
    for (UINT y = 0; y < H; ++y) {
        for (UINT x = 0; x < W; ++x) {
            bool   c = ((x / 32) ^ (y / 32)) & 1;
            uint8_t v = c ? 220 : 40;
            uint8_t r = v;
            uint8_t g = v;
            uint8_t b = v;
            pixels[y * W + x] =
                (0xFFu << 24) |
                (uint32_t(b) << 16) |
                (uint32_t(g) << 8) |
                uint32_t(r);
        }
    }

    // Default heap texture (GPU)
    D3D12_RESOURCE_DESC tex{};
    tex.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    tex.Alignment = 0;
    tex.Width = W;
    tex.Height = H;
    tex.DepthOrArraySize = 1;
    tex.MipLevels = 1;
    tex.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex.SampleDesc = { 1, 0 };
    tex.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    tex.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defHeap{};
    defHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    if (FAILED(m_device->GetDevice()->CreateCommittedResource(
        &defHeap,
        D3D12_HEAP_FLAG_NONE,
        &tex,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_tex))))
        return false;

    // Upload heap
    const UINT64 uploadSize =
        GetRequiredIntermediateSize(m_tex.Get(), 0, 1);

    D3D12_HEAP_PROPERTIES upHeap{};
    upHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    auto upDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);

    ComPtr<ID3D12Resource> upload;
    if (FAILED(m_device->GetDevice()->CreateCommittedResource(
        &upHeap,
        D3D12_HEAP_FLAG_NONE,
        &upDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&upload))))
        return false;

    // Subresource data
    D3D12_SUBRESOURCE_DATA s{};
    s.pData = pixels.data();
    s.RowPitch = LONG(W * sizeof(uint32_t));
    s.SlicePitch = LONG(W * sizeof(uint32_t) * H);

    // Record copy + transition
    m_cmdAlloc->Reset();
    m_cmdList->Reset(m_cmdAlloc.Get(), nullptr);

    UpdateSubresources(
        m_cmdList.Get(),
        m_tex.Get(),
        upload.Get(),
        0, 0, 1, &s);

    auto toSRV = CD3DX12_RESOURCE_BARRIER::Transition(
        m_tex.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_cmdList->ResourceBarrier(1, &toSRV);

    m_cmdList->Close();
    ID3D12CommandList* lists[] = { m_cmdList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);

    // Wait until upload complete
    WaitForGpu();

    // SRV at descriptor index 1 in the same heap
    D3D12_CPU_DESCRIPTOR_HANDLE cpuStart =
        m_cbvHeap->GetCPUDescriptorHandleForHeapStart();
    UINT inc = m_device->GetDevice()->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE cpuSrv{
        cpuStart.ptr + SIZE_T(inc) // index 1
    };

    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;
    srv.Texture2D.MostDetailedMip = 0;
    srv.Texture2D.ResourceMinLODClamp = 0.0f;

    m_device->GetDevice()->CreateShaderResourceView(
        m_tex.Get(), &srv, cpuSrv);

    return true;
}

// --------------------------------------------------------
// File load helper
// --------------------------------------------------------
bool DXRenderer::LoadFileBinary(const wchar_t* path,
    std::vector<uint8_t>& data) noexcept {
    FILE* f = nullptr;
    _wfopen_s(&f, path, L"rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    data.resize(size_t(sz));
    if (sz > 0)
        fread(data.data(), 1, size_t(sz), f);

    fclose(f);
    return true;
}

// --------------------------------------------------------
// GPU sync helper
// --------------------------------------------------------
void DXRenderer::WaitForGpu() noexcept {
    if (!m_commandQueue || !m_fence) return;

    const UINT64 fenceToWait = ++m_fenceValue;
    m_commandQueue->Signal(m_fence.Get(), fenceToWait);

    if (m_fence->GetCompletedValue() < fenceToWait) {
        m_fence->SetEventOnCompletion(fenceToWait, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

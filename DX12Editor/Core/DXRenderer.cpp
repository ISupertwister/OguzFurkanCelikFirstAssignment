#include <windows.h>
#include <filesystem>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#include "DXRenderer.h"
#include "DXDevice.h"
#include <d3dx12.h> 

// ImGui Headers
#include "imgui/imgui.h" 
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// External declaration for ImGui's Win32 message handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ========================================================
// IMGUI WNDPROC HANDLER
// ========================================================
LRESULT DXRenderer::ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return ::ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
}
// ========================================================
// INPUT HANDLERS (called from Win32 / Window)
// ========================================================

void DXRenderer::OnMouseMove(float dx, float dy)
{
    // Accumulate mouse delta for this frame
    m_mouseDeltaX += dx;
    m_mouseDeltaY += dy;
}

void DXRenderer::OnMouseWheel(float wheelTicks)
{
    m_wheelTicks += wheelTicks;
}

void DXRenderer::OnRightMouseDown()
{
    m_isRightMouseDown = true;
}

void DXRenderer::OnRightMouseUp()
{
    m_isRightMouseDown = false;
}

void DXRenderer::OnLeftMouseDown()
{
    m_isLeftMouseDown = true;
}

void DXRenderer::OnLeftMouseUp()
{
    m_isLeftMouseDown = false;
}

void DXRenderer::OnKeyDown(UINT key)
{
    if (key == VK_MENU)  m_isAltDown = true;
    if (key == VK_SHIFT) m_isShiftDown = true;

    if (key == 'W') m_keyW = true;
    if (key == 'A') m_keyA = true;
    if (key == 'S') m_keyS = true;
    if (key == 'D') m_keyD = true;
    if (key == 'Q') m_keyQ = true;
    if (key == 'E') m_keyE = true;

    // Focus key (F): focus on origin for now
    if (key == 'F')
    {
        DirectX::XMFLOAT3 target = { 0.0f, 0.0f, 0.0f };
        float distance = 5.0f;
        m_camera.Focus(target, distance);
    }
}

void DXRenderer::OnKeyUp(UINT key)
{
    if (key == VK_MENU)  m_isAltDown = false;
    if (key == VK_SHIFT) m_isShiftDown = false;

    if (key == 'W') m_keyW = false;
    if (key == 'A') m_keyA = false;
    if (key == 'S') m_keyS = false;
    if (key == 'D') m_keyD = false;
    if (key == 'Q') m_keyQ = false;
    if (key == 'E') m_keyE = false;
}

// --------------------------------------------------------
// Destructor (Cleanup)
// --------------------------------------------------------
DXRenderer::~DXRenderer() noexcept {
    WaitForGpu();
    if (m_fenceEvent) CloseHandle(m_fenceEvent);

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

// --------------------------------------------------------
// Initialization
// --------------------------------------------------------
bool DXRenderer::Initialize(HWND hwnd, DXDevice* device, UINT width, UINT height) noexcept {
    assert(hwnd && device);

    m_hwnd = hwnd;
    m_device = device;
    m_width = width;
    m_height = height;

    // Kamera için başlangıç ayarı
    float aspect = (m_height == 0) ? 1.0f : float(m_width) / float(m_height);
    m_camera.SetProjection(XM_PIDIV4, aspect, 0.1f, 1000.0f);

    // Temel GPU nesneleri
    if (!CreateCommandQueue()) return false;
    if (!CreateSwapChain(hwnd, width, height)) return false;
    if (!CreateRTVDescriptorHeap()) return false;
    if (!CreateRenderTargets()) return false;
    if (!CreateDepthResources()) return false;

    // Komut listesi oluşturucusu
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

    m_cmdList->Close(); // Şimdilik kapatıyoruz

    // Senkronizasyon (Fence)
    if (FAILED(m_device->GetDevice()->CreateFence(
        0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence))))
        return false;

    m_fenceValue = 1;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) return false;

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Viewport / scissor
    m_viewport = { 0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f };
    m_scissor = { 0, 0, int(width), int(height) };

    // Pipeline ve kaynaklar
    if (!CreateRootSignature()) return false;
    if (!CreatePipelineState()) return false;
    if (!CreateConstantBuffer()) return false;
    if (!CreateTriangleVB()) return false;
    if (!CreateCheckerTextureSRV()) return false;

    // ====================================================
    // IMGUI ENTEGRASYONU VE FONT YÜKLEME
    // ====================================================
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 1;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    if (FAILED(m_device->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_imguiSrvHeap))))
        return false;

    // 1) ImGui context + style
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    // 2) Platform backend (Win32)
    ImGui_ImplWin32_Init(m_hwnd);

    // 3) Renderer backend (DX12)
    //  - Burada KESİNLİKLE valid command queue, format, vs. veriyoruz.
    //  - Font atlas'ı kendi içinde halledecek.
    ImGui_ImplDX12_Init(
        m_device->GetDevice(),
        kBufferCount,
        m_backbufferFormat,
        m_imguiSrvHeap.Get(),
        m_imguiSrvHeap->GetCPUDescriptorHandleForHeapStart(),
        m_imguiSrvHeap->GetGPUDescriptorHandleForHeapStart()
    );

    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    return true;
}

// --------------------------------------------------------
// Frame rendering
// --------------------------------------------------------
void DXRenderer::Render() noexcept
{
    // Time update
    m_timer.Tick();
    float dt = (float)m_timer.Delta();
    if (dt > 0.1f) dt = 0.1f;

    // ====================================================
    // CAMERA INPUT HANDLING (before updating camera)
    // ====================================================

    // Ignore camera input when ImGui is capturing the mouse
    if (!IsImGuiCapturingMouse())
    {
        // 1) Mouse wheel zoom
        if (m_wheelTicks != 0.0f) {
            m_camera.Zoom(m_wheelTicks);
            m_wheelTicks = 0.0f;
        }

        // 2) Alt + LMB → orbit mode around origin (for now)
        if (m_isLeftMouseDown && m_isAltDown)
        {
            DirectX::XMFLOAT3 pivot = { 0.0f, 0.0f, 0.0f };
            m_camera.SetOrbitMode(true, pivot);
            m_camera.Rotate(m_mouseDeltaX, m_mouseDeltaY);
        }
        else
        {
            m_camera.SetOrbitMode(false);
        }

        // 3) RMB held → FPS free look + WASD movement
        if (m_isRightMouseDown && !m_camera.IsOrbitMode())
        {
            // Mouse look
            m_camera.Rotate(m_mouseDeltaX, m_mouseDeltaY);

            // WASD-style movement
            m_camera.SetMovement(
                m_keyW,  // forward
                m_keyS,  // backward
                m_keyA,  // left
                m_keyD,  // right
                m_keyE,  // up
                m_keyQ,  // down
                m_isShiftDown // speed multiplier
            );
        }
        else
        {
            // No movement when RMB is not pressed
            m_camera.SetMovement(false, false, false, false, false, false, m_isShiftDown);
        }
    }
    else
    {
        // When ImGui is active, stop camera movement
        m_camera.SetMovement(false, false, false, false, false, false, false);
        m_wheelTicks = 0.0f;
    }

    // Reset per-frame mouse delta
    m_mouseDeltaX = 0.0f;
    m_mouseDeltaY = 0.0f;

    // Finally update camera transform
    m_camera.Update(dt);

    // ====================================================
    // COMMAND LIST RESET
    // ====================================================
    if (FAILED(m_cmdAlloc->Reset())) return;
    if (FAILED(m_cmdList->Reset(m_cmdAlloc.Get(), m_pso.Get()))) return;

    // --------- IMGUI NEW FRAME ---------
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Simple debug window
    {
        double fps = 0.0;
        m_timer.SampleFps(0.5, fps);

        ImGui::Begin("Info");
        ImGui::Text("FPS: %.2f", fps);

        DirectX::XMFLOAT4X4 viewFloats;
        DirectX::XMStoreFloat4x4(&viewFloats, m_camera.GetViewMatrix());
        ImGui::Text("Camera Pos: %.2f %.2f %.2f",
            viewFloats._41, viewFloats._42, viewFloats._43);
        ImGui::End();
    }

    // --------- BACKBUFFER PREP ---------
    const UINT bb = m_swapChain->GetCurrentBackBufferIndex();
    ID3D12Resource* backBuffer = m_renderTargets[bb].Get();

    D3D12_RESOURCE_STATES beforeState =
        m_firstFrame ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_PRESENT;

    auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffer, beforeState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_cmdList->ResourceBarrier(1, &toRT);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvStart =
        m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{
        rtvStart.ptr + SIZE_T(bb) * SIZE_T(m_rtvDescriptorSize)
    };
    D3D12_CPU_DESCRIPTOR_HANDLE dsv =
        m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

    const float clearColor[4] = { 0.08f, 0.10f, 0.20f, 1.0f };
    m_cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    m_cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    m_cmdList->ClearDepthStencilView(
        dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // --------- SCENE RENDER (quad) ---------
    ID3D12DescriptorHeap* sceneHeaps[] = { m_cbvHeap.Get() };
    m_cmdList->SetDescriptorHeaps(1, sceneHeaps);

    DirectX::XMMATRIX V = m_camera.GetViewMatrix();
    DirectX::XMMATRIX P = m_camera.GetProjectionMatrix();
    DirectX::XMMATRIX M = DirectX::XMMatrixIdentity();

    DirectX::XMMATRIX MVP = M * V * P;
    DirectX::XMMATRIX MVPt = DirectX::XMMatrixTranspose(MVP);

    if (m_cbMapped)
    {
        CbMvp cb{};
        DirectX::XMStoreFloat4x4(&cb.mvp, MVPt);
        std::memcpy(m_cbMapped, &cb, sizeof(CbMvp));
    }

    m_cmdList->RSSetViewports(1, &m_viewport);
    m_cmdList->RSSetScissorRects(1, &m_scissor);
    m_cmdList->SetGraphicsRootSignature(m_rootSig.Get());
    m_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_cmdList->IASetVertexBuffers(0, 1, &m_vbView);

    auto gpuStart = m_cbvHeap->GetGPUDescriptorHandleForHeapStart();
    m_cmdList->SetGraphicsRootDescriptorTable(0, gpuStart); // CBV

    UINT inc = m_device->GetDevice()->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_GPU_DESCRIPTOR_HANDLE gpuSrv{ gpuStart.ptr + SIZE_T(inc) };
    m_cmdList->SetGraphicsRootDescriptorTable(1, gpuSrv);   // SRV

    m_cmdList->DrawInstanced(6, 1, 0, 0);

    // --------- IMGUI RENDER ---------
    ImGui::Render();

    ID3D12DescriptorHeap* imguiHeaps[] = { m_imguiSrvHeap.Get() };
    m_cmdList->SetDescriptorHeaps(1, imguiHeaps);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_cmdList.Get());

    // --------- PRESENT ---------
    auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffer,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    m_cmdList->ResourceBarrier(1, &toPresent);

    m_cmdList->Close();
    ID3D12CommandList* lists[] = { m_cmdList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);

    m_swapChain->Present(1, 0);
    m_firstFrame = false;

    const UINT64 fenceToWait = m_fenceValue++;
    m_commandQueue->Signal(m_fence.Get(), fenceToWait);
    if (m_fence->GetCompletedValue() < fenceToWait)
    {
        m_fence->SetEventOnCompletion(fenceToWait, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}



// --------------------------------------------------------
// Resize
// --------------------------------------------------------
void DXRenderer::Resize(UINT width, UINT height) noexcept {
    if (!m_swapChain || width == 0 || height == 0) return;

    WaitForGpu();

    for (auto& rt : m_renderTargets) rt.Reset();
    m_depth.Reset();

    m_width = width;
    m_height = height;

    m_swapChain->ResizeBuffers(kBufferCount, width, height, m_backbufferFormat, 0);
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    CreateRenderTargets();
    CreateDepthResources();

    m_viewport = { 0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f };
    m_scissor = { 0, 0, int(width), int(height) };
    m_firstFrame = true;

    float aspect = (m_height == 0) ? 1.0f : float(m_width) / float(m_height);
    m_camera.SetProjection(XM_PIDIV4, aspect, 0.1f, 1000.0f);
}

// --------------------------------------------------------
// Yardımcı Fonksiyonlar
// --------------------------------------------------------
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
    if (FAILED(m_device->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtvHeap)))) return false;
    m_rtvDescriptorSize = m_device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    return true;
}

bool DXRenderer::CreateRenderTargets() noexcept {
    m_renderTargets.resize(kBufferCount);
    D3D12_CPU_DESCRIPTOR_HANDLE start = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < kBufferCount; ++i) {
        if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])))) return false;
        D3D12_CPU_DESCRIPTOR_HANDLE handle{ start.ptr + SIZE_T(i) * SIZE_T(m_rtvDescriptorSize) };
        m_device->GetDevice()->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, handle);
    }
    return true;
}

bool DXRenderer::CreateDepthResources() noexcept {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = m_width;
    desc.Height = m_height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = m_depthFormat;
    desc.SampleDesc = { 1, 0 };
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clear{};
    clear.Format = m_depthFormat;
    clear.DepthStencil.Depth = 1.0f;
    clear.DepthStencil.Stencil = 0;

    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;

    if (FAILED(m_device->GetDevice()->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear, IID_PPV_ARGS(&m_depth))))
        return false;

    D3D12_DESCRIPTOR_HEAP_DESC dh{};
    dh.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dh.NumDescriptors = 1;
    dh.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    if (FAILED(m_device->GetDevice()->CreateDescriptorHeap(&dh, IID_PPV_ARGS(&m_dsvHeap)))) return false;

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
    dsv.Format = m_depthFormat;
    dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv.Flags = D3D12_DSV_FLAG_NONE;

    m_device->GetDevice()->CreateDepthStencilView(m_depth.Get(), &dsv, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    return true;
}

bool DXRenderer::CreateRootSignature() noexcept {
    D3D12_DESCRIPTOR_RANGE rngCBV{};
    rngCBV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    rngCBV.NumDescriptors = 1;

    D3D12_ROOT_DESCRIPTOR_TABLE tblCBV{};
    tblCBV.NumDescriptorRanges = 1;
    tblCBV.pDescriptorRanges = &rngCBV;

    D3D12_ROOT_PARAMETER paramCBV{};
    paramCBV.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    paramCBV.DescriptorTable = tblCBV;
    paramCBV.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_DESCRIPTOR_RANGE rngSRV{};
    rngSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    rngSRV.NumDescriptors = 1;

    D3D12_ROOT_DESCRIPTOR_TABLE tblSRV{};
    tblSRV.NumDescriptorRanges = 1;
    tblSRV.pDescriptorRanges = &rngSRV;

    D3D12_ROOT_PARAMETER paramSRV{};
    paramSRV.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    paramSRV.DescriptorTable = tblSRV;
    paramSRV.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_PARAMETER paramsRS[2] = { paramCBV, paramSRV };

    D3D12_STATIC_SAMPLER_DESC samp{};
    samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samp.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    samp.MaxLOD = D3D12_FLOAT32_MAX;
    samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rs{};
    rs.NumParameters = 2;
    rs.pParameters = paramsRS;
    rs.NumStaticSamplers = 1;
    rs.pStaticSamplers = &samp;
    rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> blob, err;
    if (FAILED(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))) return false;
    return SUCCEEDED(m_device->GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_rootSig)));
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

    D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = m_rootSig.Get();
    pso.VS = { vs.data(), (UINT)vs.size() };
    pso.PS = { ps.data(), (UINT)ps.size() };
    pso.InputLayout = { layout, _countof(layout) };
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = m_backbufferFormat;
    pso.DSVFormat = m_depthFormat;
    pso.SampleDesc = { 1, 0 };

    return SUCCEEDED(m_device->GetDevice()->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_pso)));
}

bool DXRenderer::CreateTriangleVB() noexcept {
    const Vertex verts[6] = {
        { XMFLOAT3(-0.5f,  0.5f, 0.0f), XMFLOAT3(1,0,0), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT3(0,1,0), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT3(0,0,1), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(-0.5f,  0.5f, 0.0f), XMFLOAT3(1,0,0), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(0.5f,  0.5f, 0.0f), XMFLOAT3(0,1,1), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT3(0,1,0), XMFLOAT2(1.0f, 1.0f) },
    };

    const UINT vbSize = sizeof(verts);
    D3D12_HEAP_PROPERTIES heap{ D3D12_HEAP_TYPE_UPLOAD };
    D3D12_RESOURCE_DESC buf = CD3DX12_RESOURCE_DESC::Buffer(vbSize);

    if (FAILED(m_device->GetDevice()->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &buf, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_vertexBuffer))))
        return false;

    void* mapped = nullptr;
    m_vertexBuffer->Map(0, nullptr, &mapped);
    std::memcpy(mapped, verts, vbSize);
    m_vertexBuffer->Unmap(0, nullptr);

    m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vbView.SizeInBytes = vbSize;
    m_vbView.StrideInBytes = sizeof(Vertex);

    return true;
}

bool DXRenderer::CreateConstantBuffer() noexcept {
    m_cbSize = (sizeof(CbMvp) + 255) & ~255u;
    D3D12_HEAP_PROPERTIES heap{ D3D12_HEAP_TYPE_UPLOAD };
    D3D12_RESOURCE_DESC buf = CD3DX12_RESOURCE_DESC::Buffer(m_cbSize);

    if (FAILED(m_device->GetDevice()->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &buf, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_cbUpload))))
        return false;

    if (FAILED(m_cbUpload->Map(0, nullptr, reinterpret_cast<void**>(&m_cbMapped)))) return false;

    D3D12_DESCRIPTOR_HEAP_DESC h{};
    h.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    h.NumDescriptors = 2; // CBV + SRV
    h.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    if (FAILED(m_device->GetDevice()->CreateDescriptorHeap(&h, IID_PPV_ARGS(&m_cbvHeap)))) return false;

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv{};
    cbv.BufferLocation = m_cbUpload->GetGPUVirtualAddress();
    cbv.SizeInBytes = m_cbSize;
    m_device->GetDevice()->CreateConstantBufferView(&cbv, m_cbvHeap->GetCPUDescriptorHandleForHeapStart());

    return true;
}

bool DXRenderer::CreateCheckerTextureSRV() noexcept {
    const UINT W = 256; const UINT H = 256;
    std::vector<uint32_t> pixels(W * H);
    for (UINT i = 0; i < W * H; ++i) {
        bool c = (((i % W) / 32) ^ ((i / W) / 32)) & 1;
        uint8_t v = c ? 220 : 40;
        pixels[i] = 0xFF000000 | (v << 16) | (v << 8) | v;
    }

    D3D12_HEAP_PROPERTIES defHeap{ D3D12_HEAP_TYPE_DEFAULT };
    D3D12_RESOURCE_DESC tex = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, W, H);
    if (FAILED(m_device->GetDevice()->CreateCommittedResource(&defHeap, D3D12_HEAP_FLAG_NONE, &tex, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_tex)))) return false;

    const UINT64 uploadSize = GetRequiredIntermediateSize(m_tex.Get(), 0, 1);
    D3D12_HEAP_PROPERTIES upHeap{ D3D12_HEAP_TYPE_UPLOAD };
    auto upDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
    ComPtr<ID3D12Resource> upload;
    if (FAILED(m_device->GetDevice()->CreateCommittedResource(&upHeap, D3D12_HEAP_FLAG_NONE, &upDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload)))) return false;

    D3D12_SUBRESOURCE_DATA s{};
    s.pData = pixels.data();
    s.RowPitch = W * 4;
    s.SlicePitch = s.RowPitch * H;

    if (FAILED(m_cmdAlloc->Reset())) return false;
    if (FAILED(m_cmdList->Reset(m_cmdAlloc.Get(), nullptr))) return false;

    UpdateSubresources(m_cmdList.Get(), m_tex.Get(), upload.Get(), 0, 0, 1, &s);
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_tex.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_cmdList->ResourceBarrier(1, &barrier);
    m_cmdList->Close();

    ID3D12CommandList* lists[] = { m_cmdList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);
    WaitForGpu();

    D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = m_cbvHeap->GetCPUDescriptorHandleForHeapStart();
    UINT inc = m_device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE cpuSrv{ cpuStart.ptr + SIZE_T(inc) };

    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;
    m_device->GetDevice()->CreateShaderResourceView(m_tex.Get(), &srv, cpuSrv);
    return true;
}

bool DXRenderer::LoadFileBinary(const wchar_t* path, std::vector<uint8_t>& data) noexcept {
    FILE* f = nullptr;
    _wfopen_s(&f, path, L"rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    data.resize(sz);
    if (sz > 0) fread(data.data(), 1, sz, f);
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
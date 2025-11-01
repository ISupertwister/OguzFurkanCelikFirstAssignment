#include "DXRenderer.h"
#include "DXDevice.h"
#include <cassert>

using Microsoft::WRL::ComPtr;

bool DXRenderer::Initialize(HWND hwnd, DXDevice* device, UINT width, UINT height) noexcept {
    assert(hwnd && device);

    m_device = device;
    m_width = width;
    m_height = height;

    // 1) Queue
    if (!CreateCommandQueue()) return false;

    // 2) Swap chain
    if (!CreateSwapChain(hwnd, width, height)) return false;

    // 3) RTV heap
    if (!CreateRTVDescriptorHeap()) return false;

    // 4) Back buffer resources + RTVs
    if (!CreateRenderTargets()) return false;

    // 5) Command allocator/list (for simple frame recording)
    if (FAILED(m_device->GetDevice()->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmdAlloc))))
        return false;

    if (FAILED(m_device->GetDevice()->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&m_cmdList))))
        return false;

    // Command list is created in "open" state. Close so we can Reset() each frame.
    m_cmdList->Close();

    // 6) Fence for basic CPU-GPU sync
    if (FAILED(m_device->GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence))))
        return false;

    m_fenceValue = 1;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) return false;

    // Cache the current back-buffer index
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    return true;
}

void DXRenderer::Render() noexcept {
    // Reset allocator & command list for this frame.
    m_cmdAlloc->Reset();
    m_cmdList->Reset(m_cmdAlloc.Get(), nullptr);

    // Resolve the back-buffer resource for this frame.
    const UINT bb = m_swapChain->GetCurrentBackBufferIndex();
    ID3D12Resource* backBuffer = m_renderTargets[bb].Get();

    // Transition to RENDER_TARGET (first frame uses COMMON -> RT).
    D3D12_RESOURCE_BARRIER toRT{};
    toRT.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toRT.Transition.pResource = backBuffer;
    toRT.Transition.StateBefore = m_firstFrame ? D3D12_RESOURCE_STATE_COMMON
        : D3D12_RESOURCE_STATE_PRESENT;
    toRT.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toRT.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_cmdList->ResourceBarrier(1, &toRT);

    // Bind RTV and clear
    const D3D12_CPU_DESCRIPTOR_HANDLE rtvStart =
        m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = { rtvStart.ptr + bb * m_rtvDescriptorSize };

    m_cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    const float clearColor[4] = { 0.10f, 0.12f, 0.16f, 1.0f }; // dark bluish tone
    m_cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

    // Transition back to PRESENT
    D3D12_RESOURCE_BARRIER toPresent = toRT;
    std::swap(toPresent.Transition.StateBefore, toPresent.Transition.StateAfter);
    m_cmdList->ResourceBarrier(1, &toPresent);

    // Close, execute and present
    m_cmdList->Close();
    ID3D12CommandList* lists[] = { m_cmdList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);

    m_swapChain->Present(1, 0); // vsync on

    // After first present, subsequent frames will start from PRESENT state
    m_firstFrame = false;

    // Simple sync (keeps things deterministic while learning)
    const UINT64 fenceToWait = m_fenceValue++;
    m_commandQueue->Signal(m_fence.Get(), fenceToWait);
    if (m_fence->GetCompletedValue() < fenceToWait) {
        m_fence->SetEventOnCompletion(fenceToWait, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    // Update cached frame index
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void DXRenderer::Resize(UINT width, UINT height) noexcept {
    if (!m_swapChain) return;
    if (width == 0 || height == 0) return; // minimized

    WaitForGpu();

    // Release previous back-buffers
    for (auto& rt : m_renderTargets) rt.Reset();

    m_width = width;
    m_height = height;

    m_swapChain->ResizeBuffers(kBufferCount, width, height, m_backbufferFormat, 0);
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Recreate RTVs for the resized buffers
    CreateRenderTargets();

    // Next frame will again start from COMMON for new buffers
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

    D3D12_CPU_DESCRIPTOR_HANDLE start =
        m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < kBufferCount; ++i) {
        if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]))))
            return false;

        D3D12_CPU_DESCRIPTOR_HANDLE handle{ start.ptr + i * m_rtvDescriptorSize };
        m_device->GetDevice()->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, handle);
    }
    return true;
}

void DXRenderer::WaitForGpu() noexcept {
    if (!m_commandQueue || !m_fence) return;
    const UINT64 fenceToWait = m_fenceValue++;
    m_commandQueue->Signal(m_fence.Get(), fenceToWait);
    if (m_fence->GetCompletedValue() < fenceToWait) {
        m_fence->SetEventOnCompletion(fenceToWait, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}


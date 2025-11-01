#include "DXRenderer.h"
#include "DXDevice.h"
#include <cassert>

using Microsoft::WRL::ComPtr;

bool DXRenderer::Initialize(HWND hwnd, DXDevice* device, UINT width, UINT height) noexcept
{
    assert(hwnd && device);
    m_device = device;
    m_width = width;
    m_height = height;

    // 1) Command queue
    if (!CreateCommandQueue()) return false;

    // 2) Swapchain
    if (!CreateSwapChain(hwnd, width, height)) return false;

    // 3) RTV heap
    if (!CreateRTVDescriptorHeap()) return false;

    // 4) Backbuffers + RTVs
    if (!CreateRenderTargets()) return false;

    // 5) Command allocator + list (no PSO yet)
    if (FAILED(m_device->GetDevice()->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmdAlloc))))
        return false;

    if (FAILED(m_device->GetDevice()->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&m_cmdList))))
        return false;

    // Created in "open" state → close so we can Reset() per frame
    m_cmdList->Close();

    // 6) Fence + event (once)
    if (FAILED(m_device->GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence))))
        return false;

    m_fenceValue = 1;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) return false;

    // Cache initial backbuffer index
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // First frame starts from COMMON state for backbuffers
    m_firstFrame = true;

    return true;
}

void DXRenderer::Render() noexcept
{
    // Which backbuffer do we render this frame?
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Reset allocator + command list for recording
    m_cmdAlloc->Reset();
    m_cmdList->Reset(m_cmdAlloc.Get(), nullptr);

    // Transition: PRESENT/COMMON -> RENDER_TARGET
    D3D12_RESOURCE_BARRIER toRT{};
    toRT.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toRT.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    toRT.Transition.StateBefore = m_firstFrame ? D3D12_RESOURCE_STATE_COMMON
        : D3D12_RESOURCE_STATE_PRESENT;
    toRT.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toRT.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_cmdList->ResourceBarrier(1, &toRT);

    // RTV handle for this backbuffer
    auto rtvStart = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{
        rtvStart.ptr + SIZE_T(m_frameIndex) * SIZE_T(m_rtvDescriptorSize)
    };

    // Bind & clear
    m_cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    const float clearColor[4] = { 0.05f, 0.15f, 0.32f, 1.0f }; // navy-ish
    m_cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // Transition back: RENDER_TARGET -> PRESENT
    D3D12_RESOURCE_BARRIER toPresent{};
    toPresent.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toPresent.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    toPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    toPresent.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_cmdList->ResourceBarrier(1, &toPresent);

    // Submit
    m_cmdList->Close();
    ID3D12CommandList* lists[] = { m_cmdList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);

    // Present (1 = vsync)
    m_swapChain->Present(1, 0);

    // After first frame, we start from PRESENT
    m_firstFrame = false;

    // Simple GPU sync (learning phase)
    WaitForGpu();
}

void DXRenderer::Resize(UINT width, UINT height) noexcept
{
    if (!m_swapChain) return;
    if (width == 0 || height == 0) return; // minimized

    // Ensure GPU is idle wrt current resources
    WaitForGpu();

    // Release old backbuffers
    for (auto& rt : m_renderTargets) rt.Reset();

    m_width = width;
    m_height = height;

    // Resize swapchain buffers
    m_swapChain->ResizeBuffers(kBufferCount, width, height, m_backbufferFormat, 0);
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Recreate RTVs for resized buffers
    CreateRenderTargets();

    // New buffers start from COMMON
    m_firstFrame = true;
}

bool DXRenderer::CreateCommandQueue() noexcept
{
    D3D12_COMMAND_QUEUE_DESC desc{};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    return SUCCEEDED(m_device->GetDevice()->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_commandQueue)));
}

bool DXRenderer::CreateSwapChain(HWND hwnd, UINT width, UINT height) noexcept
{
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

bool DXRenderer::CreateRTVDescriptorHeap() noexcept
{
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

bool DXRenderer::CreateRenderTargets() noexcept
{
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

void DXRenderer::WaitForGpu() noexcept
{
    if (!m_commandQueue || !m_fence) return;

    const UINT64 fenceToWait = m_fenceValue++;
    m_commandQueue->Signal(m_fence.Get(), fenceToWait);

    if (m_fence->GetCompletedValue() < fenceToWait) {
        m_fence->SetEventOnCompletion(fenceToWait, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}



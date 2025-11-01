#pragma once
#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <memory>

class DXDevice; // forward declaration

class DXRenderer {
public:
    DXRenderer() noexcept = default;
    ~DXRenderer() = default;

    DXRenderer(const DXRenderer&) = delete;
    DXRenderer& operator=(const DXRenderer&) = delete;

    // Initialize all D3D12 objects that depend on the window size/handle.
    bool Initialize(HWND hwnd, DXDevice* device, UINT width, UINT height) noexcept;

    // Record & execute a very simple frame (clear + present).
    void Render() noexcept;

    // Recreate size-dependent resources (called on WM_SIZE).
    void Resize(UINT width, UINT height) noexcept;

private:
    // Helpers split by responsibility (keep functions small and readable).
    bool CreateCommandQueue() noexcept;
    bool CreateSwapChain(HWND hwnd, UINT width, UINT height) noexcept;
    bool CreateRTVDescriptorHeap() noexcept;
    bool CreateRenderTargets() noexcept;

    void WaitForGpu() noexcept; // simple CPU-GPU sync for resize/shutdown

private:
    // External device/factory holder (non-owning raw pointer by design).
    DXDevice* m_device{ nullptr };

    // Core D3D12 objects
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>      m_commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain4>         m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>    m_rtvHeap;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_renderTargets;

    // Per-frame recording
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>  m_cmdAlloc;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cmdList;

    // Sync
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue{ 0 };
    HANDLE m_fenceEvent{ nullptr };

    // Cached values
    UINT m_rtvDescriptorSize{ 0 };
    UINT m_frameIndex{ 0 };
    UINT m_width{ 0 };
    UINT m_height{ 0 };

    static constexpr UINT kBufferCount = 2;
    DXGI_FORMAT m_backbufferFormat{ DXGI_FORMAT_R8G8B8A8_UNORM };

    bool m_firstFrame{ true }; // handle COMMON state on very first frame
};


#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <vector>
#include <cstdint>
#include <windows.h>
#include "FrameTimer.h"   // <-- for real dt + FPS

class DXDevice; // forward decl

class DXRenderer {
public:
    DXRenderer() noexcept = default;
    ~DXRenderer() = default;

    bool Initialize(HWND hwnd, DXDevice* device, UINT width, UINT height) noexcept;
    void Render() noexcept;
    void Resize(UINT width, UINT height) noexcept;

private:
    // ---- helpers ----
    bool CreateCommandQueue() noexcept;
    bool CreateSwapChain(HWND hwnd, UINT width, UINT height) noexcept;
    bool CreateRTVDescriptorHeap() noexcept;
    bool CreateRenderTargets() noexcept;

    bool CreateRootSignature() noexcept;
    bool CreatePipelineState() noexcept;
    bool CreateTriangleVB() noexcept;
    bool CreateConstantBuffer() noexcept;      // NEW

    bool LoadFileBinary(const wchar_t* path, std::vector<uint8_t>& data) noexcept;
    void WaitForGpu() noexcept;

private:
    // Simple vertex: position + color
    struct Vertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 color;
    };

    // ---- Constant Buffer (MVP) ----
    // Must be 256-byte aligned for CBV
    struct alignas(256) CbMvp {
        DirectX::XMFLOAT4X4 mvp;
    };

private:
    // window handle (for optional title updates)
    HWND      m_hwnd{ nullptr };

    // device / factory provided via DXDevice
    DXDevice* m_device{ nullptr };

    // command submission
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>       m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>   m_cmdAlloc;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cmdList;

    // swap-chain & RTVs
    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    UINT m_rtvDescriptorSize{ 0 };
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_renderTargets;

    // sync
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    HANDLE  m_fenceEvent{ nullptr };
    UINT64  m_fenceValue{ 0 };
    UINT    m_frameIndex{ 0 };
    bool    m_firstFrame{ true };

    // pipeline
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSig;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;

    // geometry
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW               m_vbView{};

    // CBV resources (for MVP)
    Microsoft::WRL::ComPtr<ID3D12Resource>       m_cbUpload;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
    UINT     m_cbSize{ 0 };
    uint8_t* m_cbMapped{ nullptr };

    // timing
    FrameTimer m_timer;         // real dt + fps sampling
    float      m_time{ 0.0f };  // accumulated rotation (radians)

    // viewport/scissor
    D3D12_VIEWPORT m_viewport{};
    D3D12_RECT     m_scissor{};

    // backbuffer info
    static constexpr UINT kBufferCount = 2;
    DXGI_FORMAT           m_backbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    // size
    UINT m_width{ 0 };
    UINT m_height{ 0 };
};





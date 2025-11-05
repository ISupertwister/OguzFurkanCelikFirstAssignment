#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <vector>
#include <memory>

class DXDevice;

class DXRenderer {
public:
    DXRenderer() noexcept = default;
    ~DXRenderer() = default;

    DXRenderer(const DXRenderer&) = delete;
    DXRenderer& operator=(const DXRenderer&) = delete;

    bool Initialize(HWND hwnd, DXDevice* device, UINT width, UINT height) noexcept;
    void Render() noexcept;
    void Resize(UINT width, UINT height) noexcept;

private:
    // Simple vertex: position + color
    struct Vertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 color;
    };

    // Setup helpers
    bool CreateCommandQueue() noexcept;
    bool CreateSwapChain(HWND hwnd, UINT width, UINT height) noexcept;
    bool CreateRTVDescriptorHeap() noexcept;
    bool CreateRenderTargets() noexcept;

    bool CreateRootSignature() noexcept;
    bool CreatePipelineState() noexcept;
    bool CreateTriangleVB() noexcept;

    bool LoadFileBinary(const wchar_t* path, std::vector<uint8_t>& data) noexcept;
    void WaitForGpu() noexcept;

private:
    DXDevice* m_device{ nullptr };

    // Present path
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>     m_commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain4>        m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>   m_rtvHeap;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_renderTargets;
    UINT m_rtvDescriptorSize{ 0 };
    UINT m_frameIndex{ 0 };
    static constexpr UINT kBufferCount = 2;
    DXGI_FORMAT m_backbufferFormat{ DXGI_FORMAT_R8G8B8A8_UNORM };

    // Commands
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>     m_cmdAlloc;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>  m_cmdList;

    // Sync
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue{ 0 };
    HANDLE m_fenceEvent{ nullptr };
    bool   m_firstFrame{ true };

    // Pipeline for triangle
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSig;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;
    Microsoft::WRL::ComPtr<ID3D12Resource>      m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW                    m_vbView{};

    D3D12_VIEWPORT m_viewport{};
    D3D12_RECT     m_scissor{};

    // Backbuffer size
    UINT m_width{ 0 };
    UINT m_height{ 0 };
};



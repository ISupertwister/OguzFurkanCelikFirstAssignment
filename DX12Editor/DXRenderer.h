#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <memory>
#include <DirectXMath.h>
using namespace DirectX;

class DXDevice;

class DXRenderer {
public:
    DXRenderer() noexcept = default;
    ~DXRenderer() = default;

    DXRenderer(const DXRenderer&) = delete;
    DXRenderer& operator=(const DXRenderer&) = delete;

    // Initialize renderer with OS window, device and initial size
    bool Initialize(HWND hwnd, DXDevice* device, UINT width, UINT height) noexcept;

    // Record & submit a minimal frame (clear + present)
    void Render() noexcept;

    // Recreate size-dependent resources on resize
    void Resize(UINT width, UINT height) noexcept;

private:
    // Internal helpers
    bool CreateCommandQueue() noexcept;
    bool CreateSwapChain(HWND hwnd, UINT width, UINT height) noexcept;
    bool CreateRTVDescriptorHeap() noexcept;
    bool CreateRenderTargets() noexcept;
    void WaitForGpu() noexcept;

private:
    DXDevice* m_device{ nullptr };

    Microsoft::WRL::ComPtr<ID3D12CommandQueue>          m_commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain4>             m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>        m_rtvHeap;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_renderTargets;

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>      m_cmdAlloc;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>   m_cmdList;

    Microsoft::WRL::ComPtr<ID3D12Fence>                 m_fence;
    UINT64  m_fenceValue{ 0 };
    HANDLE  m_fenceEvent{ nullptr };

    UINT    m_rtvDescriptorSize{ 0 };
    UINT    m_frameIndex{ 0 };
    UINT    m_width{ 0 };
    UINT    m_height{ 0 };

    static constexpr UINT kBufferCount = 2;
    DXGI_FORMAT m_backbufferFormat{ DXGI_FORMAT_R8G8B8A8_UNORM };

    bool    m_firstFrame{ true }; // first frame starts from COMMON -> RT
    // Vertex for our colored triangle
struct Vertex {
    XMFLOAT3 position;
    XMFLOAT3 color;
    };

private:
    // Pipeline pieces
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSig;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;

    // Geometry
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer; // we use an UPLOAD heap for now
    D3D12_VERTEX_BUFFER_VIEW               m_vbView{};

    // Viewport / Scissor
    D3D12_VIEWPORT m_viewport{};
    D3D12_RECT     m_scissor{};

private:
    // Setup helpers
    bool CreateRootSignature() noexcept;
    bool CreatePipelineState() noexcept;
    bool CreateTriangleVB() noexcept;

    // Small file loader for .cso
    bool LoadFileBinary(const wchar_t* path, std::vector<uint8_t>& data) noexcept;
};



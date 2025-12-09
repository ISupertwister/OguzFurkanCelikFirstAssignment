#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <vector>
#include <cstdint>
#include <windows.h>
#include "FrameTimer.h"
#include "DXMesh.h"
#include "Camera.h" 

// ImGui Headers
#include "imgui/imgui.h" 
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"

using Microsoft::WRL::ComPtr;

class DXDevice;
class DXMesh;

class DXRenderer {
public:
    DXRenderer() noexcept = default;
    ~DXRenderer() noexcept;

    bool Initialize(HWND hwnd, DXDevice* device, UINT width, UINT height) noexcept;
    void Render() noexcept;
    void Resize(UINT width, UINT height) noexcept;

    // Access to camera (if needed)
    Camera* GetCamera() { return &m_camera; }

    // ImGui Win32 hook
    static LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static bool IsImGuiCapturingMouse() { return ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow); }

    // Input hooks from Window / Win32
    void OnMouseMove(float dx, float dy);      // accumulate mouse delta
    void OnMouseWheel(float wheelTicks);       // mouse wheel ticks (usually +/-1 per notch)
    void OnRightMouseDown();
    void OnRightMouseUp();
    void OnLeftMouseDown();
    void OnLeftMouseUp();
    void OnKeyDown(UINT key);
    void OnKeyUp(UINT key);

private:
    bool CreateCommandQueue() noexcept;
    bool CreateSwapChain(HWND hwnd, UINT width, UINT height) noexcept;
    bool CreateRTVDescriptorHeap() noexcept;
    bool CreateRenderTargets() noexcept;
    bool CreateRootSignature() noexcept;
    bool CreatePipelineState() noexcept;
    bool CreateTriangleVB() noexcept;
    bool CreateConstantBuffer() noexcept;
    bool CreateDepthResources() noexcept;
    bool CreateCheckerTextureSRV() noexcept;
    bool LoadFileBinary(const wchar_t* path, std::vector<uint8_t>& data) noexcept;
    void WaitForGpu() noexcept;

private:
    struct Vertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 color;
        DirectX::XMFLOAT2 uv;
    };

    struct alignas(256) CbMvp {
        DirectX::XMFLOAT4X4 mvp;
    };

private:
    HWND m_hwnd{ nullptr };
    DXDevice* m_device{ nullptr };

    Microsoft::WRL::ComPtr<ID3D12CommandQueue>        m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    m_cmdAlloc;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cmdList;

    Microsoft::WRL::ComPtr<IDXGISwapChain4>      m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    UINT m_rtvDescriptorSize{ 0 };
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_renderTargets;

    Microsoft::WRL::ComPtr<ID3D12Resource>       m_depth;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    DXGI_FORMAT m_depthFormat = DXGI_FORMAT_D32_FLOAT;

    ComPtr<ID3D12DescriptorHeap> m_imguiSrvHeap;

    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    HANDLE  m_fenceEvent{ nullptr };
    UINT64  m_fenceValue{ 0 };
    UINT    m_frameIndex{ 0 };
    bool    m_firstFrame{ true };

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSig;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vbView{};

    Microsoft::WRL::ComPtr<ID3D12Resource>       m_cbUpload;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
    UINT     m_cbSize{ 0 };
    uint8_t* m_cbMapped{ nullptr };

    Microsoft::WRL::ComPtr<ID3D12Resource> m_tex;

    FrameTimer m_timer;
    float m_time{ 0.0f };

    D3D12_VIEWPORT m_viewport{};
    D3D12_RECT     m_scissor{};

    static constexpr UINT kBufferCount = 2;
    DXGI_FORMAT m_backbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    UINT m_width{ 0 };
    UINT m_height{ 0 };
    DXMesh m_testMesh;

    Camera m_camera;

    // --- Input state for camera controls ---
    bool  m_isRightMouseDown{ false };
    bool  m_isLeftMouseDown{ false };
    bool  m_isAltDown{ false };
    bool  m_isShiftDown{ false };

    bool  m_keyW{ false };
    bool  m_keyA{ false };
    bool  m_keyS{ false };
    bool  m_keyD{ false };
    bool  m_keyQ{ false }; // down
    bool  m_keyE{ false }; // up

    float m_mouseDeltaX{ 0.0f };
    float m_mouseDeltaY{ 0.0f };
    float m_wheelTicks{ 0.0f };
};

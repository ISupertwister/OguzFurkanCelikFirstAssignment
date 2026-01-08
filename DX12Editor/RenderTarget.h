#pragma once
#include <d3d12.h>
#include <wrl.h>

class DXDevice;

class RenderTarget
{
public:
    bool Initialize(
        DXDevice* device,
        UINT width,
        UINT height,
        DXGI_FORMAT colorFormat,
        DXGI_FORMAT depthFormat,
        ID3D12DescriptorHeap* imguiSrvHeap,
        UINT imguiSrvIndex);

    void Release();
    void Resize(UINT width, UINT height);

    // Handles for rendering
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const { return m_rtvCpu; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const { return m_dsvCpu; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGpu() const { return m_srvGpu; }

    ID3D12Resource* GetColorResource() const { return m_color.Get(); }

    UINT GetWidth() const { return m_width; }
    UINT GetHeight() const { return m_height; }

private:
    bool CreateResources();

private:
    DXDevice* m_device = nullptr;

    UINT m_width = 0;
    UINT m_height = 0;

    DXGI_FORMAT m_colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_depthFormat = DXGI_FORMAT_D32_FLOAT;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_color;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depth;

    // RTV/DSV heaps owned by this RenderTarget
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;

    D3D12_CPU_DESCRIPTOR_HANDLE m_rtvCpu{};
    D3D12_CPU_DESCRIPTOR_HANDLE m_dsvCpu{};

    // SRV goes into ImGui's shader-visible heap
    ID3D12DescriptorHeap* m_imguiSrvHeap = nullptr;
    UINT m_imguiSrvIndex = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE m_srvGpu{};
};

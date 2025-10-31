#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <string>

class DXDevice {
public:
    DXDevice() noexcept = default;
    ~DXDevice() = default;

    DXDevice(const DXDevice&) = delete;
    DXDevice& operator=(const DXDevice&) = delete;

    // Initialize D3D12 device and DXGI factory/adapter
    bool Initialize(bool enableDebugLayer) noexcept;

    // Accessors (const-correct)
    ID3D12Device* GetDevice()  const noexcept { return m_device.Get(); }
    IDXGIFactory6* GetFactory() const noexcept { return m_factory.Get(); }
    IDXGIAdapter1* GetAdapter() const noexcept { return m_adapter.Get(); }
    bool           IsWarp()     const noexcept { return m_isWarp; }
    std::wstring   AdapterDesc() const noexcept { return m_adapterDesc; }

private:
    // Split responsibilities into small helpers
    bool CreateFactory(bool enableDebugLayer) noexcept;
    bool PickAdapter() noexcept;    // choose HW adapter or fallback to WARP
    bool CreateDevice() noexcept;

private:
    Microsoft::WRL::ComPtr<IDXGIFactory6> m_factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> m_adapter;
    Microsoft::WRL::ComPtr<ID3D12Device>  m_device;

    bool m_isWarp{ false };
    std::wstring m_adapterDesc;
};

#include "DXDevice.h"
#include <cassert>

using Microsoft::WRL::ComPtr;

bool DXDevice::Initialize(bool enableDebugLayer) noexcept {
#if _DEBUG
    if (enableDebugLayer) {
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
            debug->EnableDebugLayer();
        }
    }
#endif
    if (!CreateFactory(enableDebugLayer)) return false;
    if (!PickAdapter()) return false;
    if (!CreateDevice()) return false;
    return true;
}

bool DXDevice::CreateFactory(bool enableDebugLayer) noexcept {
    UINT flags = 0;
#if _DEBUG
    if (enableDebugLayer) flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    return SUCCEEDED(CreateDXGIFactory2(flags, IID_PPV_ARGS(&m_factory)));
}

bool DXDevice::PickAdapter() noexcept {
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; m_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
            __uuidof(ID3D12Device), nullptr))) {
            m_adapter = adapter;
            m_adapterDesc = desc.Description;
            m_isWarp = false;
            return true;
        }
    }
    if (FAILED(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))))
        return false;

    DXGI_ADAPTER_DESC1 desc{};
    adapter->GetDesc1(&desc);
    m_adapter = adapter;
    m_adapterDesc = desc.Description;
    m_isWarp = true;
    return true;
}

bool DXDevice::CreateDevice() noexcept {
    return SUCCEEDED(D3D12CreateDevice(
        m_adapter.Get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&m_device)
    ));
}


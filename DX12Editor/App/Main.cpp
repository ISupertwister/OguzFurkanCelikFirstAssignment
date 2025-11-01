#include <windows.h>
#include "Window.h"
#include "../Core/DXDevice.h"
#include "../Core/DXRenderer.h"
#include <sstream>
    

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    Window window(L"DX12 Editor", 1600, 900);

    if (!window.Create()) return -1;

    DXDevice dx;
    if (!dx.Initialize(true)) {
        MessageBoxW(window.GetHWND(), L"DX12 device init failed", L"Error", MB_OK | MB_ICONERROR);
        return -2;
    }

    DXRenderer renderer;
    if (!renderer.Initialize(window.GetHWND(), &dx, window.Width(), window.Height())) {
        MessageBoxW(window.GetHWND(), L"Renderer init failed", L"Error", MB_OK | MB_ICONERROR);
        return -3;
    }

    std::wstringstream ss;
    ss << L"DX12 Editor  —  Adapter: " << dx.AdapterDesc();
    SetWindowTextW(window.GetHWND(), ss.str().c_str());

    MSG msg{};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            renderer.Render();
        }
    }
    return 0;
}


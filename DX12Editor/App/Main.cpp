#include <windows.h>
#include "Window.h"
#include "../Core/DXDevice.h"   // relative path: src/App -> src/Core
#include <sstream>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    Window window(L"DX12 Editor", 1600, 900);

    // Create the actual OS window first so HWND is valid.
    if (!window.Create()) return -1;

    DXDevice dx;
    if (!dx.Initialize(true)) {
        // Use nullptr if you want to show a message box even when HWND is not reliable.
        MessageBoxW(nullptr, L"DX12 device init failed", L"Error", MB_OK | MB_ICONERROR);
        return -2;
    }

    // Show adapter name in the window title (now HWND is valid)
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
            // TODO: Renderer::Render();
        }
    }
    return 0;
}

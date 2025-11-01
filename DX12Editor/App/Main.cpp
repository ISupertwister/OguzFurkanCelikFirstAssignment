#include <windows.h>
#include "Window.h"
#include "DXDevice.h"
#include "DXRenderer.h"
#include "FrameTimer.h"
#include <sstream>
#include <iomanip>   // NEW: fixed, setprecision

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
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

    // Live resize hook
    window.SetResizeCallback([&](UINT w, UINT h) {
        renderer.Resize(w, h);
        });

    // Base title with adapter name (kept constant)
    std::wstringstream baseTitle;
    baseTitle << L"DX12 Editor  —  Adapter: " << dx.AdapterDesc();

    FrameTimer timer;
    double fps = 0.0;
    std::wstringstream title;

    MSG msg{};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // Tick timer and render a frame
            timer.Tick();
            renderer.Render();

            // Update window title every 0.5s with FPS and ms
            if (timer.SampleFps(0.5, fps)) {
                const double ms = (fps > 0.0) ? (1000.0 / fps) : 0.0;
                title.str(L"");
                title.clear();
                title << baseTitle.str()
                    << L"  |  FPS: " << std::fixed << std::setprecision(0) << fps
                    << L"  (" << std::setprecision(2) << ms << L" ms)";
                SetWindowTextW(window.GetHWND(), title.str().c_str());
            }
        }
    }
    return 0;
}





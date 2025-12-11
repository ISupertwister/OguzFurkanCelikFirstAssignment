#include <windows.h>
#include "Window.h"
#include <windowsx.h> 
#include "../Core/DXDevice.h"
#include "../Core/DXRenderer.h"
#include "../Core/FrameTimer.h"
#include <sstream>

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

    // Live resize hook -> let renderer recreate size-dependent resources
    window.SetResizeCallback([&](UINT w, UINT h) {
        renderer.Resize(w, h);
        });

    // Win32 message callback: route input + ImGui to renderer
    window.SetMessageCallback([&](HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT
        {
            // 1) Let ImGui process the message first
            if (DXRenderer::ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
                return 1; // ImGui consumed the message

            // 2) Camera input handling (we only update state here)
            switch (msg)
            {
            case WM_RBUTTONDOWN:
                renderer.OnRightMouseDown();
                break;
            case WM_RBUTTONUP:
                renderer.OnRightMouseUp();
                break;

            case WM_LBUTTONDOWN:
                renderer.OnLeftMouseDown();
                break;
            case WM_LBUTTONUP:
                renderer.OnLeftMouseUp();
                break;

            case WM_MOUSEMOVE:
            {
                static int lastX = GET_X_LPARAM(lParam);
                static int lastY = GET_Y_LPARAM(lParam);

                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);

                float dx = float(x - lastX);
                float dy = float(y - lastY);

                lastX = x;
                lastY = y;

                renderer.OnMouseMove(dx, dy);
                break;
            }

            case WM_MOUSEWHEEL:
            {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam); // usually +/-120 per notch
                float ticks = static_cast<float>(delta) / 120.0f;
                renderer.OnMouseWheel(ticks);
                break;
            }

            case WM_KEYDOWN:
                renderer.OnKeyDown(static_cast<UINT>(wParam));
                break;

            case WM_KEYUP:
                renderer.OnKeyUp(static_cast<UINT>(wParam));
                break;
            }

            // Returning 0 means "not fully handled, let Window do its thing"
            return 0;
        });
    // Put adapter name in the title once
    std::wstringstream base;
    base << L"DX12 Editor  —  Adapter: " << dx.AdapterDesc();
    SetWindowTextW(window.GetHWND(), base.str().c_str());

    // FPS/ms in title
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
            timer.Tick();
            renderer.Render();

            // Update window title every ~0.5s
            if (timer.SampleFps(0.5, fps)) {
                const double ms = (fps > 0.0) ? (1000.0 / fps) : 0.0;
                title.str(L"");
                title.clear();
                title << base.str()
                    << L"  |  FPS: " << std::fixed << std::setprecision(0) << fps
                    << L"  (" << std::setprecision(2) << ms << L" ms)";
                SetWindowTextW(window.GetHWND(), title.str().c_str());
            }
        }
    }
    return 0;
}






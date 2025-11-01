#pragma once
#include <windows.h>
#include <string>
#include <functional> // NEW

class Window {
public:
    Window(const std::wstring& title, int width, int height) noexcept;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool Create() noexcept;
    HWND GetHWND() const noexcept { return m_hWnd; }
    int  Width()  const noexcept { return m_width; }
    int  Height() const noexcept { return m_height; }

    using ResizeCallback = std::function<void(UINT, UINT)>;
    void SetResizeCallback(ResizeCallback cb) noexcept { m_onResize = std::move(cb); }

private:
    static LRESULT CALLBACK WndProcSetup(HWND, UINT, WPARAM, LPARAM) noexcept;
    static LRESULT CALLBACK WndProcThunk(HWND, UINT, WPARAM, LPARAM) noexcept;
    LRESULT WndProc(HWND, UINT, WPARAM, LPARAM) noexcept;

private:
    std::wstring m_title;
    int m_width{ 0 };
    int m_height{ 0 };
    HWND m_hWnd{ nullptr };
    HINSTANCE m_hInstance{ nullptr };

    ResizeCallback m_onResize{};
};


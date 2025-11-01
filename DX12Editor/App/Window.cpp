#include "Window.h"

static const wchar_t* kWndClass = L"DX12EditorWindowClass";

Window::Window(const std::wstring& title, int width, int height) noexcept
    : m_title(title), m_width(width), m_height(height) {
    m_hInstance = GetModuleHandleW(nullptr);
}

Window::~Window() {
    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
    if (m_hInstance) {
        UnregisterClassW(kWndClass, m_hInstance);
    }
}

LRESULT CALLBACK Window::WndProcSetup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept {
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        Window* pThis = reinterpret_cast<Window*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        SetWindowLongPtrW(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Window::WndProcThunk));
        return pThis->WndProc(hWnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK Window::WndProcThunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept {
    Window* pThis = reinterpret_cast<Window*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    return pThis->WndProc(hWnd, msg, wParam, lParam);
}

LRESULT Window::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept {
    switch (msg) {
    case WM_SIZE:
        // Update cached size
        m_width = LOWORD(lParam);
        m_height = HIWORD(lParam);

        // Call user callback unless minimized
        if (m_onResize && wParam != SIZE_MINIMIZED) {
            m_onResize(static_cast<UINT>(m_width), static_cast<UINT>(m_height));
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

bool Window::Create() noexcept {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = &Window::WndProcSetup;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kWndClass;
    if (!RegisterClassExW(&wc)) return false;

    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT wr{ 0, 0, m_width, m_height };
    AdjustWindowRect(&wr, style, FALSE);

    m_hWnd = CreateWindowExW(
        0, kWndClass, m_title.c_str(), style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, m_hInstance, this);

    if (!m_hWnd) return false;
    ShowWindow(m_hWnd, SW_SHOW);
    UpdateWindow(m_hWnd);
    return true;
}

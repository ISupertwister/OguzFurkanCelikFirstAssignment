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

// =======================================================
// Create() implementation (eksik olan kısım buydu)
// =======================================================
bool Window::Create() noexcept
{
    // 1) Window class register
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = &Window::WndProcSetup;   // first messages go to setup
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = m_hInstance;
    wc.hIcon = nullptr;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = kWndClass;
    wc.hIconSm = nullptr;

    if (!RegisterClassExW(&wc))
        return false;

    // 2) Desired client area → real window size
    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT wr{ 0, 0, m_width, m_height };
    AdjustWindowRect(&wr, style, FALSE);

    // 3) Create the window
    m_hWnd = CreateWindowExW(
        0,
        kWndClass,
        m_title.c_str(),
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left,
        wr.bottom - wr.top,
        nullptr,
        nullptr,
        m_hInstance,
        this // lpParam → Window* (WndProcSetup içinde alıyoruz)
    );

    if (!m_hWnd)
        return false;

    ShowWindow(m_hWnd, SW_SHOW);
    UpdateWindow(m_hWnd);
    return true;
}

// =======================================================

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

    // First allow external callback (ImGui + input)
    if (m_onMessage) {
        LRESULT handled = m_onMessage(hWnd, msg, wParam, lParam);
        if (handled != 0) {
            return handled;
        }
    }

    switch (msg) {
    case WM_SIZE: {
        m_width = LOWORD(lParam);
        m_height = HIWORD(lParam);
        if (m_onResize) m_onResize(m_width, m_height);
        return 0;
    }
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

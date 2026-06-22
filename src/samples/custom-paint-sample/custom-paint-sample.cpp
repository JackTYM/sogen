#include <windows.h>

namespace
{
    constexpr auto* kWindowClassName = "CustomPaintSampleClass";
    constexpr auto* kWindowTitle = "Custom Paint Sample";

    LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        switch (msg)
        {
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_CREATE:
            PostMessageA(hwnd, WM_CLOSE, 0, 0);
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            break;
        }

        return DefWindowProcA(hwnd, msg, wp, lp);
    }
}

int main()
{
    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &window_proc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = kWindowClassName;
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);

    if (!RegisterClassExA(&wc))
    {
        return 10;
    }

    HWND hwnd = CreateWindowExA(0, kWindowClassName, kWindowTitle, WS_OVERLAPPEDWINDOW, 200, 200, 320, 180, nullptr, nullptr, wc.hInstance,
                                nullptr);
    if (!hwnd)
    {
        return 11;
    }

    MSG msg{};
    while (GetMessageA(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return static_cast<int>(msg.wParam);
}

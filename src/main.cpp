#include <windows.h>

#include <array>
#include <cwchar>
#include <string>

namespace
{
constexpr wchar_t kWindowClassName[] = L"CalculagraphTimerWindowClass";
constexpr wchar_t kWindowTitle[] = L"Calculagraph Timer";
constexpr int kWindowWidth = 360;
constexpr int kWindowHeight = 88;
constexpr int kTimerIntervalMs = 1000;
constexpr int kMaxSecondsBeforeReset = 3600;
constexpr int kDisplayId = 1001;
constexpr int kResetButtonId = 1002;
constexpr int kMainTimerId = 1;
constexpr BYTE kWindowAlpha = 220;

int g_elapsedSeconds = 0;
HWND g_displayHandle = nullptr;
HWND g_resetButtonHandle = nullptr;
HFONT g_displayFont = nullptr;
HBRUSH g_backgroundBrush = nullptr;

// 将当前累计秒数转换为“分:秒”字符串，并显示到界面。
void UpdateTimeDisplay()
{
    const int minutes = g_elapsedSeconds / 60;
    const int seconds = g_elapsedSeconds % 60;

    std::array<wchar_t, 16> buffer{};
    const int written = swprintf(buffer.data(), buffer.size(), L"%02d:%02d", minutes, seconds);

    if (written > 0)
    {
        SetWindowTextW(g_displayHandle, buffer.data());
    }
}

// 将计时器状态重置到 00:00，并立即刷新显示。
void ResetTimerState()
{
    g_elapsedSeconds = 0;
    UpdateTimeDisplay();
}

// 每秒更新一次计时，如果达到 1 小时自动归零后继续计时。
void TickTimer()
{
    const int nextSeconds = g_elapsedSeconds + 1;

    if (nextSeconds >= kMaxSecondsBeforeReset)
    {
        g_elapsedSeconds = 0;
    }
    else
    {
        g_elapsedSeconds = nextSeconds;
    }

    UpdateTimeDisplay();
}

// 创建用于显示计时文本和重置按钮的子控件。
void CreateChildControls(HWND windowHandle)
{
    const int margin = 12;
    const int buttonWidth = 84;
    const int buttonHeight = 34;
    const int displayWidth = kWindowWidth - margin * 3 - buttonWidth;
    const int displayHeight = 44;

    g_displayHandle = CreateWindowExW(
        0,
        L"STATIC",
        L"00:00",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        margin,
        margin,
        displayWidth,
        displayHeight,
        windowHandle,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kDisplayId)),
        GetModuleHandleW(nullptr),
        nullptr);

    g_resetButtonHandle = CreateWindowExW(
        0,
        L"BUTTON",
        L"重置",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        margin * 2 + displayWidth,
        margin,
        buttonWidth,
        buttonHeight,
        windowHandle,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kResetButtonId)),
        GetModuleHandleW(nullptr),
        nullptr);

    const int fontHeight = -28;
    g_displayFont = CreateFontW(
        fontHeight,
        0,
        0,
        0,
        FW_BOLD,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        FF_DONTCARE,
        L"Segoe UI");

    if (g_displayFont != nullptr)
    {
        SendMessageW(g_displayHandle, WM_SETFONT, reinterpret_cast<WPARAM>(g_displayFont), TRUE);
    }
}

// 统一清理 GDI 资源，防止窗口销毁后发生资源泄露。
void CleanupResources()
{
    if (g_displayFont != nullptr)
    {
        DeleteObject(g_displayFont);
        g_displayFont = nullptr;
    }

    if (g_backgroundBrush != nullptr)
    {
        DeleteObject(g_backgroundBrush);
        g_backgroundBrush = nullptr;
    }
}

// 处理主窗口消息，完成计时逻辑和交互行为。
LRESULT CALLBACK WindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        CreateChildControls(windowHandle);

        const COLORREF backgroundColor = RGB(245, 245, 245);
        g_backgroundBrush = CreateSolidBrush(backgroundColor);

        SetLayeredWindowAttributes(windowHandle, 0, kWindowAlpha, LWA_ALPHA);
        SetTimer(windowHandle, kMainTimerId, kTimerIntervalMs, nullptr);
        UpdateTimeDisplay();
        return 0;
    }
    case WM_TIMER:
    {
        const UINT_PTR timerId = static_cast<UINT_PTR>(wParam);

        if (timerId == kMainTimerId)
        {
            TickTimer();
        }
        return 0;
    }
    case WM_COMMAND:
    {
        const int controlId = LOWORD(wParam);
        const int notificationCode = HIWORD(wParam);

        if (controlId == kResetButtonId && notificationCode == BN_CLICKED)
        {
            ResetTimerState();
            return 0;
        }
        break;
    }
    case WM_CTLCOLORSTATIC:
    {
        HDC deviceContext = reinterpret_cast<HDC>(wParam);
        HWND controlHandle = reinterpret_cast<HWND>(lParam);

        if (controlHandle == g_displayHandle)
        {
            SetTextColor(deviceContext, RGB(220, 0, 0));
            SetBkMode(deviceContext, TRANSPARENT);

            if (g_backgroundBrush != nullptr)
            {
                return reinterpret_cast<INT_PTR>(g_backgroundBrush);
            }
        }
        break;
    }
    case WM_CLOSE:
    {
        const int result = MessageBoxW(
            windowHandle,
            L"确定要关闭计时器吗？",
            L"确认关闭",
            MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);

        if (result == IDYES)
        {
            DestroyWindow(windowHandle);
        }
        return 0;
    }
    case WM_DESTROY:
    {
        KillTimer(windowHandle, kMainTimerId);
        CleanupResources();
        PostQuitMessage(0);
        return 0;
    }
    default:
        break;
    }

    return DefWindowProcW(windowHandle, message, wParam, lParam);
}

// 注册窗口类并创建固定尺寸、置顶、半透明的主窗口。
bool CreateMainWindow(HINSTANCE instanceHandle, int showCommand)
{
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instanceHandle;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kWindowClassName;

    const ATOM classAtom = RegisterClassW(&windowClass);

    if (classAtom == 0)
    {
        return false;
    }

    constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    constexpr DWORD exStyle = WS_EX_TOPMOST | WS_EX_LAYERED;

    HWND windowHandle = CreateWindowExW(
        exStyle,
        kWindowClassName,
        kWindowTitle,
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kWindowWidth,
        kWindowHeight,
        nullptr,
        nullptr,
        instanceHandle,
        nullptr);

    if (windowHandle == nullptr)
    {
        return false;
    }

    ShowWindow(windowHandle, showCommand);
    UpdateWindow(windowHandle);
    return true;
}
} // namespace

// 程序入口：创建窗口并进入标准 Win32 消息循环。
int WINAPI wWinMain(HINSTANCE instanceHandle, HINSTANCE, PWSTR, int showCommand)
{
    const bool created = CreateMainWindow(instanceHandle, showCommand);

    if (!created)
    {
        return 1;
    }

    MSG message{};

    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}

#include <windows.h>

#include <array>
#include <cwchar>
#include <string>

namespace
{
constexpr wchar_t kWindowClassName[] = L"CalculagraphTimerWindowClass";
constexpr wchar_t kWindowTitle[] = L"Calculagraph Timer";
constexpr int kWindowClientWidth = 184;
constexpr int kWindowClientHeight = 52;
constexpr int kTimerIntervalMs = 1000;
constexpr int kMaxSecondsBeforeReset = 3600;
constexpr int kMainDisplayId = 1001;
constexpr int kMainResetButtonId = 1002;
constexpr int kTargetDisplayId = 1003;
constexpr int kTargetInputId = 1004;
constexpr int kTargetResetButtonId = 1005;
constexpr int kMainTimerId = 1;
constexpr BYTE kWindowAlpha = 220;
constexpr int kLayoutMargin = 4;
constexpr int kLayoutGap = 4;
constexpr int kControlRowHeight = 20;
constexpr int kTargetDisplayWidth = 40;
constexpr int kTargetInputWidth = 86;
constexpr int kResetButtonWidth = 42;

struct TargetTimeState
{
    bool hasValidTarget = false;
    SYSTEMTIME targetTime{};
};

int g_elapsedSeconds = 0;
HWND g_mainDisplayHandle = nullptr;
HWND g_mainResetButtonHandle = nullptr;
HWND g_targetDisplayHandle = nullptr;
HWND g_targetInputHandle = nullptr;
HWND g_targetResetButtonHandle = nullptr;
HFONT g_mainDisplayFont = nullptr;
HFONT g_smallControlFont = nullptr;
HBRUSH g_backgroundBrush = nullptr;
TargetTimeState g_targetTimeState{};

// 将当前累计秒数转换为“分:秒”字符串，并显示到原有计时区域。
void UpdateTimeDisplay()
{
    const int minutes = g_elapsedSeconds / 60;
    const int seconds = g_elapsedSeconds % 60;

    std::array<wchar_t, 16> buffer{};
    const int written = swprintf(buffer.data(), buffer.size(), L"%02d:%02d", minutes, seconds);

    if (written > 0)
    {
        SetWindowTextW(g_mainDisplayHandle, buffer.data());
    }
}

// 将计时器状态重置到 00:00，并立即刷新原有计时显示。
void ResetTimerState()
{
    g_elapsedSeconds = 0;
    UpdateTimeDisplay();
}

// 每秒更新一次原有计时，如果达到 1 小时自动归零后继续计时。
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

// 判断字符是否属于输入两端允许忽略的空白字符。
bool IsWhitespace(wchar_t character)
{
    const bool isSpace = character == L' ';
    const bool isTab = character == L'\t';
    const bool isLineBreak = character == L'\n' || character == L'\r';

    return isSpace || isTab || isLineBreak;
}

// 去掉输入文本首尾空白，方便后续只验证真正的时间内容。
std::wstring TrimWhitespace(const std::wstring& text)
{
    std::size_t beginIndex = 0;
    const std::size_t textLength = text.length();

    while (beginIndex < textLength && IsWhitespace(text[beginIndex]))
    {
        beginIndex += 1;
    }

    std::size_t endIndex = textLength;

    while (endIndex > beginIndex && IsWhitespace(text[endIndex - 1]))
    {
        endIndex -= 1;
    }

    const std::size_t trimmedLength = endIndex - beginIndex;
    return text.substr(beginIndex, trimmedLength);
}

// 判断字符是否为 ASCII 数字，时间格式只接受 00:00:00 这样的半角数字。
bool IsDigit(wchar_t character)
{
    const bool isAtLeastZero = character >= L'0';
    const bool isAtMostNine = character <= L'9';

    return isAtLeastZero && isAtMostNine;
}

// 将两个数字字符转换成 0 到 99 之间的整数。
int ParseTwoDigitNumber(wchar_t tensCharacter, wchar_t onesCharacter)
{
    const int zeroCode = static_cast<int>(L'0');
    const int tensCode = static_cast<int>(tensCharacter);
    const int onesCode = static_cast<int>(onesCharacter);
    const int tensValue = tensCode - zeroCode;
    const int onesValue = onesCode - zeroCode;
    const int number = tensValue * 10 + onesValue;

    return number;
}

// 严格解析 HH:MM:SS 格式，并校验时、分、秒的有效范围。
bool TryParseTimeText(const std::wstring& text, int& hours, int& minutes, int& seconds)
{
    constexpr std::size_t kExpectedLength = 8;

    if (text.length() != kExpectedLength)
    {
        return false;
    }

    const bool hasFirstColon = text[2] == L':';
    const bool hasSecondColon = text[5] == L':';

    if (!hasFirstColon || !hasSecondColon)
    {
        return false;
    }

    const bool firstHourDigit = IsDigit(text[0]);
    const bool secondHourDigit = IsDigit(text[1]);
    const bool firstMinuteDigit = IsDigit(text[3]);
    const bool secondMinuteDigit = IsDigit(text[4]);
    const bool firstSecondDigit = IsDigit(text[6]);
    const bool secondSecondDigit = IsDigit(text[7]);

    const bool hasValidHourDigits = firstHourDigit && secondHourDigit;
    const bool hasValidMinuteDigits = firstMinuteDigit && secondMinuteDigit;
    const bool hasValidSecondDigits = firstSecondDigit && secondSecondDigit;
    const bool allDigits = hasValidHourDigits && hasValidMinuteDigits && hasValidSecondDigits;

    if (!allDigits)
    {
        return false;
    }

    const int parsedHours = ParseTwoDigitNumber(text[0], text[1]);
    const int parsedMinutes = ParseTwoDigitNumber(text[3], text[4]);
    const int parsedSeconds = ParseTwoDigitNumber(text[6], text[7]);

    const bool hoursInRange = parsedHours >= 0 && parsedHours <= 23;
    const bool minutesInRange = parsedMinutes >= 0 && parsedMinutes <= 59;
    const bool secondsInRange = parsedSeconds >= 0 && parsedSeconds <= 59;

    if (!hoursInRange || !minutesInRange || !secondsInRange)
    {
        return false;
    }

    hours = parsedHours;
    minutes = parsedMinutes;
    seconds = parsedSeconds;
    return true;
}

// 从编辑框读取当前文本，独立封装以保持按钮逻辑清晰。
std::wstring ReadTargetInputText()
{
    const int textLength = GetWindowTextLengthW(g_targetInputHandle);

    if (textLength <= 0)
    {
        return {};
    }

    const int bufferLength = textLength + 1;
    std::wstring buffer(static_cast<std::size_t>(bufferLength), L'\0');
    GetWindowTextW(g_targetInputHandle, buffer.data(), bufferLength);

    std::wstring text = buffer.c_str();
    return text;
}

// 把本地 SYSTEMTIME 转换为 64 位 FILETIME 数值，便于计算秒级差值。
bool TryConvertSystemTimeToFileTicks(const SYSTEMTIME& systemTime, unsigned long long& ticks)
{
    FILETIME fileTime{};
    const BOOL converted = SystemTimeToFileTime(&systemTime, &fileTime);

    if (converted == FALSE)
    {
        return false;
    }

    ULARGE_INTEGER largeInteger{};
    largeInteger.LowPart = fileTime.dwLowDateTime;
    largeInteger.HighPart = fileTime.dwHighDateTime;
    ticks = largeInteger.QuadPart;
    return true;
}

// 根据已保存的目标时间和当前系统时间，刷新目标时间差输出框。
void UpdateTargetDifferenceDisplay()
{
    if (!g_targetTimeState.hasValidTarget)
    {
        return;
    }

    SYSTEMTIME currentTime{};
    GetLocalTime(&currentTime);

    unsigned long long targetTicks = 0;
    const bool targetConverted = TryConvertSystemTimeToFileTicks(g_targetTimeState.targetTime, targetTicks);

    unsigned long long currentTicks = 0;
    const bool currentConverted = TryConvertSystemTimeToFileTicks(currentTime, currentTicks);

    if (!targetConverted || !currentConverted)
    {
        SetWindowTextW(g_targetDisplayHandle, L"错误");
        return;
    }

    unsigned long long largerTicks = currentTicks;
    unsigned long long smallerTicks = targetTicks;
    const bool targetIsLater = targetTicks > currentTicks;

    if (targetIsLater)
    {
        largerTicks = targetTicks;
        smallerTicks = currentTicks;
    }

    const unsigned long long differenceTicks = largerTicks - smallerTicks;
    constexpr unsigned long long kTicksPerSecond = 10000000ULL;
    const unsigned long long differenceSeconds = differenceTicks / kTicksPerSecond;

    if (differenceSeconds >= static_cast<unsigned long long>(kMaxSecondsBeforeReset))
    {
        SetWindowTextW(g_targetDisplayHandle, L"超出");
        return;
    }

    const unsigned long long minutes = differenceSeconds / 60ULL;
    const unsigned long long seconds = differenceSeconds % 60ULL;

    std::array<wchar_t, 16> buffer{};
    const int written = swprintf(buffer.data(), buffer.size(), L"%02llu:%02llu", minutes, seconds);

    if (written > 0)
    {
        SetWindowTextW(g_targetDisplayHandle, buffer.data());
    }
}

// 处理新增重置按钮：空输入取当前时间，非空输入严格解析为当天目标时间。
void ResetTargetTimeState()
{
    SYSTEMTIME resetClickTime{};
    GetLocalTime(&resetClickTime);

    const std::wstring rawText = ReadTargetInputText();
    const std::wstring trimmedText = TrimWhitespace(rawText);

    if (trimmedText.empty())
    {
        g_targetTimeState.targetTime = resetClickTime;
        g_targetTimeState.hasValidTarget = true;
        UpdateTargetDifferenceDisplay();
        return;
    }

    int parsedHours = 0;
    int parsedMinutes = 0;
    int parsedSeconds = 0;
    const bool parsed = TryParseTimeText(trimmedText, parsedHours, parsedMinutes, parsedSeconds);

    if (!parsed)
    {
        g_targetTimeState.hasValidTarget = false;
        SetWindowTextW(g_targetDisplayHandle, L"错误");
        return;
    }

    SYSTEMTIME targetTime = resetClickTime;
    targetTime.wHour = static_cast<WORD>(parsedHours);
    targetTime.wMinute = static_cast<WORD>(parsedMinutes);
    targetTime.wSecond = static_cast<WORD>(parsedSeconds);
    targetTime.wMilliseconds = 0;

    g_targetTimeState.targetTime = targetTime;
    g_targetTimeState.hasValidTarget = true;
    UpdateTargetDifferenceDisplay();
}

// 为指定控件设置字体，并通过单独函数避免重复发送窗口消息。
void ApplyFont(HWND controlHandle, HFONT fontHandle)
{
    if (fontHandle == nullptr)
    {
        return;
    }

    SendMessageW(controlHandle, WM_SETFONT, reinterpret_cast<WPARAM>(fontHandle), TRUE);
}

// 保存一行控件的水平和垂直位置，便于后续创建窗口控件。
struct ControlRowLayout
{
    int top = 0;
    int displayLeft = 0;
    int displayWidth = 0;
    int inputLeft = 0;
    int inputWidth = 0;
    int buttonLeft = 0;
};

// 计算目标时间行的控件位置，让输入框只保留 HH:MM:SS 所需宽度。
ControlRowLayout BuildTargetRowLayout()
{
    ControlRowLayout layout{};

    layout.top = kLayoutMargin;
    layout.displayLeft = kLayoutMargin;
    layout.displayWidth = kTargetDisplayWidth;

    const int inputLeftOffset = layout.displayWidth + kLayoutGap;
    layout.inputLeft = layout.displayLeft + inputLeftOffset;
    layout.inputWidth = kTargetInputWidth;

    const int buttonLeftOffset = layout.inputWidth + kLayoutGap;
    layout.buttonLeft = layout.inputLeft + buttonLeftOffset;

    return layout;
}

// 计算主计时行的控件位置，宽度跟随紧凑窗口的客户区。
ControlRowLayout BuildMainRowLayout()
{
    ControlRowLayout layout{};

    const int firstRowBottom = kLayoutMargin + kControlRowHeight;
    layout.top = firstRowBottom + kLayoutGap;
    layout.displayLeft = kLayoutMargin;

    const int rightMarginWidth = kLayoutMargin;
    const int buttonAndGapWidth = kResetButtonWidth + kLayoutGap;
    const int reservedWidth = rightMarginWidth + buttonAndGapWidth;
    layout.displayWidth = kWindowClientWidth - kLayoutMargin - reservedWidth;

    const int buttonLeftOffset = layout.displayWidth + kLayoutGap;
    layout.buttonLeft = layout.displayLeft + buttonLeftOffset;

    return layout;
}

// 创建一个静态文本控件，用于显示计时结果或目标时间差。
HWND CreateDisplayControl(
    HWND parentWindowHandle,
    int controlId,
    const wchar_t* text,
    int left,
    int top,
    int width,
    DWORD alignmentStyle)
{
    const DWORD style = WS_CHILD | WS_VISIBLE | alignmentStyle;
    HINSTANCE instanceHandle = GetModuleHandleW(nullptr);
    HMENU menuHandle = reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId));

    HWND controlHandle = CreateWindowExW(
        0,
        L"STATIC",
        text,
        style,
        left,
        top,
        width,
        kControlRowHeight,
        parentWindowHandle,
        menuHandle,
        instanceHandle,
        nullptr);

    return controlHandle;
}

// 创建一个重置按钮控件，两行按钮共用相同尺寸以保持界面整齐。
HWND CreateResetButton(HWND parentWindowHandle, int controlId, int left, int top)
{
    const DWORD style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
    HINSTANCE instanceHandle = GetModuleHandleW(nullptr);
    HMENU menuHandle = reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId));

    HWND controlHandle = CreateWindowExW(
        0,
        L"BUTTON",
        L"重置",
        style,
        left,
        top,
        kResetButtonWidth,
        kControlRowHeight,
        parentWindowHandle,
        menuHandle,
        instanceHandle,
        nullptr);

    return controlHandle;
}

// 创建目标时间输入框，宽度控制为刚好显示类似 17:44:44 的时间字符串。
HWND CreateTargetInput(HWND parentWindowHandle, const ControlRowLayout& layout)
{
    const DWORD style = WS_CHILD | WS_VISIBLE | ES_CENTER | ES_AUTOHSCROLL;
    HINSTANCE instanceHandle = GetModuleHandleW(nullptr);
    HMENU menuHandle = reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTargetInputId));

    HWND controlHandle = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        style,
        layout.inputLeft,
        layout.top,
        layout.inputWidth,
        kControlRowHeight,
        parentWindowHandle,
        menuHandle,
        instanceHandle,
        nullptr);

    return controlHandle;
}

// 创建两行紧凑界面控件：目标时间行在上方，原有计时器行在下方。
void CreateChildControls(HWND windowHandle)
{
    const ControlRowLayout targetRow = BuildTargetRowLayout();
    const ControlRowLayout mainRow = BuildMainRowLayout();

    g_targetDisplayHandle = CreateDisplayControl(
        windowHandle,
        kTargetDisplayId,
        L"--:--",
        targetRow.displayLeft,
        targetRow.top,
        targetRow.displayWidth,
        SS_CENTER);

    g_targetInputHandle = CreateTargetInput(windowHandle, targetRow);

    g_targetResetButtonHandle = CreateResetButton(
        windowHandle,
        kTargetResetButtonId,
        targetRow.buttonLeft,
        targetRow.top);

    g_mainDisplayHandle = CreateDisplayControl(
        windowHandle,
        kMainDisplayId,
        L"00:00",
        mainRow.displayLeft,
        mainRow.top,
        mainRow.displayWidth,
        SS_LEFT);

    g_mainResetButtonHandle = CreateResetButton(
        windowHandle,
        kMainResetButtonId,
        mainRow.buttonLeft,
        mainRow.top);

    const int mainFontHeight = -17;
    g_mainDisplayFont = CreateFontW(
        mainFontHeight,
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

    const int smallFontHeight = -13;
    g_smallControlFont = CreateFontW(
        smallFontHeight,
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        FF_DONTCARE,
        L"Segoe UI");

    ApplyFont(g_mainDisplayHandle, g_mainDisplayFont);
    ApplyFont(g_targetDisplayHandle, g_smallControlFont);
    ApplyFont(g_targetInputHandle, g_smallControlFont);
    ApplyFont(g_targetResetButtonHandle, g_smallControlFont);
    ApplyFont(g_mainResetButtonHandle, g_smallControlFont);
}

// 统一清理 GDI 资源，防止窗口销毁后发生资源泄露。
void CleanupResources()
{
    if (g_mainDisplayFont != nullptr)
    {
        DeleteObject(g_mainDisplayFont);
        g_mainDisplayFont = nullptr;
    }

    if (g_smallControlFont != nullptr)
    {
        DeleteObject(g_smallControlFont);
        g_smallControlFont = nullptr;
    }

    if (g_backgroundBrush != nullptr)
    {
        DeleteObject(g_backgroundBrush);
        g_backgroundBrush = nullptr;
    }
}

// 处理主窗口消息，完成计时逻辑、目标时间差刷新和交互行为。
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
            UpdateTargetDifferenceDisplay();
        }
        return 0;
    }
    case WM_COMMAND:
    {
        const int controlId = LOWORD(wParam);
        const int notificationCode = HIWORD(wParam);

        if (controlId == kMainResetButtonId && notificationCode == BN_CLICKED)
        {
            ResetTimerState();
            return 0;
        }

        if (controlId == kTargetResetButtonId && notificationCode == BN_CLICKED)
        {
            ResetTargetTimeState();
            return 0;
        }
        break;
    }
    case WM_CTLCOLORSTATIC:
    {
        HDC deviceContext = reinterpret_cast<HDC>(wParam);
        HWND controlHandle = reinterpret_cast<HWND>(lParam);

        if (controlHandle == g_mainDisplayHandle)
        {
            SetTextColor(deviceContext, RGB(220, 0, 0));
            SetBkMode(deviceContext, TRANSPARENT);

            if (g_backgroundBrush != nullptr)
            {
                return reinterpret_cast<INT_PTR>(g_backgroundBrush);
            }
        }

        if (controlHandle == g_targetDisplayHandle)
        {
            SetTextColor(deviceContext, RGB(0, 90, 160));
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

    RECT windowRect{};
    windowRect.right = kWindowClientWidth;
    windowRect.bottom = kWindowClientHeight;

    const BOOL adjusted = AdjustWindowRectEx(&windowRect, style, FALSE, exStyle);

    if (adjusted == FALSE)
    {
        return false;
    }

    const int adjustedWindowWidth = windowRect.right - windowRect.left;
    const int adjustedWindowHeight = windowRect.bottom - windowRect.top;

    HWND windowHandle = CreateWindowExW(
        exStyle,
        kWindowClassName,
        kWindowTitle,
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        adjustedWindowWidth,
        adjustedWindowHeight,
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

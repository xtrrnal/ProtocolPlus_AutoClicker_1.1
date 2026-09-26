#define UNICODE
#define _UNICODE

#include <windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cwchar>

// ============================================================
// Protocol+ Auto Clicker
// ============================================================

static HWND g_mainWindow = nullptr;
static HWND g_cpsEdit = nullptr;
static HWND g_buttonCombo = nullptr;
static HWND g_hotkeyText = nullptr;
static HWND g_status = nullptr;
static HWND g_toggle = nullptr;
static HWND g_changeHotkey = nullptr;

static std::atomic<bool> g_running(false);
static std::atomic<bool> g_stop(false);

static HBRUSH g_bgBrush = nullptr;
static HBRUSH g_statusBrush = nullptr;

static HHOOK g_keyboardHook = nullptr;
static bool g_capturingHotkey = false;

static UINT g_hotkeyVK = VK_F6;
static UINT g_hotkeyMods = 0;

static constexpr int HOTKEY_ID = 1;
static constexpr int BUTTON_START = 1001;
static constexpr int BUTTON_CHANGE_HOTKEY = 1002;

static const wchar_t* APP_NAME = L"Protocol+ Auto Clicker";

// Protocol+ blue theme
static constexpr COLORREF BLUE = RGB(30, 110, 220);
static constexpr COLORREF DARK_BLUE = RGB(18, 68, 135);
static constexpr COLORREF LIGHT_BLUE = RGB(232, 243, 255);
static constexpr COLORREF STATUS_BLUE = RGB(205, 227, 255);
static constexpr COLORREF WHITE = RGB(255, 255, 255);
static constexpr COLORREF TEXT = RGB(25, 45, 70);


// ============================================================
// Clicking
// ============================================================

static void ClickLoop(double cps, DWORD downFlag, DWORD upFlag)
{
    if (cps < 1.0)
        cps = 1.0;

    if (cps > 1000.0)
        cps = 1000.0;

    const double interval = 1.0 / cps;

    auto nextClick = std::chrono::steady_clock::now();

    while (!g_stop.load())
    {
        INPUT inputs[2]{};

        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = downFlag;

        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = upFlag;

        SendInput(2, inputs, sizeof(INPUT));

        nextClick += std::chrono::duration_cast<
            std::chrono::steady_clock::duration
        >(
            std::chrono::duration<double>(interval)
        );

        auto now = std::chrono::steady_clock::now();

        if (nextClick > now)
        {
            std::this_thread::sleep_for(nextClick - now);
        }
        else
        {
            nextClick = now;
        }
    }
}


// ============================================================
// CPS
// ============================================================

static double GetCPS()
{
    wchar_t buffer[32]{};

    GetWindowTextW(
        g_cpsEdit,
        buffer,
        static_cast<int>(std::size(buffer))
    );

    double cps = _wtof(buffer);

    if (cps < 1.0 || cps > 1000.0)
        return 0.0;

    return cps;
}


// ============================================================
// Status
// ============================================================

static void SetStatus(const std::wstring& text)
{
    if (!g_status)
        return;

    SetWindowTextW(g_status, text.c_str());

    InvalidateRect(
        g_status,
        nullptr,
        TRUE
    );
}


// ============================================================
// Stop
// ============================================================

static void StopClicker()
{
    if (!g_running.load())
        return;

    g_stop.store(true);
    g_running.store(false);

    SetStatus(L"Stopped");

    if (g_toggle)
        SetWindowTextW(g_toggle, L"Start");
}


// ============================================================
// Start / Stop
// ============================================================

static void ToggleClicker()
{
    if (g_running.load())
    {
        StopClicker();
        return;
    }

    double cps = GetCPS();

    if (cps <= 0.0)
    {
        MessageBoxW(
            g_mainWindow,
            L"Enter a CPS value from 1 to 1000.",
            APP_NAME,
            MB_ICONWARNING
        );

        return;
    }

    int button = static_cast<int>(
        SendMessageW(
            g_buttonCombo,
            CB_GETCURSEL,
            0,
            0
        )
    );

    DWORD downFlag = MOUSEEVENTF_LEFTDOWN;
    DWORD upFlag = MOUSEEVENTF_LEFTUP;

    if (button == 1)
    {
        downFlag = MOUSEEVENTF_RIGHTDOWN;
        upFlag = MOUSEEVENTF_RIGHTUP;
    }
    else if (button == 2)
    {
        downFlag = MOUSEEVENTF_MIDDLEDOWN;
        upFlag = MOUSEEVENTF_MIDDLEUP;
    }

    g_stop.store(false);
    g_running.store(true);

    wchar_t status[64]{};

    swprintf_s(
        status,
        L"Running - %.0f CPS",
        cps
    );

    SetStatus(status);

    SetWindowTextW(
        g_toggle,
        L"Stop"
    );

    std::thread(
        ClickLoop,
        cps,
        downFlag,
        upFlag
    ).detach();
}


// ============================================================
// Hotkey
// ============================================================

static bool IsModifierVK(UINT vk)
{
    return
        vk == VK_SHIFT ||
        vk == VK_LSHIFT ||
        vk == VK_RSHIFT ||
        vk == VK_CONTROL ||
        vk == VK_LCONTROL ||
        vk == VK_RCONTROL ||
        vk == VK_MENU ||
        vk == VK_LMENU ||
        vk == VK_RMENU ||
        vk == VK_LWIN ||
        vk == VK_RWIN;
}


static UINT CurrentModifierMask()
{
    UINT mods = 0;

    if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
        mods |= MOD_CONTROL;

    if (GetAsyncKeyState(VK_MENU) & 0x8000)
        mods |= MOD_ALT;

    if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
        mods |= MOD_SHIFT;

    if ((GetAsyncKeyState(VK_LWIN) & 0x8000) ||
        (GetAsyncKeyState(VK_RWIN) & 0x8000))
    {
        mods |= MOD_WIN;
    }

    return mods;
}


// ============================================================
// Hotkey name
// ============================================================

static std::wstring KeyName(UINT vk, UINT mods)
{
    std::wstring result;

    if (mods & MOD_CONTROL)
        result += L"Ctrl+";

    if (mods & MOD_ALT)
        result += L"Alt+";

    if (mods & MOD_SHIFT)
        result += L"Shift+";

    if (mods & MOD_WIN)
        result += L"Win+";

    UINT scanCode = MapVirtualKeyW(
        vk,
        MAPVK_VK_TO_VSC
    );

    LONG keyInfo = static_cast<LONG>(
        scanCode << 16
    );

    if (vk == VK_LEFT ||
        vk == VK_UP ||
        vk == VK_RIGHT ||
        vk == VK_DOWN ||
        vk == VK_PRIOR ||
        vk == VK_NEXT ||
        vk == VK_END ||
        vk == VK_HOME ||
        vk == VK_INSERT ||
        vk == VK_DELETE)
    {
        keyInfo |= (1 << 24);
    }

    wchar_t name[64]{};

    if (GetKeyNameTextW(
        keyInfo,
        name,
        static_cast<int>(std::size(name))
    ) > 0)
    {
        result += name;
    }
    else
    {
        result += L"Key ";
        result += std::to_wstring(vk);
    }

    return result;
}


// ============================================================
// Register current hotkey
//
// IMPORTANT:
// The hotkey is registered against g_mainWindow rather than
// nullptr. This ensures WM_HOTKEY is delivered to our window.
// ============================================================

static bool RegisterCurrentHotkey()
{
    if (!g_mainWindow)
        return false;

    return RegisterHotKey(
        g_mainWindow,
        HOTKEY_ID,
        g_hotkeyMods,
        g_hotkeyVK
    ) != FALSE;
}


static void UpdateHotkeyLabel()
{
    if (!g_hotkeyText)
        return;

    SetWindowTextW(
        g_hotkeyText,
        KeyName(
            g_hotkeyVK,
            g_hotkeyMods
        ).c_str()
    );
}


// ============================================================
// End hotkey capture
// ============================================================

static void EndHotkeyCapture()
{
    if (g_keyboardHook)
    {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = nullptr;
    }

    g_capturingHotkey = false;

    if (g_changeHotkey)
        SetWindowTextW(
            g_changeHotkey,
            L"Change"
        );

    if (g_running.load())
        SetStatus(L"Running");
    else
        SetStatus(L"Stopped");
}


// ============================================================
// Low-level keyboard hook
// ============================================================

static LRESULT CALLBACK LowLevelKeyboardProc(
    int code,
    WPARAM wParam,
    LPARAM lParam
)
{
    if (code == HC_ACTION &&
        g_capturingHotkey &&
        (wParam == WM_KEYDOWN ||
         wParam == WM_SYSKEYDOWN))
    {
        const KBDLLHOOKSTRUCT* key =
            reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);

        UINT vk = key->vkCode;

        // Modifier keys are collected but don't finish capture.
        if (!IsModifierVK(vk))
        {
            UINT mods = CurrentModifierMask();

            UINT oldVK = g_hotkeyVK;
            UINT oldMods = g_hotkeyMods;

            // Temporarily remove the old hotkey.
            UnregisterHotKey(
                g_mainWindow,
                HOTKEY_ID
            );

            g_hotkeyVK = vk;
            g_hotkeyMods = mods;

            // Try registering the new combination.
            if (!RegisterCurrentHotkey())
            {
                // Restore old hotkey.
                g_hotkeyVK = oldVK;
                g_hotkeyMods = oldMods;

                RegisterCurrentHotkey();

                MessageBoxW(
                    g_mainWindow,
                    L"That key combination is already in use by Windows or another application.\n\nPlease choose a different key.",
                    APP_NAME,
                    MB_ICONWARNING
                );
            }
            else
            {
                UpdateHotkeyLabel();
            }

            EndHotkeyCapture();

            // Prevent the captured key from being passed through.
            return 1;
        }
    }

    return CallNextHookEx(
        g_keyboardHook,
        code,
        wParam,
        lParam
    );
}


// ============================================================
// Begin custom hotkey capture
// ============================================================

static void BeginHotkeyCapture()
{
    // Stop clicking while choosing a new hotkey.
    if (g_running.load())
        StopClicker();

    // Remove the currently registered hotkey.
    if (g_mainWindow)
    {
        UnregisterHotKey(
            g_mainWindow,
            HOTKEY_ID
        );
    }

    g_capturingHotkey = true;

    SetWindowTextW(
        g_changeHotkey,
        L"Press a key..."
    );

    SetStatus(
        L"Press any key to bind the toggle hotkey"
    );

    g_keyboardHook = SetWindowsHookExW(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        GetModuleHandleW(nullptr),
        0
    );

    if (!g_keyboardHook)
    {
        g_capturingHotkey = false;

        RegisterCurrentHotkey();

        SetWindowTextW(
            g_changeHotkey,
            L"Change"
        );

        SetStatus(L"Stopped");

        MessageBoxW(
            g_mainWindow,
            L"Windows could not start hotkey capture.\n\nPlease try again.",
            APP_NAME,
            MB_ICONERROR
        );
    }
}


// ============================================================
// Font
// ============================================================

static void ApplyFont(
    HWND control,
    HFONT font
)
{
    SendMessageW(
        control,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(font),
        TRUE
    );
}


// ============================================================
// Blue Start / Stop button
// ============================================================

static void PaintButton(
    const DRAWITEMSTRUCT* dis
)
{
    HDC dc = dis->hDC;
    RECT rc = dis->rcItem;

    bool pressed =
        (dis->itemState & ODS_SELECTED) != 0;

    bool disabled =
        (dis->itemState & ODS_DISABLED) != 0;

    COLORREF buttonColor;

    if (disabled)
    {
        buttonColor = RGB(160, 175, 195);
    }
    else if (pressed)
    {
        buttonColor = DARK_BLUE;
    }
    else
    {
        buttonColor = BLUE;
    }

    HBRUSH brush =
        CreateSolidBrush(buttonColor);

    FillRect(
        dc,
        &rc,
        brush
    );

    DeleteObject(brush);

    HBRUSH frame =
        CreateSolidBrush(DARK_BLUE);

    FrameRect(
        dc,
        &rc,
        frame
    );

    DeleteObject(frame);

    SetBkMode(
        dc,
        TRANSPARENT
    );

    SetTextColor(
        dc,
        WHITE
    );

    wchar_t text[64]{};

    GetWindowTextW(
        dis->hwndItem,
        text,
        static_cast<int>(std::size(text))
    );

    DrawTextW(
        dc,
        text,
        -1,
        &rc,
        DT_CENTER |
        DT_VCENTER |
        DT_SINGLELINE
    );
}


// ============================================================
// Main window procedure
// ============================================================

LRESULT CALLBACK WindowProcedure(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (message)
    {
        case WM_CREATE:
        {
            g_mainWindow = hwnd;

            g_bgBrush =
                CreateSolidBrush(
                    LIGHT_BLUE
                );

            g_statusBrush =
                CreateSolidBrush(
                    STATUS_BLUE
                );

            HFONT font =
                reinterpret_cast<HFONT>(
                    GetStockObject(DEFAULT_GUI_FONT)
                );


            // Title
            HWND title = CreateWindowW(
                L"STATIC",
                L"Protocol+ Auto Clicker",
                WS_CHILD | WS_VISIBLE,
                24,
                18,
                360,
                32,
                hwnd,
                nullptr,
                nullptr,
                nullptr
            );

            ApplyFont(title, font);


            // Subtitle
            HWND subtitle = CreateWindowW(
                L"STATIC",
                L"Fast, simple Windows auto-clicker",
                WS_CHILD | WS_VISIBLE,
                24,
                45,
                360,
                22,
                hwnd,
                nullptr,
                nullptr,
                nullptr
            );

            ApplyFont(subtitle, font);


            // CPS label
            HWND cpsLabel = CreateWindowW(
                L"STATIC",
                L"Clicks per second (CPS):",
                WS_CHILD | WS_VISIBLE,
                24,
                82,
                220,
                24,
                hwnd,
                nullptr,
                nullptr,
                nullptr
            );

            ApplyFont(cpsLabel, font);


            // CPS input
            g_cpsEdit = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"10",
                WS_CHILD |
                WS_VISIBLE |
                ES_NUMBER,
                270,
                78,
                100,
                28,
                hwnd,
                nullptr,
                nullptr,
                nullptr
            );

            ApplyFont(
                g_cpsEdit,
                font
            );


            // Mouse button label
            HWND mouseLabel = CreateWindowW(
                L"STATIC",
                L"Mouse button:",
                WS_CHILD | WS_VISIBLE,
                24,
                122,
                220,
                24,
                hwnd,
                nullptr,
                nullptr,
                nullptr
            );

            ApplyFont(
                mouseLabel,
                font
            );


            // Mouse button selector
            g_buttonCombo = CreateWindowW(
                L"COMBOBOX",
                L"",
                WS_CHILD |
                WS_VISIBLE |
                CBS_DROPDOWNLIST,
                270,
                118,
                100,
                120,
                hwnd,
                nullptr,
                nullptr,
                nullptr
            );

            ApplyFont(
                g_buttonCombo,
                font
            );

            const wchar_t* buttons[] =
            {
                L"Left",
                L"Right",
                L"Middle"
            };

            for (const wchar_t* item : buttons)
            {
                SendMessageW(
                    g_buttonCombo,
                    CB_ADDSTRING,
                    0,
                    reinterpret_cast<LPARAM>(item)
                );
            }

            SendMessageW(
                g_buttonCombo,
                CB_SETCURSEL,
                0,
                0
            );


            // Hotkey label
            HWND hotkeyLabel = CreateWindowW(
                L"STATIC",
                L"Toggle hotkey:",
                WS_CHILD | WS_VISIBLE,
                24,
                162,
                220,
                24,
                hwnd,
                nullptr,
                nullptr,
                nullptr
            );

            ApplyFont(
                hotkeyLabel,
                font
            );


            // Hotkey display
            g_hotkeyText = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"STATIC",
                L"F6",
                WS_CHILD |
                WS_VISIBLE |
                SS_CENTER,
                270,
                158,
                100,
                28,
                hwnd,
                nullptr,
                nullptr,
                nullptr
            );

            ApplyFont(
                g_hotkeyText,
                font
            );


            // Change hotkey button
            g_changeHotkey = CreateWindowW(
                L"BUTTON",
                L"Change",
                WS_CHILD |
                WS_VISIBLE |
                BS_PUSHBUTTON,
                270,
                190,
                100,
                28,
                hwnd,
                reinterpret_cast<HMENU>(
                    BUTTON_CHANGE_HOTKEY
                ),
                nullptr,
                nullptr
            );

            ApplyFont(
                g_changeHotkey,
                font
            );


            // Status
            g_status = CreateWindowW(
                L"STATIC",
                L"Stopped",
                WS_CHILD |
                WS_VISIBLE |
                SS_CENTER,
                24,
                232,
                346,
                30,
                hwnd,
                nullptr,
                nullptr,
                nullptr
            );

            ApplyFont(
                g_status,
                font
            );


            // Start/Stop button
            g_toggle = CreateWindowW(
                L"BUTTON",
                L"Start",
                WS_CHILD |
                WS_VISIBLE |
                BS_OWNERDRAW,
                24,
                272,
                346,
                44,
                hwnd,
                reinterpret_cast<HMENU>(
                    BUTTON_START
                ),
                nullptr,
                nullptr
            );

            ApplyFont(
                g_toggle,
                font
            );


            // Help text
            HWND help = CreateWindowW(
                L"STATIC",
                L"Default: F6  -  Click Change to bind your own key.",
                WS_CHILD |
                WS_VISIBLE |
                SS_CENTER,
                24,
                328,
                346,
                26,
                hwnd,
                nullptr,
                nullptr,
                nullptr
            );

            ApplyFont(
                help,
                font
            );


            // =================================================
            // IMPORTANT FIX:
            // Register F6 against THIS WINDOW.
            // =================================================

            g_hotkeyVK = VK_F6;
            g_hotkeyMods = 0;

            if (!RegisterCurrentHotkey())
            {
                MessageBoxW(
                    hwnd,
                    L"F6 could not be registered because another application is already using it.\n\nUse Change to select another key.",
                    APP_NAME,
                    MB_ICONWARNING
                );
            }

            UpdateHotkeyLabel();

            return 0;
        }


        // ====================================================
        // Buttons
        // ====================================================

        case WM_COMMAND:
        {
            int controlID =
                LOWORD(wParam);

            int notification =
                HIWORD(wParam);


            if (controlID == BUTTON_START &&
                notification == BN_CLICKED)
            {
                ToggleClicker();

                return 0;
            }


            if (controlID == BUTTON_CHANGE_HOTKEY &&
                notification == BN_CLICKED)
            {
                if (g_capturingHotkey)
                {
                    EndHotkeyCapture();
                }
                else
                {
                    BeginHotkeyCapture();
                }

                return 0;
            }

            break;
        }


        // ====================================================
        // Draw Start/Stop button
        // ====================================================

        case WM_DRAWITEM:
        {
            if (wParam == BUTTON_START)
            {
                PaintButton(
                    reinterpret_cast<
                        const DRAWITEMSTRUCT*
                    >(lParam)
                );

                return TRUE;
            }

            break;
        }


        // ====================================================
        // Static text colors
        // ====================================================

        case WM_CTLCOLORSTATIC:
        {
            HDC dc =
                reinterpret_cast<HDC>(wParam);

            HWND control =
                reinterpret_cast<HWND>(lParam);

            SetBkMode(
                dc,
                TRANSPARENT
            );

            if (control == g_status)
            {
                SetTextColor(
                    dc,
                    DARK_BLUE
                );

                return reinterpret_cast<LRESULT>(
                    g_statusBrush
                );
            }

            SetTextColor(
                dc,
                TEXT
            );

            return reinterpret_cast<LRESULT>(
                g_bgBrush
            );
        }


        // ====================================================
        // Edit box
        // ====================================================

        case WM_CTLCOLOREDIT:
        {
            HDC dc =
                reinterpret_cast<HDC>(wParam);

            SetTextColor(
                dc,
                TEXT
            );

            SetBkColor(
                dc,
                WHITE
            );

            return reinterpret_cast<LRESULT>(
                GetStockObject(WHITE_BRUSH)
            );
        }


        // ====================================================
        // Combo box
        // ====================================================

        case WM_CTLCOLORLISTBOX:
        {
            HDC dc =
                reinterpret_cast<HDC>(wParam);

            SetTextColor(
                dc,
                TEXT
            );

            SetBkColor(
                dc,
                WHITE
            );

            return reinterpret_cast<LRESULT>(
                GetStockObject(WHITE_BRUSH)
            );
        }


        // ====================================================
        // GLOBAL HOTKEY
        //
        // This now works because RegisterHotKey() used
        // g_mainWindow instead of nullptr.
        // ====================================================

        case WM_HOTKEY:
        {
            if (wParam == HOTKEY_ID &&
                !g_capturingHotkey)
            {
                ToggleClicker();
            }

            return 0;
        }


        // ====================================================
        // Background
        // ====================================================

        case WM_ERASEBKGND:
        {
            RECT rc;

            GetClientRect(
                hwnd,
                &rc
            );

            FillRect(
                reinterpret_cast<HDC>(wParam),
                &rc,
                g_bgBrush
            );

            return 1;
        }


        // ====================================================
        // Closing
        // ====================================================

        case WM_DESTROY:
        {
            if (g_keyboardHook)
            {
                UnhookWindowsHookEx(
                    g_keyboardHook
                );

                g_keyboardHook = nullptr;
            }

            if (g_mainWindow)
            {
                UnregisterHotKey(
                    g_mainWindow,
                    HOTKEY_ID
                );
            }

            g_stop.store(true);
            g_running.store(false);

            if (g_bgBrush)
            {
                DeleteObject(
                    g_bgBrush
                );

                g_bgBrush = nullptr;
            }

            if (g_statusBrush)
            {
                DeleteObject(
                    g_statusBrush
                );

                g_statusBrush = nullptr;
            }

            PostQuitMessage(0);

            return 0;
        }
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam
    );
}


// ============================================================
// Application entry point
// ============================================================

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int showCommand
)
{
    WNDCLASSW wc{};

    wc.lpfnWndProc =
        WindowProcedure;

    wc.hInstance =
        instance;

    wc.lpszClassName =
        L"ProtocolPlusAutoClicker";

    wc.hCursor =
        LoadCursorW(
            nullptr,
            IDC_ARROW
        );

    wc.hIcon =
        LoadIconW(
            instance,
            MAKEINTRESOURCEW(101)
        );

    wc.hbrBackground =
        reinterpret_cast<HBRUSH>(
            COLOR_WINDOW + 1
        );


    if (!RegisterClassW(&wc))
        return 1;


    HWND window = CreateWindowW(
        wc.lpszClassName,
        APP_NAME,
        WS_OVERLAPPED |
        WS_CAPTION |
        WS_SYSMENU |
        WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        410,
        400,
        nullptr,
        nullptr,
        instance,
        nullptr
    );


    if (!window)
        return 1;


    ShowWindow(
        window,
        showCommand
    );

    UpdateWindow(window);


    MSG msg{};

    while (
        GetMessageW(
            &msg,
            nullptr,
            0,
            0
        ) > 0
    )
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }


    return 0;
}

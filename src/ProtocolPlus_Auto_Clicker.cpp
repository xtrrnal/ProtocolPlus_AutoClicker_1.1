#define UNICODE
#define _UNICODE
#include <windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cwchar>

static HWND g_cpsEdit=nullptr,g_buttonCombo=nullptr,g_hotkeyText=nullptr,g_status=nullptr,g_toggle=nullptr,g_changeHotkey=nullptr;
static std::atomic<bool> g_running(false),g_stop(false);
static HBRUSH g_bgBrush=nullptr,g_statusBrush=nullptr;
static HHOOK g_keyboardHook=nullptr;
static bool g_capturingHotkey=false;
static UINT g_hotkeyVK=VK_F6,g_hotkeyMods=0;
static const wchar_t* APP_NAME=L"Protocol+ Auto Clicker";
static constexpr COLORREF BLUE=RGB(30,110,220),DARK_BLUE=RGB(18,68,135),LIGHT_BLUE=RGB(232,243,255),WHITE=RGB(255,255,255),TEXT=RGB(25,45,70);

static void ClickLoop(double cps,DWORD downFlag,DWORD upFlag){
    if(cps<1.0)cps=1.0;if(cps>1000.0)cps=1000.0;
    const double interval=1.0/cps;auto nextClick=std::chrono::steady_clock::now();
    while(!g_stop.load()){
        INPUT inputs[2]{};inputs[0].type=INPUT_MOUSE;inputs[0].mi.dwFlags=downFlag;inputs[1].type=INPUT_MOUSE;inputs[1].mi.dwFlags=upFlag;
        SendInput(2,inputs,sizeof(INPUT));
        nextClick+=std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(interval));
        auto now=std::chrono::steady_clock::now();if(nextClick>now)std::this_thread::sleep_for(nextClick-now);else nextClick=now;
    }
}
static double GetCPS(){wchar_t b[32]{};GetWindowTextW(g_cpsEdit,b,31);double cps=_wtof(b);return(cps<1.0||cps>1000.0)?0.0:cps;}
static void SetStatus(const std::wstring& text){SetWindowTextW(g_status,text.c_str());InvalidateRect(g_status,nullptr,TRUE);}
static void StopClicker(){if(!g_running.load())return;g_stop=true;g_running=false;SetStatus(L"Stopped");SetWindowTextW(g_toggle,L"Start");}
static void ToggleClicker(){
    if(g_running.load()){StopClicker();return;}
    double cps=GetCPS();if(cps<=0.0){MessageBoxW(nullptr,L"Enter a CPS value from 1 to 1000.",APP_NAME,MB_ICONWARNING);return;}
    int button=(int)SendMessageW(g_buttonCombo,CB_GETCURSEL,0,0);DWORD downFlag=MOUSEEVENTF_LEFTDOWN,upFlag=MOUSEEVENTF_LEFTUP;
    if(button==1){downFlag=MOUSEEVENTF_RIGHTDOWN;upFlag=MOUSEEVENTF_RIGHTUP;}else if(button==2){downFlag=MOUSEEVENTF_MIDDLEDOWN;upFlag=MOUSEEVENTF_MIDDLEUP;}
    g_stop=false;g_running=true;wchar_t status[64]{};swprintf_s(status,L"Running — %.0f CPS",cps);SetStatus(status);SetWindowTextW(g_toggle,L"Stop");std::thread(ClickLoop,cps,downFlag,upFlag).detach();
}
static bool IsModifierVK(UINT vk){return vk==VK_SHIFT||vk==VK_LSHIFT||vk==VK_RSHIFT||vk==VK_CONTROL||vk==VK_LCONTROL||vk==VK_RCONTROL||vk==VK_MENU||vk==VK_LMENU||vk==VK_RMENU||vk==VK_LWIN||vk==VK_RWIN;}
static UINT CurrentModifierMask(){UINT mods=0;if(GetAsyncKeyState(VK_CONTROL)&0x8000)mods|=MOD_CONTROL;if(GetAsyncKeyState(VK_MENU)&0x8000)mods|=MOD_ALT;if(GetAsyncKeyState(VK_SHIFT)&0x8000)mods|=MOD_SHIFT;if((GetAsyncKeyState(VK_LWIN)&0x8000)||(GetAsyncKeyState(VK_RWIN)&0x8000))mods|=MOD_WIN;return mods;}
static std::wstring KeyName(UINT vk,UINT mods){
    std::wstring r;if(mods&MOD_CONTROL)r+=L"Ctrl+";if(mods&MOD_ALT)r+=L"Alt+";if(mods&MOD_SHIFT)r+=L"Shift+";if(mods&MOD_WIN)r+=L"Win+";
    UINT scan=MapVirtualKeyW(vk,MAPVK_VK_TO_VSC);LONG lp=(LONG)(scan<<16);
    if(vk==VK_LEFT||vk==VK_UP||vk==VK_RIGHT||vk==VK_DOWN||vk==VK_PRIOR||vk==VK_NEXT||vk==VK_END||vk==VK_HOME||vk==VK_INSERT||vk==VK_DELETE)lp|=(1<<24);
    wchar_t name[64]{};if(GetKeyNameTextW(lp,name,64)>0)r+=name;else r+=L"Key "+std::to_wstring(vk);return r;
}
static bool RegisterCurrentHotkey(){return RegisterHotKey(nullptr,1,g_hotkeyMods,g_hotkeyVK)!=FALSE;}
static void UpdateHotkeyLabel(){SetWindowTextW(g_hotkeyText,KeyName(g_hotkeyVK,g_hotkeyMods).c_str());}
static void EndHotkeyCapture(){if(g_keyboardHook){UnhookWindowsHookEx(g_keyboardHook);g_keyboardHook=nullptr;}g_capturingHotkey=false;SetWindowTextW(g_changeHotkey,L"Change");SetStatus(g_running.load()?L"Running":L"Stopped");}
static LRESULT CALLBACK LowLevelKeyboardProc(int code,WPARAM wParam,LPARAM lParam){
    if(code==HC_ACTION&&g_capturingHotkey&&(wParam==WM_KEYDOWN||wParam==WM_SYSKEYDOWN)){
        const KBDLLHOOKSTRUCT* key=(const KBDLLHOOKSTRUCT*)lParam;UINT vk=key->vkCode;
        if(!IsModifierVK(vk)){
            UINT mods=CurrentModifierMask(),oldVK=g_hotkeyVK,oldMods=g_hotkeyMods;g_hotkeyVK=vk;g_hotkeyMods=mods;
            if(!RegisterCurrentHotkey()){
                g_hotkeyVK=oldVK;g_hotkeyMods=oldMods;RegisterCurrentHotkey();MessageBoxW(nullptr,L"That key combination is already in use by Windows or another application. Choose a different key.",APP_NAME,MB_ICONWARNING);
            }else UpdateHotkeyLabel();
            EndHotkeyCapture();return 1;
        }
    }return CallNextHookEx(g_keyboardHook,code,wParam,lParam);
}
static void BeginHotkeyCapture(){
    if(g_running.load())StopClicker();UnregisterHotKey(nullptr,1);g_capturingHotkey=true;SetWindowTextW(g_changeHotkey,L"Press a key...");SetStatus(L"Press any key to bind the toggle hotkey");
    g_keyboardHook=SetWindowsHookExW(WH_KEYBOARD_LL,LowLevelKeyboardProc,GetModuleHandleW(nullptr),0);
    if(!g_keyboardHook){g_capturingHotkey=false;RegisterCurrentHotkey();SetWindowTextW(g_changeHotkey,L"Change");MessageBoxW(nullptr,L"Windows could not start hotkey capture. Please try again.",APP_NAME,MB_ICONERROR);}
}
static void ApplyFont(HWND control,HFONT font){SendMessageW(control,WM_SETFONT,(WPARAM)font,TRUE);}
static void PaintButton(const DRAWITEMSTRUCT* dis){HDC dc=dis->hDC;RECT rc=dis->rcItem;bool pressed=(dis->itemState&ODS_SELECTED)!=0,disabled=(dis->itemState&ODS_DISABLED)!=0;HBRUSH brush=CreateSolidBrush(disabled?RGB(160,175,195):(pressed?DARK_BLUE:BLUE));FillRect(dc,&rc,brush);DeleteObject(brush);HBRUSH frame=CreateSolidBrush(DARK_BLUE);FrameRect(dc,&rc,frame);DeleteObject(frame);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,WHITE);wchar_t text[64]{};GetWindowTextW(dis->hwndItem,text,63);DrawTextW(dc,text,-1,&rc,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}

LRESULT CALLBACK WindowProcedure(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam){
    switch(message){
    case WM_CREATE:{
        g_bgBrush=CreateSolidBrush(LIGHT_BLUE);g_statusBrush=CreateSolidBrush(RGB(205,227,255));HFONT font=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HWND title=CreateWindowW(L"STATIC",L"Protocol+ Auto Clicker",WS_CHILD|WS_VISIBLE,24,18,360,32,hwnd,nullptr,nullptr,nullptr);ApplyFont(title,font);
        HWND subtitle=CreateWindowW(L"STATIC",L"Fast, simple Windows auto-clicker",WS_CHILD|WS_VISIBLE,24,45,360,22,hwnd,nullptr,nullptr,nullptr);ApplyFont(subtitle,font);
        HWND cpsLabel=CreateWindowW(L"STATIC",L"Clicks per second (CPS):",WS_CHILD|WS_VISIBLE,24,82,220,24,hwnd,nullptr,nullptr,nullptr);ApplyFont(cpsLabel,font);
        g_cpsEdit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"10",WS_CHILD|WS_VISIBLE|ES_NUMBER,270,78,100,28,hwnd,nullptr,nullptr,nullptr);ApplyFont(g_cpsEdit,font);
        HWND mouseLabel=CreateWindowW(L"STATIC",L"Mouse button:",WS_CHILD|WS_VISIBLE,24,122,220,24,hwnd,nullptr,nullptr,nullptr);ApplyFont(mouseLabel,font);
        g_buttonCombo=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,270,118,100,120,hwnd,nullptr,nullptr,nullptr);ApplyFont(g_buttonCombo,font);const wchar_t* buttons[]={L"Left",L"Right",L"Middle"};for(const wchar_t* item:buttons)SendMessageW(g_buttonCombo,CB_ADDSTRING,0,(LPARAM)item);SendMessageW(g_buttonCombo,CB_SETCURSEL,0,0);
        HWND hotkeyLabel=CreateWindowW(L"STATIC",L"Toggle hotkey:",WS_CHILD|WS_VISIBLE,24,162,220,24,hwnd,nullptr,nullptr,nullptr);ApplyFont(hotkeyLabel,font);
        g_hotkeyText=CreateWindowExW(WS_EX_CLIENTEDGE,L"STATIC",L"F6",WS_CHILD|WS_VISIBLE|SS_CENTER,270,158,100,28,hwnd,nullptr,nullptr,nullptr);ApplyFont(g_hotkeyText,font);
        g_changeHotkey=CreateWindowW(L"BUTTON",L"Change",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,270,190,100,28,hwnd,(HMENU)1002,nullptr,nullptr);ApplyFont(g_changeHotkey,font);
        g_status=CreateWindowW(L"STATIC",L"Stopped",WS_CHILD|WS_VISIBLE|SS_CENTER,24,232,346,30,hwnd,nullptr,nullptr,nullptr);ApplyFont(g_status,font);
        g_toggle=CreateWindowW(L"BUTTON",L"Start",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,24,272,346,44,hwnd,(HMENU)1001,nullptr,nullptr);ApplyFont(g_toggle,font);
        HWND help=CreateWindowW(L"STATIC",L"Default: F6  •  Click Change to bind any keyboard key.",WS_CHILD|WS_VISIBLE|SS_CENTER,24,328,346,26,hwnd,nullptr,nullptr,nullptr);ApplyFont(help,font);
        RegisterHotKey(nullptr,1,0,VK_F6);UpdateHotkeyLabel();return 0;}
    case WM_COMMAND:
        if(LOWORD(wParam)==1001&&HIWORD(wParam)==BN_CLICKED){ToggleClicker();return 0;}
        if(LOWORD(wParam)==1002&&HIWORD(wParam)==BN_CLICKED){if(g_capturingHotkey)EndHotkeyCapture();else BeginHotkeyCapture();return 0;}return 0;
    case WM_DRAWITEM:if(wParam==1001){PaintButton((const DRAWITEMSTRUCT*)lParam);return TRUE;}break;
    case WM_CTLCOLORSTATIC:{HDC dc=(HDC)wParam;HWND control=(HWND)lParam;SetBkMode(dc,TRANSPARENT);if(control==g_status){SetTextColor(dc,DARK_BLUE);return(LRESULT)g_statusBrush;}SetTextColor(dc,TEXT);return(LRESULT)g_bgBrush;}
    case WM_CTLCOLOREDIT:{HDC dc=(HDC)wParam;SetTextColor(dc,TEXT);SetBkColor(dc,WHITE);return(LRESULT)GetStockObject(WHITE_BRUSH);}
    case WM_CTLCOLORLISTBOX:{HDC dc=(HDC)wParam;SetTextColor(dc,TEXT);SetBkColor(dc,WHITE);return(LRESULT)GetStockObject(WHITE_BRUSH);}
    case WM_HOTKEY:if(wParam==1&&!g_capturingHotkey)ToggleClicker();return 0;
    case WM_ERASEBKGND:{RECT rc;GetClientRect(hwnd,&rc);FillRect((HDC)wParam,&rc,g_bgBrush);return 1;}
    case WM_DESTROY:if(g_keyboardHook)UnhookWindowsHookEx(g_keyboardHook);UnregisterHotKey(nullptr,1);g_stop=true;if(g_bgBrush)DeleteObject(g_bgBrush);if(g_statusBrush)DeleteObject(g_statusBrush);PostQuitMessage(0);return 0;
    }return DefWindowProcW(hwnd,message,wParam,lParam);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int showCommand){
    WNDCLASSW wc{};wc.lpfnWndProc=WindowProcedure;wc.hInstance=instance;wc.lpszClassName=L"ProtocolPlusAutoClicker";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101));wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassW(&wc);
    HWND window=CreateWindowW(wc.lpszClassName,APP_NAME,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,410,400,nullptr,nullptr,instance,nullptr);if(!window)return 1;ShowWindow(window,showCommand);UpdateWindow(window);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return 0;
}

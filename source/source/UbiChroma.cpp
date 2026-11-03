#include "../headers/pch.h"
#include "../headers/App.h"
#include "../headers/SimpleCapture.h"
#include "../headers/Helper.h"
#include "../headers/InputHelper.h"
#include "../headers/IniParser.h"
#include "../headers/Win32WindowEnumeration.h"
#include "../headers/HotKeyManager.h"

//Variable declaration
auto g_app = std::make_shared<App>();
HWND g_hwnd = nullptr;
auto g_windows = EnumerateWindows();
std::string g_userID = "";

INT g_index = INVALID_SELECTION;

NOTIFYICONDATA g_niData = {};
INT g_appliedFilter = ID_FILTER_DEFAULT;
INT g_appliedMode = ID_NONTRANSPARENT_ITEM;
bool m_Show = true;
bool g_isToolWindowMax = false;
bool g_isGameWindowMax = false;
bool g_isGameWindowMin = false;

                                                                    WINDOWPLACEMENT g_targetPlacement{};
bool g_targetMinimizedByToggle = false;
RECT g_rectBeforeMoveSize{};

std::map<int,std::string> globalList;

std::string g_screenshotPath = "";
std::string g_iniFilePath = "";

HWINEVENTHOOK g_hwEventHook = {};
bool static capturingStart = false;
bool static isAboutShowing = false;
bool static isOptionsShowing = false;
bool static isBrowseShowing = false;
HICON projectIcon;
HICON handIcon;

void StartCaptureWithWindow(const HWND& hwnd, const LPCSTR& windowName, const HWND& desthwnd, int& appliedFilter, const int& currentFilter)
{    
    if (IsIconic(desthwnd))
    {        
        MSGBOXPARAMSA msgbox = { 0 };
        CustomMessageBox(g_hwnd, msgbox, "Captured application is minimized !! .\n\nPlease maximize the captured application window to continue.");
        MessageBoxIndirectA(&msgbox);
        return;
    }
    SetWindowTextA(hwnd, windowName);
    appliedFilter = currentFilter;    
    g_app->StartCapture(desthwnd, hwnd, appliedFilter);
    capturingStart = true;      
}

void ShowSourceWindowMenu(HWND hwnd)
{
    HMENU popupMenu = CreatePopupMenu();

    WINRT_VERIFY(popupMenu);
    g_windows = EnumerateWindows();

    for (int index = 0; index < (int)g_windows.size(); index++)
    {
        if (wcscmp(g_windows.at(index).ClassName().c_str(), L"UbiChroma") != 0)
        {
            AppendMenu(popupMenu, MF_STRING, (int)(index + MENU_OFFSET), g_windows.at(index).Title().c_str());
        }
    }

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(popupMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(popupMenu);
}

void ToggleShowHide(HWND hwnd)
{
    if (m_Show)
    {
        g_targetMinimizedByToggle = false;

        if (g_index != INVALID_SELECTION && g_index < (INT)g_windows.size())
        {
            HWND target = g_windows.at(g_index).Hwnd();
            if (target && !IsIconic(target))
            {
                g_targetPlacement.length = sizeof(WINDOWPLACEMENT);
                GetWindowPlacement(target, &g_targetPlacement);
                g_targetMinimizedByToggle = true;
                ShowWindow(target, SW_MINIMIZE);
            }
        }

        ShowWindow(hwnd, SW_HIDE);
        m_Show = false;
    }
    else
    {
        m_Show = true;

        if (g_targetMinimizedByToggle && g_index != INVALID_SELECTION && g_index < (INT)g_windows.size())
        {
            HWND target = g_windows.at(g_index).Hwnd();
            SetWindowPlacement(target, &g_targetPlacement);
            g_targetMinimizedByToggle = false;
        }

        ShowWindow(hwnd, SW_SHOW);
    }
}

void CALLBACK WinEventProc(
    HWINEVENTHOOK hWinEventHook,
    DWORD         event,
    HWND          hwnd,
    LONG          idObject,
    LONG          idChild,
    DWORD         idEventThread,
    DWORD         dwmsEventTime)
{
    if (g_index == INVALID_SELECTION || g_index >= g_windows.size())
    {
        return;
    }

    HWND srchwnd = g_windows.at(g_index).Hwnd();

    switch (event)
    {
        // When game application is closed
        case EVENT_OBJECT_DESTROY:
            if (hwnd == srchwnd && idObject == OBJID_WINDOW && idChild == INDEXID_CONTAINER)
            {
                g_index = INVALID_SELECTION;

                MSGBOXPARAMSA msgbox = { 0 };
                CustomMessageBox(g_hwnd, msgbox, "Captured application terminated or is closed !! .\n\nPlease select another application");
                MessageBoxIndirectA(&msgbox);        
                return;
            }
            break;     
        // When game application is moved 
        case EVENT_SYSTEM_MOVESIZEEND:
            if (hwnd == srchwnd && idObject == OBJID_WINDOW && idChild == CHILDID_SELF)
            {            
                StartCaptureWithWindow(g_hwnd, globalList[g_appliedFilter].c_str(), g_windows.at(g_index).Hwnd(), g_appliedFilter, g_appliedFilter);
            }
            break;
        // When game application is opened from minimized state 
        case EVENT_SYSTEM_MINIMIZEEND:
            if (hwnd == srchwnd && idObject == OBJID_WINDOW && idChild == CHILDID_SELF && g_isGameWindowMin)
            {
                g_isGameWindowMin = false;
                StartCaptureWithWindow(g_hwnd, globalList[g_appliedFilter].c_str(), g_windows.at(g_index).Hwnd(), g_appliedFilter, g_appliedFilter);
            }
            break;
        // When game application is maximized or restored 
        case EVENT_OBJECT_LOCATIONCHANGE:
            if (hwnd == srchwnd && idObject == OBJID_WINDOW && idChild == INDEXID_CONTAINER)
            {
                WINDOWPLACEMENT windowPlace;
                windowPlace.length = sizeof(WINDOWPLACEMENT);
                GetWindowPlacement(hwnd, &windowPlace);

                if (SW_SHOWMAXIMIZED == windowPlace.showCmd && !g_isGameWindowMax)
                {
                    g_isGameWindowMax = true;
                    StartCaptureWithWindow(g_hwnd, globalList[g_appliedFilter].c_str(), g_windows.at(g_index).Hwnd(), g_appliedFilter, g_appliedFilter);
                }
                else if (SW_SHOWNORMAL == windowPlace.showCmd && g_isGameWindowMax)
                {
                    g_isGameWindowMax = false;
                    StartCaptureWithWindow(g_hwnd, globalList[g_appliedFilter].c_str(), g_windows.at(g_index).Hwnd(), g_appliedFilter, g_appliedFilter);
                }
                else if (SW_SHOWMINIMIZED == windowPlace.showCmd)
                {
                    g_isGameWindowMin = true;
                }
            }
            break;
    }
}

BOOL RegisterWinHook(HWND hwnd)
{
    DWORD dwProcessId;
    DWORD dwThreadId = GetWindowThreadProcessId(hwnd, &dwProcessId);
    if (dwThreadId)
    {
        g_hwEventHook = SetWinEventHook(EVENT_SYSTEM_MOVESIZEEND, EVENT_OBJECT_LOCATIONCHANGE, NULL, WinEventProc, dwProcessId, dwThreadId, WINEVENT_OUTOFCONTEXT);
    }
    return g_hwEventHook != nullptr;
}

BOOL CALLBACK AboutProc(
    HWND hDlg,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (msg)
    {       
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK)
            {
                isAboutShowing = false;
                EndDialog(hDlg, LOWORD(wParam));
                return TRUE;
            }
            break;
       
        case WM_CLOSE:
            isAboutShowing = false;
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
    }
    return FALSE;
}

BOOL CALLBACK SettingsProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (msg)
    {
        case WM_INITDIALOG:
        {
            const CHAR* ComboBoxItems[] = { "ALT","CTRL","SHIFT" };
            for (HotKey& hotKey : HotKeyManager::GetInstance().GetHotKeyList())
            {
                HWND hModKeyCBox = GetDlgItem(hDlg, hotKey.GetModKeyComID());
                SendMessageA(hModKeyCBox, CB_ADDSTRING, 0, (LPARAM)ComboBoxItems[0]);
                SendMessageA(hModKeyCBox, CB_ADDSTRING, 0, (LPARAM)ComboBoxItems[1]);
                SendMessageA(hModKeyCBox, CB_ADDSTRING, 0, (LPARAM)ComboBoxItems[2]);                
                int itemIndex = (INT)SendMessageA(hModKeyCBox, CB_FINDSTRING, -1, (LPARAM)hotKey.GetModKeyValue().c_str());
                SendMessage(hModKeyCBox, CB_SETCURSEL, (WPARAM)itemIndex, (LPARAM)0);                

                HWND hAlphanumKeyCBox = GetDlgItem(hDlg, hotKey.GetAlphanumKeyComID());
                for (char num = '1';num <= '9';num++)
                {
                    std::string str(1, num);
                    SendMessageA(hAlphanumKeyCBox, CB_ADDSTRING, 0, (LPARAM)str.c_str());
                }
                for (char letter = 'a'; letter <= 'z'; letter++)
                {
                    std::string str(1, letter);
                    SendMessageA(hAlphanumKeyCBox, CB_ADDSTRING, 0, (LPARAM)str.c_str());
                }
                itemIndex = (INT)SendMessageA(hAlphanumKeyCBox, CB_FINDSTRING, -1, (LPARAM)hotKey.GetAlphanumKeyValue().c_str());
                SendMessage(hAlphanumKeyCBox, CB_SETCURSEL, (WPARAM)itemIndex, (LPARAM)0);
            }
            
            if (g_screenshotPath.empty())
            {
                CHAR my_documents[MAX_PATH];
                SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, my_documents);
                g_screenshotPath = my_documents;
            }
            SetDlgItemTextA(hDlg, IDC_SSBROWSE_EDIT, g_screenshotPath.c_str());
            return TRUE;
        }
        case WM_COMMAND:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                int ItemIndex = (INT)SendMessage((HWND)lParam, (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
                CHAR ListItem[256];
                SendMessageA((HWND)lParam, (UINT)CB_GETLBTEXT, (WPARAM)ItemIndex, (LPARAM)ListItem);

                std::string listItem = ListItem; 
                auto hotKey  = HotKeyManager::GetInstance().GetHotKey(LOWORD(wParam));
                if (hotKey)
                {
                    hotKey->SetKeyValue(LOWORD(wParam), listItem);
                    if (HotKeyManager::GetInstance().ExistKeyCombination(*hotKey))
                    {
                        MSGBOXPARAMSA msgbox = { 0 };
                        CustomMessageBox(hDlg, msgbox, "Key combination already exists.\n\nPlease select different key values");
                        MessageBoxIndirectA(&msgbox);
                        hotKey->SetKeyValue(hotKey->GetModKeyComID(), std::string(""));
                        hotKey->SetKeyValue(hotKey->GetAlphanumKeyComID(), std::string(""));

                        HWND hModKeyCBox = GetDlgItem(hDlg, hotKey->GetModKeyComID());
                        SendMessage(hModKeyCBox, CB_SETCURSEL, (WPARAM)-1, (LPARAM)0);
                        HWND hAlphanumKeyCBox = GetDlgItem(hDlg, hotKey->GetAlphanumKeyComID());
                        SendMessage(hAlphanumKeyCBox, CB_SETCURSEL, (WPARAM)-1, (LPARAM)0);
                    }
                }                            
                return TRUE;
            }
            else 
            {
                switch (LOWORD(wParam))
                {
                    case IDRESET:
                    {
                        for (HotKey& hotKey : HotKeyManager::GetInstance().GetHotKeyList())
                        {
                            HWND hModKeyCBox = GetDlgItem(hDlg, hotKey.GetModKeyComID());
                            SendMessage(hModKeyCBox, CB_SETCURSEL, (WPARAM)-1, (LPARAM)0);
                            HWND hAlphanumKeyCBox = GetDlgItem(hDlg, hotKey.GetAlphanumKeyComID());
                            SendMessage(hAlphanumKeyCBox, CB_SETCURSEL, (WPARAM)-1, (LPARAM)0);
                        }
                        HotKeyManager::GetInstance().ResetAllAssignments(g_hwnd);
                    }
                    break;
                    case IDSAVE:
                    {
                        if (!HotKeyManager::GetInstance().ValidateKeyCombinations())
                        {
                            MSGBOXPARAMSA msgbox = { 0 };
                            CustomMessageBox(g_hwnd, msgbox, "Invalid key combinations !!!\n\n Please provide both key values (Modifier and Alphanumeric)");
                            MessageBoxIndirectA(&msgbox);
                            break;
                        }
                        IniParser::WriteToIni();                        
                        IniParser::WriteToIni("SCREENSHOT", g_screenshotPath);
                        HotKeyManager::GetInstance().RegisterAllHotKeys(g_hwnd);
                        isOptionsShowing = false;
                        EndDialog(hDlg, LOWORD(wParam));
                        return TRUE;
                    }
                    case IDC_SSBROWSE_BUTTON:
                        if (!isBrowseShowing)
                        {
                            isBrowseShowing = true;
                            g_screenshotPath = BrowseFolder(hDlg);
                            SetDlgItemTextA(hDlg, IDC_SSBROWSE_EDIT, g_screenshotPath.c_str());
                            isBrowseShowing = false;;
                        }                        
                        break;
                }                
            }
            break;
        case WM_CLOSE:
            isOptionsShowing = false;
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
    }
    return FALSE;
}

LRESULT CALLBACK WndProcMain(
    HWND   hwnd,
    UINT   msg,
    WPARAM wParam,
    LPARAM lParam)
{
    DWORD error = 0;
    switch (msg)
    {                 
        case IDC_POPUPMENU:
            switch (lParam)
            {
                case WM_LBUTTONDBLCLK:
                    if (!m_Show)
                    {
                        ToggleShowHide(hwnd);
                    }                   
                    break;
                case WM_RBUTTONDOWN:
                case WM_CONTEXTMENU:
                    if (!capturingStart)
                    {
                        MSGBOXPARAMSA msgbox = { 0 };
                        CustomMessageBox(hwnd, msgbox, "Please select application to capture!");                        
                        MessageBoxIndirectA(&msgbox);
                        break;
                    }
                    InputHelper::ShowContextMenu(hwnd, g_appliedFilter, g_appliedMode,m_Show);
                    break;
            }
            break;
        case WM_DESTROY:
            Shell_NotifyIcon(NIM_DELETE, &g_niData);
            if (g_hwEventHook)
            {
                UnhookWinEvent(g_hwEventHook);
            }
            HotKeyManager::GetInstance().UnRegisterAllHotKeys(hwnd);
            PostQuitMessage(0);
            break;
        case WM_COMMAND:
            if (wParam >= MENU_OFFSET && wParam < g_windows.size() + MENU_OFFSET)
            {
                g_index = (INT)(wParam - MENU_OFFSET);
                if (RegisterWinHook(g_windows.at(g_index).Hwnd()))
                {
                    StartCaptureWithWindow(hwnd, globalList[g_appliedFilter].c_str(), g_windows.at(g_index).Hwnd(), g_appliedFilter, g_appliedFilter);
                    WINDOWPLACEMENT windowPlace;
                    windowPlace.length = sizeof(WINDOWPLACEMENT);
                    GetWindowPlacement(g_windows.at(g_index).Hwnd(), &windowPlace);
                    g_isGameWindowMax = (SW_SHOWMAXIMIZED == windowPlace.showCmd);
                }
            }
            else
            {                
                switch (LOWORD(wParam))
                {
                    case ID_SHOW_HIDE_ITEM:
                        ToggleShowHide(hwnd);
                        break;
                    case ID_TRANSPARENT_ITEM:
                    {
                        g_appliedMode = ID_TRANSPARENT_ITEM;
                        long wAttr = GetWindowLongW(hwnd, GWL_EXSTYLE);
                        SetWindowLongW(hwnd, GWL_EXSTYLE, wAttr | WS_EX_TRANSPARENT | WS_EX_LAYERED);                        
                        break;
                    }
                    case ID_NONTRANSPARENT_ITEM:
                    {
                        g_appliedMode = ID_NONTRANSPARENT_ITEM;
                        long wAttr = GetWindowLongW(hwnd, GWL_EXSTYLE);
                        wAttr = (wAttr & ~WS_EX_TRANSPARENT) & ~WS_EX_LAYERED;
                        SetWindowLongW(hwnd, GWL_EXSTYLE, wAttr);
                        break;
                    }                        
                    case ID_FILTER_DEFAULT:
                        if (g_index != INVALID_SELECTION)
                        {
                            StartCaptureWithWindow(hwnd, globalList[ID_FILTER_DEFAULT].c_str(), g_windows.at(g_index).Hwnd(), g_appliedFilter, ID_FILTER_DEFAULT);
                        }
                        break;
                    case ID_FILTER_PROTAN:
                        if (g_index != INVALID_SELECTION)
                        {                            
                            StartCaptureWithWindow(hwnd, globalList[ID_FILTER_PROTAN].c_str(), g_windows.at(g_index).Hwnd(), g_appliedFilter, ID_FILTER_PROTAN);
                        }
                        break;
                    case ID_FILTER_DEUTAN:
                        if (g_index != INVALID_SELECTION)
                        {
                            StartCaptureWithWindow(hwnd, globalList[ID_FILTER_DEUTAN].c_str(), g_windows.at(g_index).Hwnd(), g_appliedFilter, ID_FILTER_DEUTAN);
                        }
                        break;
                    case ID_FILTER_TRITAN:
                        if (g_index != INVALID_SELECTION)
                        {
                            StartCaptureWithWindow(hwnd, globalList[ID_FILTER_TRITAN].c_str(), g_windows.at(g_index).Hwnd(), g_appliedFilter, ID_FILTER_TRITAN);
                        }
                        break;
                    case ID_FILTER_GSCALE:
                        if (g_index != INVALID_SELECTION)
                        {
                            StartCaptureWithWindow(hwnd, globalList[ID_FILTER_GSCALE].c_str(), g_windows.at(g_index).Hwnd(), g_appliedFilter, ID_FILTER_GSCALE);
                        }
                        break;
                    case IDM_EXIT:
                        DestroyWindow(hwnd);
                        break;
                    case ID_ABOUT_ITEM:
                        if (!isAboutShowing)
                        {
                            isAboutShowing = true;
                            DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUT), hwnd, (DLGPROC)AboutProc);                            
                        }
                        break;
                    case ID_SETTINGS_ITEM:
                        if (!isOptionsShowing)
                        {
                            isOptionsShowing = true;
                            DialogBoxW(NULL, MAKEINTRESOURCE(IDD_SETTINGS), hwnd, (DLGPROC)SettingsProc);                            
                        }                        
                        break;
                    case ID_HELP_ITEM:
                    {
                        HINSTANCE hresult = ShellExecuteA(GetDesktopWindow(), "open", "resource\\Userguide.pdf", NULL, NULL, SW_SHOWNORMAL);
                        if ((INT_PTR)hresult <= 32)
                        {
                            assert(hresult);
                        }                        
                    }
                    break;
                }
            }
            break; 
        case WM_SIZE:
            switch (wParam) 
            {                
                case SIZE_MAXIMIZED:
                    if (g_index != INVALID_SELECTION)
                    {
                        g_isToolWindowMax = true;
                        StartCaptureWithWindow(hwnd, globalList[g_appliedFilter].c_str(), g_windows.at(g_index).Hwnd(), g_appliedFilter, g_appliedFilter);
                    }
                    break;
                case SIZE_RESTORED:
                    if (g_isToolWindowMax && g_index != INVALID_SELECTION)
                    {
                        g_isToolWindowMax = false;
                        StartCaptureWithWindow(hwnd, globalList[g_appliedFilter].c_str(), g_windows.at(g_index).Hwnd(), g_appliedFilter, g_appliedFilter);
                    }
                    break;
            }
            break;
        case WM_WINDOWPOSCHANGED:
            if (g_index != INVALID_SELECTION)
            {
                g_app->WindowResized(g_windows.at(g_index).Hwnd(), hwnd, 0);
            }
            break;
        case WM_ENTERSIZEMOVE:
            GetWindowRect(hwnd, &g_rectBeforeMoveSize);
            break;
        case WM_EXITSIZEMOVE:
        {
            RECT rc;
            GetWindowRect(hwnd, &rc);

            // HTCAPTION makes even a plain click start a move loop, so only
            // restart the capture when the window was actually moved or resized.
            if (g_index != INVALID_SELECTION && !IsIconic(hwnd) && !EqualRect(&rc, &g_rectBeforeMoveSize))
            {
                StartCaptureWithWindow(hwnd, globalList[g_appliedFilter].c_str(), g_windows.at(g_index).Hwnd(), g_appliedFilter, g_appliedFilter);
            }
            break;
        }
        case WM_NCHITTEST:
        {
            // Game (click-through) mode must not intercept the mouse
            if (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT)
            {
                return HTTRANSPARENT;
            }

            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            RECT rc;
            GetWindowRect(hwnd, &rc);
            const int border = 8;

            bool left = pt.x < rc.left + border;
            bool right = pt.x >= rc.right - border;
            bool top = pt.y < rc.top + border;
            bool bottom = pt.y >= rc.bottom - border;

            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;
            return HTCAPTION;
        }
        case WM_NCRBUTTONUP:
            ShowSourceWindowMenu(hwnd);
            break;
        case WM_RBUTTONDOWN:
            ShowSourceWindowMenu(hwnd);
            break;
        case WM_KEYDOWN:
            switch (wParam)
            {
            case VK_LEFT:
                if (g_index != INVALID_SELECTION)
                {
                    g_app->WindowResized(g_windows.at(g_index).Hwnd(), hwnd, VK_LEFT);
                }
                break;
            case VK_UP:
                if (g_index != INVALID_SELECTION)
                {
                    g_app->WindowResized(g_windows.at(g_index).Hwnd(), hwnd, VK_UP);
                }
                break;
            case VK_RIGHT:
                if (g_index != INVALID_SELECTION)
                {
                    g_app->WindowResized(g_windows.at(g_index).Hwnd(), hwnd, VK_RIGHT);
                }
                break;
            case VK_DOWN:
                if (g_index != INVALID_SELECTION)
                {
                    g_app->WindowResized(g_windows.at(g_index).Hwnd(), hwnd, VK_DOWN);
                }
                break;
            }
            break;
        case WM_HOTKEY:            
            switch (wParam)
            {
                case ID_HOTKEY_INPUT:
                    if (g_appliedMode == ID_TRANSPARENT_ITEM)
                    {
                        SendMessageA(hwnd, WM_COMMAND, MAKEWPARAM(ID_NONTRANSPARENT_ITEM, 0), 0);
                        MSGBOXPARAMSA msgbox = { 0 };
                        CustomMessageBox(hwnd, msgbox, "Application is set to TOOL input mode");
                        MessageBoxIndirectA(&msgbox);
                    }
                    else
                    {
                        SendMessageA(hwnd, WM_COMMAND, MAKEWPARAM(ID_TRANSPARENT_ITEM,0), 0);
                        MSGBOXPARAMSA msgbox = { 0 };
                        CustomMessageBox(hwnd, msgbox, "Application is set to GAME input mode");
                        MessageBoxIndirectA(&msgbox);
                    }
                    break;
                case ID_HOTKEY_TOOL_SCREENSHOT:
                    if (!DirectoryExist(g_screenshotPath.c_str()))
                    {
                        MSGBOXPARAMSA msgbox = { 0 };
                        msgbox.cbSize = sizeof(MSGBOXPARAMS);
                        msgbox.hwndOwner = hwnd;
                        msgbox.hInstance = GetModuleHandle(NULL);
                        msgbox.lpszText = "Specified Screen shot folder doesn't exist !!!\n\n Save screenshot to default(Documents) location ?";
                        msgbox.lpszCaption = "UbiChroma";
                        msgbox.dwStyle = MB_YESNOCANCEL | MB_USERICON | MB_SYSTEMMODAL;
                        msgbox.lpszIcon = MAKEINTRESOURCEA(IDI_AUTOMATION_ICON);                        
                        int msgID = MessageBoxIndirectA(&msgbox);

                        if (msgID == IDYES)
                        {
                            Sleep(SLEEP_CONST);
                            if (!ScreenshotHelper(hwnd, g_windows.at(g_index).Hwnd(), ID_HOTKEY_TOOL_SCREENSHOT))
                            {
                                MSGBOXPARAMSA msgbox = { 0 };
                                CustomMessageBox(hwnd, msgbox, "Screen shot failed !!!\n\nPlease try again");
                                MessageBoxIndirectA(&msgbox);
                            }
                        }                        
                    }
                    else
                    {
                        if (!ScreenshotHelper(hwnd, g_windows.at(g_index).Hwnd(), ID_HOTKEY_TOOL_SCREENSHOT))
                        {
                            MSGBOXPARAMSA msgbox = { 0 };
                            CustomMessageBox(hwnd, msgbox, "Screen shot failed !!!\n\nPlease try again");
                            MessageBoxIndirectA(&msgbox);
                        }
                    }
                    break;
                case ID_HOTKEY_TOOLGAME_SCREENSHOT:
                    if (!DirectoryExist(g_screenshotPath.c_str()))
                    {
                        MSGBOXPARAMSA msgbox = { 0 };
                        msgbox.cbSize = sizeof(MSGBOXPARAMS);
                        msgbox.hwndOwner = hwnd;
                        msgbox.hInstance = GetModuleHandle(NULL);
                        msgbox.lpszText = "Specified Screen shot folder doesn't exist !!!\n\n Save screenshot to default(Documents) location ?";
                        msgbox.lpszCaption = "UbiChroma";
                        msgbox.dwStyle = MB_YESNOCANCEL | MB_USERICON | MB_SYSTEMMODAL;
                        msgbox.lpszIcon = MAKEINTRESOURCEA(IDI_AUTOMATION_ICON);
                        int msgID = MessageBoxIndirectA(&msgbox);

                        if (msgID == IDYES)
                        {
                            Sleep(SLEEP_CONST);
                            if (!ScreenshotHelper(hwnd, g_windows.at(g_index).Hwnd(), ID_HOTKEY_TOOLGAME_SCREENSHOT))
                            {
                                MSGBOXPARAMSA msgbox = { 0 };
                                CustomMessageBox(hwnd, msgbox, "Screen shot failed !!!\n\nPlease try again");
                                MessageBoxIndirectA(&msgbox);
                            }
                        }
                    }
                    else
                    {
                        if (!ScreenshotHelper(hwnd, g_windows.at(g_index).Hwnd(), ID_HOTKEY_TOOLGAME_SCREENSHOT))
                        {
                            MSGBOXPARAMSA msgbox = { 0 };
                            CustomMessageBox(hwnd, msgbox, "Screen shot failed !!!\n\nPlease try again");
                            MessageBoxIndirectA(&msgbox);
                        }
                    }
                    break;
                case ID_HOTKEY_DEF_FLTR:
                    SendMessageA(hwnd, WM_COMMAND, MAKEWPARAM(ID_FILTER_DEFAULT,0), 0);
                    break;
                case ID_HOTKEY_PRO_FLTR:
                    SendMessageA(hwnd, WM_COMMAND, MAKEWPARAM(ID_FILTER_PROTAN,0), 0);
                    break;
                case ID_HOTKEY_DEU_FLTR:
                    SendMessageA(hwnd, WM_COMMAND, MAKEWPARAM(ID_FILTER_DEUTAN,0), 0);
                    break;
                case ID_HOTKEY_TRI_FLTR:
                    SendMessageA(hwnd, WM_COMMAND, MAKEWPARAM(ID_FILTER_TRITAN,0), 0);
                    break;
                case ID_HOTKEY_GRY_FLTR:
                    SendMessageA(hwnd, WM_COMMAND, MAKEWPARAM(ID_FILTER_GSCALE,0), 0);
                    break;
                case ID_HOTKEY_SHOW_HIDE:
                    ToggleShowHide(hwnd);
                    break;
            }
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
            break;
    }
    return 0;
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    // Init COM
    init_apartment(apartment_type::single_threaded);

    globalList.clear();
    globalList.insert(std::pair<int, std::string>(ID_FILTER_DEFAULT,"Default"));
    globalList.insert(std::pair<int, std::string>(ID_FILTER_PROTAN ,"Protanopia")); 
    globalList.insert(std::pair<int, std::string>(ID_FILTER_DEUTAN, "Deuteranopia")); 
    globalList.insert(std::pair<int, std::string>(ID_FILTER_TRITAN ,"Tritanopia"));
    globalList.insert(std::pair<int, std::string>(ID_FILTER_GSCALE, "Grayscale"));
    globalList.insert(std::pair<int, std::string>(ID_TRANSPARENT_ITEM, "Game Input"));
    globalList.insert(std::pair<int, std::string>(ID_NONTRANSPARENT_ITEM, "Tool Input"));
    globalList.insert(std::pair<int, std::string>(ID_HOTKEY_TOOL_SCREENSHOT, "ToolScreenshot"));
    globalList.insert(std::pair<int, std::string>(ID_HOTKEY_TOOLGAME_SCREENSHOT, "ToolAndGameScreenshot"));

    // Create the main window
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_DROPSHADOW | CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProcMain;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIconA(hInstance, MAKEINTRESOURCEA(IDI_AUTOMATION_ICON));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"UbiChroma";
    wcex.hIconSm = LoadIconA(wcex.hInstance, MAKEINTRESOURCEA(IDI_AUTOMATION_ICON));
    WINRT_VERIFY(RegisterClassEx(&wcex));

    HWND hwnd = CreateWindowEx(
        WS_EX_TOPMOST,
        L"UbiChroma",
        L"UbiChroma",
        WS_POPUP,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        600,
        300,
        NULL,
        NULL,
        hInstance,
        NULL);
    WINRT_VERIFY(hwnd);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    g_hwnd = hwnd;    

    // set INI file path 
    CHAR my_appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, my_appdata)))
    {
        g_iniFilePath = my_appdata;
        g_iniFilePath += "\\UbiChroma";
        CreateDirectoryA(g_iniFilePath.c_str(), NULL);
        g_iniFilePath += "\\Ubichroma.ini";
    }

    // load ini data
    IniParser::ParseIniData();
    g_screenshotPath = IniParser::ParseSSSectionData("SCREENSHOT");
    HotKeyManager::GetInstance().RegisterAllHotKeys(g_hwnd);

    ZeroMemory(&g_niData, sizeof(NOTIFYICONDATA));

    ULONGLONG ullVersion = GetDllVersion(L"Shell32.dll");
    if (ullVersion >= MAKEDLLVERULL(5, 0, 0, 0))
        g_niData.cbSize = sizeof(NOTIFYICONDATA);
    else g_niData.cbSize = NOTIFYICONDATA_V2_SIZE;

    // the ID number can be anything you choose
    g_niData.uID = 1;

    // state which structure members are valid
    g_niData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;

    // load the icon
    g_niData.hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_AUTOMATION_ICON),
        IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR);

    g_niData.hWnd = hwnd;
    g_niData.uCallbackMessage = IDC_POPUPMENU;

    // tool tip message
    lstrcpyn(g_niData.szTip, L"UbiChroma", sizeof(g_niData.szTip) / sizeof(TCHAR));

    Shell_NotifyIcon(NIM_ADD, &g_niData);

    // free icon handle
    if (g_niData.hIcon && DestroyIcon(g_niData.hIcon))
        g_niData.hIcon = NULL;

    // Create a DispatcherQueue for our thread
    auto controller = CreateDispatcherQueueController();

    // Initialize Composition
    auto compositor = Compositor();
    auto target = CreateDesktopWindowTarget(compositor, hwnd);
    auto root = compositor.CreateContainerVisual();
    root.RelativeSizeAdjustment({ 1.0f, 1.0f });
    target.Root(root);

    // Enqueue our capture work on the dispatcher
    auto queue = controller.DispatcherQueue();
    auto success = queue.TryEnqueue([=]() -> void
        {
            g_app->Initialize(root);
        });
    WINRT_VERIFY(success);

    // Message pump
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
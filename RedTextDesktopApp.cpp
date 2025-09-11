#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <Psapi.h>
#include <sddl.h>
#include <string>
#include <wincrypt.h>
#include <ncryptprotect.h>
#include <stdlib.h>
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ncrypt.lib")

#define PROCESS_ARRAY 2048

HFONT g_font;
wchar_t* text = NULL;
HWND g_hWnd = NULL;
int g_fontSize = 36;
int g_PosX = 0;
int g_PosY = 0;

#define WindowClassName "WindowsUpdate"

void OnTopThread(void* args)
{
    HWND hwnd = (HWND)args;
    while (1)
    {
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        SetWindowPos(hwnd, HWND_TOPMOST, g_PosX, g_PosY, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        Sleep(10);
    }
}

// Declaration of the window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Set text color to black
        SetTextColor(hdc, RGB(255, 0, 0));
        // Set transparent background for text
        OutputDebugStringW(text);
        if (text == L"?") {
            OutputDebugStringA("empty string");
            //OutputDebugStringW(text);
            SetBkMode(hdc, TRANSPARENT);
        }

        SelectObject(hdc, g_font);
        // Draw text
        RECT rect;
        GetWindowRect(hwnd, &rect);
        DrawTextW(hdc, text, -1, &rect, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        EndPaint(hwnd, &ps);
        break;
    }
    case WM_CREATE:
    {
        SetLayeredWindowAttributes(hwnd, RGB(255, 0, 128), 0, LWA_COLORKEY);  // 192 out of 255 for semi-transparency
        SetWindowPos(hwnd, HWND_TOPMOST, g_PosX, g_PosY, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        SetWindowDisplayAffinity(hwnd, 0x11);
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)OnTopThread, (void*)hwnd, 0, NULL);
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        RECT rect;
        GetWindowRect(hwnd, &rect);
        InvalidateRect(hwnd, &rect, true);
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void KillPrev()
{
    bool killed = false;
    int cnt = 0;
    while (cnt < 10)
    {
        HWND oHwnd = FindWindowA(WindowClassName, NULL);
        if (oHwnd != NULL)
        {
            PostMessageA(oHwnd, WM_CLOSE, 0, 0);
            killed = true;
            Sleep(100);
        }
        else
        {
            break;
        }
        cnt++;
    }
    if (killed)
    {
        Sleep(100);
    }
}


std::string wcharToString(wchar_t input[1024])
{
    std::wstring wstringValue(input);
    std::string convertedString(wstringValue.begin(), wstringValue.end());

    return convertedString;
}

void GetTokenInfo(HANDLE TokenHandle)
{
    LPVOID TokenInformation = NULL;
    DWORD TokenInformationLength = 0;
    DWORD ReturnLength;
    SID_NAME_USE SidType;

    GetTokenInformation(TokenHandle, TokenUser, NULL, 0, &ReturnLength);

    PTOKEN_USER pTokenUser = (PTOKEN_USER)GlobalAlloc(GPTR, ReturnLength);

    GetTokenInformation(TokenHandle, TokenUser, pTokenUser, ReturnLength, &ReturnLength);

    wchar_t* userSid = NULL;
    ConvertSidToStringSidW(pTokenUser->User.Sid, &userSid);
    std::string sid = wcharToString(userSid);

    TCHAR szGroupName[256];
    TCHAR szDomainName[256];

    DWORD cchGroupName = 256;
    DWORD cchDomainName = 256;

    LookupAccountSid(NULL, pTokenUser->User.Sid, szGroupName, &cchGroupName, szDomainName, &cchDomainName, &SidType);

    TCHAR szOutput[1000];
    _stprintf_s(szOutput, sizeof(szOutput) / sizeof(TCHAR), _T("[+] Current SID: %s\\%s @ %s\n"), szDomainName, szGroupName, sid.c_str());
    OutputDebugString(szOutput);
}

int LocateWinLogonProcess()
{

    DWORD lpidProcess[PROCESS_ARRAY], lpcbNeeded, cProcesses;

    EnumProcesses(lpidProcess, sizeof(lpidProcess), &lpcbNeeded);

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    PROCESSENTRY32 p32;
    p32.dwSize = sizeof(PROCESSENTRY32);

    int processWinlogonPid;

    if (Process32First(hSnapshot, &p32))
    {
        do {
            if (strcmp(p32.szExeFile, "winlogon.exe")==0)
            {
                // OutputDebugStringA("[+] Located winlogon.exe by process name ");
                processWinlogonPid = p32.th32ProcessID;

                return processWinlogonPid;
                break;
            }
        } while (Process32Next(hSnapshot, &p32));

        CloseHandle(hSnapshot);
    }
}

void EnableSeTcpPrivilege()
{
    LUID luid;
    HANDLE currentProc = OpenProcess(PROCESS_ALL_ACCESS, false, GetCurrentProcessId());

    if (currentProc)
    {
        HANDLE TokenHandle(NULL);
        BOOL hProcessToken = OpenProcessToken(currentProc, TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &TokenHandle);
        if (hProcessToken)
        {
            BOOL checkToken = LookupPrivilegeValue(NULL, "SeTcbPrivilege", &luid);

            if (!checkToken)
            {
                // OutputDebugStringA("[+] Current process token already includes SeDebugPrivilege\n");
            }
            else
            {
                TOKEN_PRIVILEGES tokenPrivs;

                tokenPrivs.PrivilegeCount = 1;
                tokenPrivs.Privileges[0].Luid = luid;
                tokenPrivs.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

                BOOL adjustToken = AdjustTokenPrivileges(TokenHandle, FALSE, &tokenPrivs, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL);

                if (adjustToken != 0)
                {
                    // OutputDebugStringA("[+] Added SeTcbPrivilege to the current process token");
                }
            }
            CloseHandle(TokenHandle);
        }
    }
    CloseHandle(currentProc);
}

BOOL StealToken(int TargetPID, HANDLE *outHandle)
{
    HANDLE hProcess = NULL;
    HANDLE TokenHandle = NULL;
    HANDLE NewToken = NULL;
    BOOL OpenToken;
    BOOL Impersonate;
    BOOL Duplicate;

    hProcess = OpenProcess(0x1000, 0, TargetPID);

    if (!hProcess)
    {
        // OutputDebugStringA("[!] Failed to obtain a HANDLE to the target PID");
        return -1;
    }

    // OutputDebugStringA("[+] Obtained a HANDLE to the target PID");

    OpenToken = OpenProcessToken(hProcess, TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY, &TokenHandle);

    if (!OpenToken)
    {
        // OutputDebugStringA("[!] Failed to obtain a HANDLE to the target TOKEN");
        //std::cout << GetLastError();
    }

    // OutputDebugStringA("[+] Obtained a HANDLE to the target TOKEN");

    Impersonate = ImpersonateLoggedOnUser(TokenHandle);

    if (!Impersonate)
    {
        // OutputDebugStringA("[!] Failed to impersonate the TOKEN's user");
        return -1;
    }

    // OutputDebugStringA("[+] Impersonated the TOKEN's user");

    Duplicate = DuplicateTokenEx(TokenHandle, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &NewToken);

    if (!Duplicate)
    {
        // OutputDebugStringA("[!] Failed to duplicate the target TOKEN");
        return -1;
    }

    // OutputDebugStringA("[+] Duplicated the target TOKEN");
    bool result = false;
    
    DWORD v24 = 1;
    if (SetTokenInformation(NewToken, TokenUIAccess, &v24, 4u)) {
        *outHandle = NewToken;
        result = true;
    }
    else result = false;
    //CreateImpersonatedProcess(NewToken);
    CloseHandle(hProcess);
    CloseHandle(TokenHandle);
    return result;
}

//void GetExecutableDirectory(char* outPath, size_t size) {
//    char exePath[MAX_PATH];
//    if (GetModuleFileNameA(NULL, exePath, MAX_PATH)) {
//        // Strip the executable name to get the directory
//        char* lastSlash = strrchr(exePath, '\\');
//        if (lastSlash) {
//            *lastSlash = '\0'; // Null-terminate at the last slash
//            strncpy_s(outPath, size, exePath, _TRUNCATE);
//        }
//    }
//    else {
//        // OutputDebugStringA("Failed to get executable path.");
//        outPath[0] = '\0';
//    }
//}
//
//bool IsAbsolutePath(const char* path) {
//    // Check if the path starts with a drive letter (e.g., C:\)
//    if ((strlen(path) > 2) && (path[1] == ':') &&
//        ((path[2] == '\\') || (path[2] == '/'))) {
//        return true;
//    }
//
//    // Check if the path starts with a UNC path (e.g., \\server\share)
//    if ((strlen(path) > 1) && (path[0] == '\\') && (path[1] == '\\')) {
//        return true;
//    }
//
//    // Otherwise, it's a relative path
//    return false;
//}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {


    STARTUPINFOW startupInfo = { 0 };
    PROCESS_INFORMATION processInformation = { 0 };
    wchar_t* commandLine;
    HANDLE hNewToken, hCurrentToken;
    DWORD err = 0;
    DWORD tokenInfo;
    DWORD returnLen;
    
    int v20 = 0;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hCurrentToken)) {
        err = GetLastError();
        text = _wcsdup(L"ERROR: Need Admin access");
        // goto need admin access text assign
    }
    // Get current process's token information to see its privilege
    // GetTokenInfo(hCurrentToken);
    if (GetTokenInformation(hCurrentToken, TokenUIAccess, &tokenInfo, 4u, &returnLen))
    {
        err = returnLen;
        v20 = 1;
    }
    else
        err = GetLastError();
    CloseHandle(hCurrentToken);
    if (v20 && tokenInfo)
    {
        if (__argc <= 1)
        {
            text = _wcsdup(L"ERROR: No file selected");
        }
        else // __argc >= 2
        {
            //MessageBox(NULL, "", "", MB_OKCANCEL);
            /*char exeDir[MAX_PATH];
            char fullPath[MAX_PATH];
            // Get the executable directory
            if (IsAbsolutePath(__argv[1]))
                strncpy_s(fullPath, sizeof(fullPath), __argv[1], _TRUNCATE);
            else {
                GetExecutableDirectory(exeDir, MAX_PATH);
                snprintf(fullPath, MAX_PATH, "%s\\%s", exeDir, __argv[1]);
            }*/

            // OutputDebugStringA(__argv[1]);

            FILE* f = fopen(__argv[1], "rb");
            if (!f)
            {
                text = _wcsdup(L"ERROR: Can't open file");
            }
            else
            {
                // Check for BOM
                unsigned char bom[2];
                if (fread(bom, 1, 2, f) != 2) {
                    text = _wcsdup(L"ERROR: File is too small\n");
                    fclose(f);
                }

                int has_bom = (bom[0] == 0xFF && bom[1] == 0xFE); // UTF-16LE BOM
                if (!has_bom) {
                    // If no BOM, reset position to start of file
                    fseek(f, 0, SEEK_SET);
                }

                // Determine actual content size after skipping BOM (if present)
                fseek(f, 0, SEEK_END);
                long size = ftell(f) - (has_bom ? 2 : 0);
                fseek(f, has_bom ? 2 : 0, SEEK_SET);

                // Ensure buffer is large enough for null termination
                text = (wchar_t*)calloc((size / sizeof(wchar_t)) + 1, sizeof(wchar_t));
                if (!text) {
                    text = _wcsdup(L"ERROR: Memory allocation failed\n");
                    fclose(f);
                    return 1;
                }

                // Read file into the buffer
                fread(text, sizeof(wchar_t), size / sizeof(wchar_t), f);
                fclose(f);
            }
        }
    }
    else {
        int winLogonPID = LocateWinLogonProcess();
        EnableSeTcpPrivilege();
        if (StealToken(winLogonPID, &hNewToken)) {
            // log newly created token information
            // GetTokenInfo(hNewToken);
            GetStartupInfoW(&startupInfo);
            commandLine = GetCommandLineW();
            if (CreateProcessWithTokenW(hNewToken, LOGON_WITH_PROFILE, 0, commandLine, 0, NULL, NULL, &startupInfo, &processInformation)) {
                CloseHandle(processInformation.hProcess);
                CloseHandle(processInformation.hThread);
                ExitProcess(0);
            }
            err = GetLastError();
            CloseHandle(hNewToken);
            text = _wcsdup(L"ERROR: Need Admin access");
        }
    }    

    if (__argc >= 3)
    {
        g_fontSize = atoi(__argv[2]);
        if (g_fontSize <= 0 || g_fontSize > 150)
            g_fontSize = 36;
    }

    if (__argc >= 4)
    {
        g_PosX = atoi(__argv[3]);
        if (g_PosX < 0) g_PosX = 0;
    }

    if (__argc >= 5)
    {
        g_PosY = atoi(__argv[4]);
        if (g_PosY < 0) g_PosY = 0;
    }

    // Kill old process
    KillPrev();
    if (text == L"close") {
        return 0;
    }

    // Register the window class.
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(255, 0, 128));
    wc.lpszClassName = WindowClassName;
    RegisterClassExA(&wc);

    // Create the window.
    HWND hwnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,  // Extended window style for transparency
        WindowClassName,
        "",
        WS_POPUP | WS_VISIBLE | WS_SYSMENU,
        g_PosX, g_PosY, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) * 3 / 4,
        NULL, NULL, hInstance, NULL
    );

    g_hWnd = hwnd;

    g_font = CreateFontA(g_fontSize, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, "Arial");

    // Show the window
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd);

    // Run the message loop.
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

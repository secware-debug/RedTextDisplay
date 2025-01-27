#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <Psapi.h>
#include <sddl.h>
#include <string>
#include <vector>
#pragma comment (lib,"advapi32.lib")

#define PROCESS_ARRAY 2048

HFONT g_font;
wchar_t* text = NULL;
HWND g_hWnd = NULL;
int g_fontSize = 36;

#define WindowClassName "WindowsUpdate"

void OnTopThread(void* args)
{
    HWND hwnd = (HWND)args;
    while (1)
    {
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
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
        // SetBkMode(hdc, TRANSPARENT);

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
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
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

//void GetTokenInfo(HANDLE TokenHandle)
//{
//    LPVOID TokenInformation = NULL;
//    DWORD TokenInformationLength = 0;
//    DWORD ReturnLength;
//    SID_NAME_USE SidType;
//
//    // Get the user information
//    GetTokenInformation(TokenHandle, TokenUser, NULL, 0, &ReturnLength);
//    PTOKEN_USER pTokenUser = (PTOKEN_USER)GlobalAlloc(GPTR, ReturnLength);
//    GetTokenInformation(TokenHandle, TokenUser, pTokenUser, ReturnLength, &ReturnLength);
//
//    // Convert SID to string
//    wchar_t* userSid = NULL;
//    ConvertSidToStringSidW(pTokenUser->User.Sid, &userSid);
//    std::string sid = wcharToString(userSid);
//
//    TCHAR szGroupName[256];
//    TCHAR szDomainName[256];
//
//    DWORD cchGroupName = 256;
//    DWORD cchDomainName = 256;
//
//    // Lookup account SID
//    LookupAccountSid(NULL, pTokenUser->User.Sid, szGroupName, &cchGroupName, szDomainName, &cchDomainName, &SidType);
//
//    // Output user information
//    TCHAR szOutput[1000];
//    _stprintf_s(szOutput, sizeof(szOutput) / sizeof(TCHAR), _T("[+] Current SID: %s\\%s @ %s\n"), szDomainName, szGroupName, sid.c_str());
//    OutputDebugString(szOutput);
//
//    // Retrieve privileges
//    GetTokenInformation(TokenHandle, TokenPrivileges, NULL, 0, &ReturnLength);
//    PTOKEN_PRIVILEGES pTokenPrivileges = (PTOKEN_PRIVILEGES)GlobalAlloc(GPTR, ReturnLength);
//    if (GetTokenInformation(TokenHandle, TokenPrivileges, pTokenPrivileges, ReturnLength, &ReturnLength))
//    {
//        for (DWORD i = 0; i < pTokenPrivileges->PrivilegeCount; i++)
//        {
//            LUID PrivilegeLuid = pTokenPrivileges->Privileges[i].Luid;
//            TCHAR szPrivilegeName[256];
//            DWORD cchPrivilegeName = 256;
//
//            // Lookup privilege name
//            if (LookupPrivilegeName(NULL, &PrivilegeLuid, szPrivilegeName, &cchPrivilegeName))
//            {
//                // Determine if privilege is enabled
//                TCHAR szStatus[10];
//                if (pTokenPrivileges->Privileges[i].Attributes & SE_PRIVILEGE_ENABLED)
//                {
//                    _tcscpy_s(szStatus, _T("Enabled"));
//                }
//                else
//                {
//                    _tcscpy_s(szStatus, _T("Disabled"));
//                }
//
//                // Output privilege information
//                _stprintf_s(szOutput, sizeof(szOutput) / sizeof(TCHAR), _T("[+] Privilege: %s - %s\n"), szPrivilegeName, szStatus);
//                OutputDebugString(szOutput);
//            }
//        }
//    }
//
//    // Cleanup
//    GlobalFree(pTokenUser);
//    GlobalFree(pTokenPrivileges);
//    LocalFree(userSid);
//}

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
                OutputDebugStringA("[+] Located winlogon.exe by process name ");
                processWinlogonPid = p32.th32ProcessID;

                return processWinlogonPid;
                break;
            }
        } while (Process32Next(hSnapshot, &p32));

        CloseHandle(hSnapshot);
    }
}


//DWORD GetTokenWithPrivilege(HANDLE* outToken) {
//    HANDLE hCurrentProcess = GetCurrentProcess();
//    HANDLE o_Token = nullptr;
//    //LPVOID tokenSessionId = nullptr, winlogonTokenSessionId = nullptr;
//    DWORD o_SessionId = 0;
//    DWORD winlogon_SessionId = 0;
//    DWORD dwSize = sizeof(o_SessionId);
//    PRIVILEGE_SET requiredPrivileges = { 0 };
//    HANDLE winlogonToken;
//    HANDLE newToken = nullptr;
//    HANDLE hSnapshot = nullptr;
//    DWORD dwWinLogonPID = 0;
//    BOOL hasPrivileges = FALSE;
//    DWORD error = ERROR_SUCCESS;
//
//    if (!OpenProcessToken(hCurrentProcess, 0xAu, &o_Token)) {
//        return GetLastError();
//    }
//
//    if (!GetTokenInformation(o_Token, TokenSessionId, &o_SessionId, dwSize, &dwSize)) {
//        CloseHandle(o_Token);
//        return GetLastError();
//    }
//
//    requiredPrivileges.PrivilegeCount = 1;
//    requiredPrivileges.Control = PRIVILEGE_SET_ALL_NECESSARY;
//
//    if (!LookupPrivilegeValueA(0, "SeTcbPrivilege", &requiredPrivileges.Privilege[0].Luid)) {
//        CloseHandle(o_Token);
//        return GetLastError();
//    }
//
//    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
//    if (hSnapshot == INVALID_HANDLE_VALUE) {
//        CloseHandle(o_Token);
//        return GetLastError();
//    }
//
//    PROCESSENTRY32 processEntry;
//    processEntry.dwSize = sizeof(PROCESSENTRY32);
//    error = 1168;
//
//    if (Process32First(hSnapshot, &processEntry)) {
//        do {
//            if (strcmp(processEntry.szExeFile, "winlogon.exe") == 0) {
//                dwWinLogonPID = processEntry.th32ProcessID;
//                break;
//            }
//        } while (Process32Next(hSnapshot, &processEntry));
//    }
//    CloseHandle(hSnapshot);
//
//    if (dwWinLogonPID != 0) {
//        HANDLE hWinLogonProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwWinLogonPID);
//        if (hWinLogonProcess) {
//            if (OpenProcessToken(hWinLogonProcess, TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY, &winlogonToken)) {
//                if (PrivilegeCheck(winlogonToken, &requiredPrivileges, &hasPrivileges) && hasPrivileges && GetTokenInformation(winlogonToken, TokenSessionId, &winlogon_SessionId, dwSize, &dwSize) && winlogon_SessionId == o_SessionId) {
//                    if (DuplicateTokenEx(winlogonToken, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &newToken)) {
//                        error = 0;
//                    }
//                    else
//                        error = GetLastError();
//                }
//                CloseHandle(winlogonToken);
//            }
//            CloseHandle(hWinLogonProcess);
//        }
//        else {
//            //CloseHandle(hWinLogonProcess);
//        }
//    }
//    
//    if (SetThreadToken(0, newToken)) {
//        if (DuplicateTokenEx(newToken, 0x8bu, 0, SecurityAnonymous, TokenPrimary, outToken)) {
//            DWORD v24 = 1;
//            if (!SetTokenInformation(*outToken, TokenUIAccess, &v24, 4u)) {
//                CloseHandle(*outToken);
//                RevertToSelf();
//                CloseHandle(newToken);
//                CloseHandle(o_Token);
//                return GetLastError();
//            }
//        }
//        else {
//            error = GetLastError();
//        }
//        RevertToSelf();
//        CloseHandle(newToken);
//        CloseHandle(o_Token);
//        return error;
//    }
//    else {
//        CloseHandle(newToken);
//        CloseHandle(o_Token);
//        return GetLastError();
//    }
//}

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
                OutputDebugStringA("[+] Current process token already includes SeDebugPrivilege\n");
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
                    OutputDebugStringA("[+] Added SeTcbPrivilege to the current process token");
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
        OutputDebugStringA("[!] Failed to obtain a HANDLE to the target PID");
        return -1;
    }

    OutputDebugStringA("[+] Obtained a HANDLE to the target PID");

    OpenToken = OpenProcessToken(hProcess, TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY, &TokenHandle);

    if (!OpenToken)
    {
        OutputDebugStringA("[!] Failed to obtain a HANDLE to the target TOKEN");
        //std::cout << GetLastError();
    }

    OutputDebugStringA("[+] Obtained a HANDLE to the target TOKEN");

    Impersonate = ImpersonateLoggedOnUser(TokenHandle);

    if (!Impersonate)
    {
        OutputDebugStringA("[!] Failed to impersonate the TOKEN's user");
        return -1;
    }

    OutputDebugStringA("[+] Impersonated the TOKEN's user");

    Duplicate = DuplicateTokenEx(TokenHandle, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &NewToken);

    if (!Duplicate)
    {
        OutputDebugStringA("[!] Failed to duplicate the target TOKEN");
        return -1;
    }

    OutputDebugStringA("[+] Duplicated the target TOKEN");
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

void GetExecutableDirectory(char* outPath, size_t size) {
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH)) {
        // Strip the executable name to get the directory
        char* lastSlash = strrchr(exePath, '\\');
        if (lastSlash) {
            *lastSlash = '\0'; // Null-terminate at the last slash
            strncpy_s(outPath, size, exePath, _TRUNCATE);
        }
    }
    else {
        OutputDebugStringA("Failed to get executable path.");
        outPath[0] = '\0';
    }
}

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
        return err;
        // goto need admin access text assign
    }

    OutputDebugStringA("Current token info: ");
    GetTokenInfo(hCurrentToken);
    if (GetTokenInformation(hCurrentToken, TokenUIAccess, &tokenInfo, 4u, &returnLen))
    {
        char str[50];
        sprintf_s(str, sizeof(str), "TokenInfo: %d", tokenInfo);
        OutputDebugStringA(str);
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
            char exeDir[MAX_PATH];
            char fullPath[MAX_PATH];
            // Get the executable directory
            GetExecutableDirectory(exeDir, MAX_PATH);
            snprintf(fullPath, MAX_PATH, "%s\\%s", exeDir, __argv[1]);

            FILE* f = fopen(fullPath, "rb");
            if (!f)
            {
                text = _wcsdup(L"ERROR: Can't open file");
            }
            else
            {
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fseek(f, 0, SEEK_SET);
                text = (wchar_t*)calloc(size + 2, 1);
                fread(text, 1, size, f);
                fclose(f);
            }
        }
    }
    else {
        int winLogonPID = LocateWinLogonProcess();
        EnableSeTcpPrivilege();
        if (StealToken(winLogonPID, &hNewToken)) {
            //OutputDebugStringA("Newly created token info: ");
            GetTokenInfo(hNewToken);
            GetStartupInfoW(&startupInfo);
            commandLine = GetCommandLineW();
            MessageBoxW(NULL, commandLine, L"", MB_OK);
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
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) * 3 / 4,
        NULL, NULL, hInstance, NULL
    );

    g_hWnd = hwnd;

    g_font = CreateFontA(g_fontSize, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, "Arial");

    // Show the window.
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

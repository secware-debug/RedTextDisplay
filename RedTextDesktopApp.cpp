#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <Psapi.h>
#include <sddl.h>

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

DWORD GetTokenWithPrivilege(HANDLE* outToken) {
    HANDLE hCurrentProcess = GetCurrentProcess();
    HANDLE hToken = nullptr;
    LPVOID tokenSessionId;
    PDWORD returnLength;
    PRIVILEGE_SET requiredPrivileges = { 0 };
    PHANDLE clientToken;
    PHANDLE newToken = nullptr;
    HANDLE hSnapshot = nullptr;
    DWORD dwWinLogonPID = 0;
    BOOL hasPrivileges = FALSE;
    DWORD error = ERROR_SUCCESS;

    if (!OpenProcessToken(hCurrentProcess, TOKEN_QUERY | TOKEN_DUPLICATE, &hToken)) {
        return GetLastError();
    }

    if (!GetTokenInformation(hToken, TokenSessionId, tokenSessionId, sizeof(tokenSessionId), returnLength)) {
        CloseHandle(hToken);
        return GetLastError();
    }

    requiredPrivileges.PrivilegeCount = 1;
    requiredPrivileges.Control = PRIVILEGE_SET_ALL_NECESSARY;

    if (!LookupPrivilegeValueW(nullptr, L"SeTcbPrivilege", &requiredPrivileges.Privilege[0].Luid)) {
        CloseHandle(hToken);
        return GetLastError();
    }

    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        CloseHandle(hToken);
        return GetLastError();
    }

    PROCESSENTRY32 processEntry;
    processEntry.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &processEntry)) {
        do {
            if (strcmp(processEntry.szExeFile, "winlogon.exe") == 0) {
                dwWinLogonPID = processEntry.th32ProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &processEntry));
    }
    CloseHandle(hSnapshot);

    if (dwWinLogonPID != 0) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwWinLogonPID);
        if (hProcess) {
            if (OpenProcessToken(hProcess, TOKEN_QUERY | TOKEN_DUPLICATE, clientToken)) {
                if (PrivilegeCheck(clientToken, &requiredPrivileges, &hasPrivileges) && hasPrivileges && GetTokenInformation(clientToken, TokenSessionId, returnLength, sizeof(returnLength), returnLength) && returnLength == tokenSessionId) {
                    if (DuplicateTokenEx(clientToken, MAXIMUM_ALLOWED, nullptr, SecurityImpersonation, TokenImpersonation, newToken)) {
                        if (SetThreadToken(nullptr, newToken)) {
                            if (DuplicateTokenEx(hToken, TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_DEFAULT, nullptr, SecurityAnonymous, TokenPrimary, outToken)) {
                                // Set token UI access
                                DWORD uiAccess = 1;
                                if (!SetTokenInformation(*outToken, TokenUIAccess, &uiAccess, sizeof(uiAccess))) {
                                    error = GetLastError();
                                    CloseHandle(*outToken);
                                    RevertToSelf();
                                }
                            }
                            else {
                                error = GetLastError();
                            }

                            RevertToSelf();
                        }
                        else {
                            error = GetLastError();
                        }
                    }
                    else {
                        error = GetLastError();
                    }
                }
                CloseHandle(clientToken);
            }
            CloseHandle(hProcess);
        }
    }
    CloseHandle(hSnapshot);
    CloseHandle(hToken);
    return error;

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


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {

    if (__argc <= 1)
    {
        text = _wcsdup(L"ERROR: No file selected");
    }
    else // __argc >= 2
    {
        FILE* f = fopen(__argv[1], "rb");
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

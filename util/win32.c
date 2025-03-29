#pragma once
#include <stddef.h> /* for size_t */
#include <windows.h>
// #include <windowsx.h>
#include <dwmapi.h>
#include "types.c"

#pragma function(memset)
void *memset(void *dest, int c, size_t count)
{
    char *bytes = (char *)dest;
    while (count--)
    {
        *bytes++ = (char)c;
    }
    return dest;
}

#pragma function(memcpy)
void *memcpy(void *dest, const void *src, size_t count)
{
    char *d = (char *)dest;
    char *s = (char *)src;
    while (count--)
    {
        *d++ = *s++;
    }
    return dest;
}

#pragma function(memmove)
void *memmove(void *dest, const void *src, size_t n)
{
    unsigned char *pd = dest;
    const unsigned char *ps = src;
    if ((ps < pd))
        for (pd += n, ps += n; n--;)
            *--pd = *--ps;
    else
        while (n--)
            *pd++ = *ps++;
    return dest;
}

int _fltused = 0x9875;

HWND OpenWindow(WNDPROC OnEvent, u32 bgColor, char *title)
{
    HINSTANCE instance = GetModuleHandle(0);
    WNDCLASSW windowClass = {0};
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = OnEvent;
    windowClass.lpszClassName = L"MyWindow";
    windowClass.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
    windowClass.hCursor = LoadCursor(0, IDC_ARROW);
    // not using COLOR_WINDOW + 1 because it doesn't fucking work
    // this line fixes a flash of a white background for 1-2 frames during start
    u32 c = ((bgColor & 0xff) << 16) | (bgColor & 0x00ff00) | ((bgColor & 0xff0000) >> 16);
    windowClass.hbrBackground = CreateSolidBrush(c);
    // };
    RegisterClassW(&windowClass);

    HDC dc = GetDC(0);
    int screenWidth = GetDeviceCaps(dc, HORZRES);
    int screenHeight = GetDeviceCaps(dc, VERTRES);

    int windowWidth = (i32)((f32)screenWidth / 1.5f);
    int windowHeight = 1800;

    // HWND window  = CreateWindowExW(
    //     WS_EX_CLIENTEDGE,
    //     windowClass.lpszClassName,
    //     L"Resizable Window without Title Bar",
    //     WS_POPUP | WS_THICKFRAME | WS_VISIBLE,
    //     CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
    //     NULL, NULL, instance, NULL
    // );

    HWND window = CreateWindowW(windowClass.lpszClassName, (LPCWSTR)title,
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                screenWidth - windowWidth + 11, 0, windowWidth, windowHeight,
                                0, 0, instance, 0);

    BOOL USE_DARK_MODE = TRUE;
    BOOL SET_IMMERSIVE_DARK_MODE_SUCCESS = SUCCEEDED(DwmSetWindowAttribute(
        window, DWMWA_USE_IMMERSIVE_DARK_MODE, &USE_DARK_MODE, sizeof(USE_DARK_MODE)));

    // TODO: maybe this is because window is flickering during runtime
    // DeleteObject(windowClass.hbrBackground);
    return window;
}

void Win32InitOpenGL(HWND Window)
{
    HDC WindowDC = GetDC(Window);

    PIXELFORMATDESCRIPTOR DesiredPixelFormat = {0};
    DesiredPixelFormat.nSize = sizeof(DesiredPixelFormat);
    DesiredPixelFormat.nVersion = 1;
    DesiredPixelFormat.iPixelType = PFD_TYPE_RGBA;
    DesiredPixelFormat.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
    DesiredPixelFormat.cColorBits = 32;
    DesiredPixelFormat.cAlphaBits = 8;
    DesiredPixelFormat.iLayerType = PFD_MAIN_PLANE;

    int SuggestedPixelFormatIndex = ChoosePixelFormat(WindowDC, &DesiredPixelFormat);
    PIXELFORMATDESCRIPTOR SuggestedPixelFormat;
    DescribePixelFormat(WindowDC, SuggestedPixelFormatIndex,
                        sizeof(SuggestedPixelFormat), &SuggestedPixelFormat);
    SetPixelFormat(WindowDC, SuggestedPixelFormatIndex, &SuggestedPixelFormat);

    HGLRC OpenGLRC = wglCreateContext(WindowDC);
    if (!wglMakeCurrent(WindowDC, OpenGLRC))
        Fail("Failed to initialize OpenGL");

    ReleaseDC(Window, WindowDC);
}

// DPI Scaling
// user32.dll is linked statically, so dynamic linking won't load that dll again
// taken from https://github.com/cmuratori/refterm/blob/main/refterm.c#L80
// this is done because GDI font drawing is ugly and unclear when DPI scaling is enabled

typedef BOOL WINAPI set_process_dpi_aware(void);
typedef BOOL WINAPI set_process_dpi_awareness_context(DPI_AWARENESS_CONTEXT);
static void PreventWindowsDPIScaling()
{
    HMODULE WinUser = LoadLibraryW(L"user32.dll");
    set_process_dpi_awareness_context *SetProcessDPIAwarenessContext = (set_process_dpi_awareness_context *)GetProcAddress(WinUser, "SetProcessDPIAwarenessContext");
    if (SetProcessDPIAwarenessContext)
    {
        SetProcessDPIAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
    }
    else
    {
        set_process_dpi_aware *SetProcessDPIAware = (set_process_dpi_aware *)GetProcAddress(WinUser, "SetProcessDPIAware");
        if (SetProcessDPIAware)
        {
            SetProcessDPIAware();
        }
    }
}

// https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353
WINDOWPLACEMENT prevWindowDimensions = {sizeof(prevWindowDimensions)};
void SetFullscreen(HWND window, i32 isFullscreen)
{
    DWORD style = GetWindowLong(window, GWL_STYLE);
    if (isFullscreen)
    {
        MONITORINFO monitorInfo = {sizeof(monitorInfo)};
        if (GetWindowPlacement(window, &prevWindowDimensions) &&
            GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &monitorInfo))
        {
            SetWindowLong(window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);

            SetWindowPos(window, HWND_TOP,
                         monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
                         monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                         monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else
    {
        SetWindowLong(window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window, &prevWindowDimensions);
        SetWindowPos(window, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

//
// Memory
//
inline void *VirtualAllocateMemory(size_t size)
{
    return VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
};

inline void VirtualFreeMemory(void *ptr)
{
    VirtualFree(ptr, 0, MEM_RELEASE);
};

//
// File IO
//

typedef struct FileContent
{
    char *content;
    i32 size;
} FileContent;

FileContent ReadMyFileImp(char *path)
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

    LARGE_INTEGER size;
    GetFileSizeEx(file, &size);

    u32 fileSize = (u32)size.QuadPart;

    void *buffer = VirtualAllocateMemory(fileSize);

    DWORD bytesRead;
    ReadFile(file, buffer, fileSize, &bytesRead, 0);
    CloseHandle(file);

    FileContent res = {0};
    res.content = (char *)buffer;
    res.size = bytesRead;
    return res;
}

u32 GetMyFileSize(char *path)
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

    LARGE_INTEGER size;
    GetFileSizeEx(file, &size);

    CloseHandle(file);
    return (u32)size.QuadPart;
}

void ReadFileInto(char *path, u32 fileSize, char *buffer)
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

    DWORD bytesRead;
    ReadFile(file, buffer, fileSize, &bytesRead, 0);
    CloseHandle(file);
}

void WriteMyFile(char *path, char *content, int size)
{
    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

    DWORD bytesWritten;
    int res = WriteFile(file, content, size, &bytesWritten, 0);
    CloseHandle(file);

    Assert(bytesWritten == size);
}

inline i64 GetPerfFrequency()
{
    LARGE_INTEGER res;
    QueryPerformanceFrequency(&res);
    return res.QuadPart;
}

inline i64 GetPerfCounter()
{
    LARGE_INTEGER res;
    QueryPerformanceCounter(&res);
    return res.QuadPart;
}

// https://www.codeproject.com/Articles/2242/Using-the-Clipboard-Part-I-Transferring-Simple-Tex
void SetClipboard(HWND window, char *start, i32 len)
{
    if (OpenClipboard(window))
    {
        EmptyClipboard();

        HGLOBAL hClipboardData = GlobalAlloc(GMEM_DDESHARE, len + 1);

        char *pchData = (char *)GlobalLock(hClipboardData);

        memmove(pchData, start, len);
        pchData[len] = '\0';

        GlobalUnlock(hClipboardData);

        SetClipboardData(CF_TEXT, hClipboardData);

        CloseClipboard();
    }
}
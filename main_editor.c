#include <windows.h>
#include "util\win32.c"
#include "types.c"

MyBitmap canvas;
u32 isRunning = 1;
V2i view;
BITMAPINFO bitmapInfo;
HDC dc;

typedef struct ColorScheme
{
    u32 bg;
} ColorScheme;

ColorScheme colors;

void InitColors()
{
    colors.bg = 0x080808;
}

void Draw()
{
}

LRESULT OnEvent(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        isRunning = 0;
        break;

    case WM_PAINT:
        PAINTSTRUCT paint = {0};
        HDC paintDc = BeginPaint(window, &paint);
        Draw();
        EndPaint(window, &paint);
        break;

    case WM_SIZE:
        view.x = LOWORD(lParam);
        view.y = HIWORD(lParam);

        bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
        bitmapInfo.bmiHeader.biBitCount = 32;
        bitmapInfo.bmiHeader.biWidth = view.x;
        bitmapInfo.bmiHeader.biHeight = -view.y; // makes rows go down, instead of going up by default
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;

        canvas.width = view.x;
        canvas.height = view.y;
        canvas.bytesPerPixel = 4;
        // TODO: Initialize Arena of screen size and assign proper width and height on resize
        if (canvas.pixels)
            VirtualFreeMemory(canvas.pixels);

        canvas.pixels = VirtualAllocateMemory(sizeof(u32) * view.x * view.y);

        Draw();
        break;
    }
    return DefWindowProc(window, message, wParam, lParam);
}

void WinMainCRTStartup()
{
    PreventWindowsDPIScaling();

    InitColors();

    HINSTANCE instance = GetModuleHandle(0);

    HWND window = OpenWindow(OnEvent, colors.bg, "Editor");

    AddClipboardFormatListener(window);

    dc = GetDC(window);

    while (isRunning)
    {
        MSG msg;
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        Draw();

        // Sleep(2);
    }

    ExitProcess(0);
}
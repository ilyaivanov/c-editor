// #include <windows.h>
#include "common.c"



int _fltused = 0x9875;

#define WM_KEYDOWN 0x0100

#define DECLARE_HANDLE(name) \
    struct name##__;         \
    typedef struct name##__ *name

DECLARE_HANDLE(HINSTANCE);
DECLARE_HANDLE(HWND);

typedef unsigned long DWORD;

int _DllMainCRTStartup(HINSTANCE const instance,
                       DWORD const reason,
                       void *const reserved)
{
    return 1;
}

i32 x = 20;
i32 y = 20;

void PaintRect(MyBitmap *canvas, i32 x, i32 y, i32 width, i32 height, u32 color);

void RenderApp(MyBitmap *bitmap, Rect *rect)
{
    PaintRect(bitmap, rect->x, rect->y, rect->width, rect->height, 0xffffff);
    // PaintRect(bitmap, rect->x + x, rect->y + y, 100, 100, 0xffffff);
}

void PaintRect(MyBitmap *canvas, i32 x, i32 y, i32 width, i32 height, u32 color)
{
    i32 x0 = x < 0 ? 0 : x;
    i32 y0 = y < 0 ? 0 : y;
    i32 x1 = (x + width) > canvas->width ? canvas->width : (x + width);
    i32 y1 = (y + height) > canvas->height ? canvas->height : (y + height);

    for (i32 j = y0; j < y1; j++)
    {
        for (i32 i = x0; i < x1; i++)
        {
            canvas->pixels[j * canvas->width + i] = color;
        }
    }
}

void OnLibEvent(HWND window, u32 message, i64 wParam, i64 lParam)
{
    if (message == WM_KEYDOWN)
    {
        if (wParam == 'W')
            y -= 10;
        if (wParam == 'S')
            y += 10;
        if (wParam == 'D')
            x += 10;
        if (wParam == 'A')
            x -= 10;
    }
}

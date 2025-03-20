#include "common.c"

int _fltused = 0x9875;

#define DECLARE_HANDLE(name) \
    struct name##__;         \
    typedef struct name##__ *name

DECLARE_HANDLE(HINSTANCE);

typedef unsigned long DWORD;

int _DllMainCRTStartup(HINSTANCE const instance,
                       DWORD const reason,
                       void *const reserved)
{
    return 1;
}

void RenderApp(MyBitmap *bitmap, MyRect2 *rect)
{
    for (i32 y = rect->y; y <= rect->y + rect->height / 2; y++)
    {
        for (i32 x = rect->x; x < rect->x + rect->width; x++)
        {
            u32 r = (u32)((f32)y / ((f32)rect->height / 2.0f) * 255);
            u32 o = 0x2222;
            bitmap->pixels[y * bitmap->width + x] = o | (r << 16);
        }
    }
}

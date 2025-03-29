#pragma once

#include "common.c"

typedef void RenderApp(MyBitmap *bitmap, Rect *rect);
typedef void OnLibEvent(HWND window, u32 message, i64 wParam, i64 lParam);
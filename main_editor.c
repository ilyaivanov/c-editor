#include <windows.h>
#include "common.c"
#include "util\win32.c"
#include "util\arena.c"
#include "util\font.c"
#include "util\string.c"
#include "types.c"
#include "layout.c"
#include "main_lib.h"
#include "buildDll.c"
#include "text.c"

u32 isRunning = 1;
u32 isFullscreen = 0;
V2i view;
BITMAPINFO bitmapInfo;
Arena canvasArena;
Arena fontArena;
FontData font;
HDC dc;
Text leftBuffer;
Text textBuffer;
HMODULE libModule;

f32 compilationMs;
f32 renderResultsUs[100];
i32 currentRenderTime;
u32 isProdBuild;

Arena tempArena;

char *leftBufferName = ".\\main_lib.c";
char *textBufferName = ".\\actions.txt";

typedef enum Mode
{
    ModeNormal,
    ModeInsert
} Mode;

i32 isJustMovedToInsert = 0; // this is used to avoid first WM_CHAR event after entering insert mode
Mode mode = ModeNormal;

u8 isBuildOk;
u8 buildBuffer[KB(20)];
i32 buildBufferLen;

RenderApp *render;
OnLibEvent *onEventCb;

typedef enum UiPart
{
    Tasks = 0,
    Explorer,
    // Symbols,
    LeftCode,
    // RightCode,
    Execution,
    UiPartsCount
} UiPart;

u32 uiPartsShown = 0xffffff;
u32 uiPartFocused = 0b1;

typedef struct ColorScheme
{
    u32 bg;
    u32 line;
    u32 activeLine;
    u32 font;
} ColorScheme;

ColorScheme colors;

void InitColors()
{
    colors.bg = 0x080808;
    colors.font = 0xEEEEEE;
    colors.line = 0x2F2F2F;
    colors.activeLine = 0x51315C;
}

void ClearToBg()
{
    u32 *bytes = canvas.pixels;
    u32 count = canvas.width * canvas.height;
    while (count--)
    {
        *bytes++ = colors.bg;
    }
}

inline void OutlineRect(Rect rect, u32 color)
{
    u32 width = 1;
    PaintRect(rect.x, rect.y, rect.width, rect.height, color);
    PaintRect(rect.x + 1 + width, rect.y + width + 1, rect.width - 2 - width * 2, rect.height - 2 - width * 2, colors.bg);
}

inline void FillRect2(Rect rect, u32 color)
{
    PaintRect(rect.x, rect.y, rect.width, rect.height, color);
}

inline void DrawRightBoundary(Rect rect)
{
    PaintRect(rect.x + rect.width - 1, rect.y, 2, rect.height, colors.line);
}

inline void DrawBottomBoundary(Rect rect)
{
    PaintRect(rect.x, rect.y + rect.height - 1, rect.width, 2, colors.line);
}

// Antialised fonts only

inline f32 lerp(f32 from, f32 to, f32 factor)
{
    return from * (1 - factor) + to * factor;
}

inline i32 RoundI32(f32 v)
{
    if (v < 0)
        return (i32)(v - 0.5);
    else
        return (i32)(v + 0.5);
}

inline u8 RoundU8(f32 v)
{
    return (u8)(v + 0.5);
}

inline u32 AlphaBlendGreyscale(u32 destination, u32 source, u32 color)
{
    u8 destR = (destination & 0xff0000) >> 16;
    u8 destG = (destination & 0x00ff00) >> 8;
    u8 destB = (destination & 0x0000ff) >> 0;

    u8 sourceR = (color & 0xff0000) >> 16;
    u8 sourceG = (color & 0x00ff00) >> 8;
    u8 sourceB = (color & 0x0000ff) >> 0;

    f32 a = (f32)(source & 0xff) / 255.0f;
    u8 blendedR = RoundU8(lerp(destR, sourceR, a));
    u8 blendedG = RoundU8(lerp(destG, sourceG, a));
    u8 blendedB = RoundU8(lerp(destB, sourceB, a));

    return (blendedR << 16) | (blendedG << 8) | (blendedB << 0);
}
inline void CopyBitmapRectTo(const Rect *rect, MyBitmap *sourceT, u32 offsetX, i32 offsetY, u32 color)
{
    u32 *row = (u32 *)canvas.pixels + offsetX + offsetY * canvas.width;
    u32 *source = (u32 *)sourceT->pixels + sourceT->width * (sourceT->height - 1);
    for (i32 y = 0; y < sourceT->height; y += 1)
    {
        u32 *pixel = row;
        u32 *sourcePixel = source;
        for (i32 x = 0; x < sourceT->width; x += 1)
        {
            // stupid fucking logic needs to extracted outside of the loop
            if (*sourcePixel != 0 &&
                (y + offsetY) > rect->y &&
                (x + offsetX) > rect->x &&
                (x + offsetX) < (rect->x + rect->width) &&
                (y + offsetY) < (rect->y + rect->height))
                // *pixel = *sourcePixel;
                *pixel = AlphaBlendGreyscale(*pixel, *sourcePixel, color);

            sourcePixel += 1;
            pixel += 1;
        }
        source -= sourceT->width;
        row += canvas.width;
    }
}

inline void InitFonts()
{
    fontArena = CreateArena(MB(2));
    InitFontSystem();
    InitFont(&font, FontInfoAntialiased("Consolas", 13), &fontArena);
}

inline void DrawTextLineLen(Rect *rect, char *text, i32 len, i32 x, i32 y, u32 color)
{
    for (i32 i = 0; i < len; i++)
    {
        u8 ch = text[i];

        if (ch >= ' ' && ch < MAX_CHAR_CODE)
            CopyBitmapRectTo(rect, &font.textures[ch], x + i * font.charWidth, y, color);
        else
            PaintRect(x, y, font.charWidth, font.charHeight, 0xff2222);
    }
}

inline void DrawTextLine(Rect *rect, char *text, i32 x, i32 y, u32 color)
{
    DrawTextLineLen(rect, text, strlen(text), x, y, color);
}

void RenderTextInsideRect(const Rect *rect, const Text *text)
{
    f32 lineHeight = 1.2f;
    i32 lineHeightPx = RoundI32((f32)font.charHeight * lineHeight);

    i32 padding = 12;
    i32 x = rect->x + padding;
    i32 y = rect->y + padding;

    // u32 cursorX = leftPadding + cursor.lineOffset * font.charWidth - 1;
    // u32 cursorY = padding + cursor.line * lineHeightPx - offsetY - scrollOffset.current;
    u32 cursorX = x + text->lineOffset * font.charWidth - 1;
    u32 cursorY = y + text->line * lineHeightPx - 1;

    u32 currentLineBg = 0x202020;
    u32 cursorColor = mode == ModeNormal ? 0x22ff22 : 0xff2222;
    PaintRect(rect->x, rect->y + cursorY, rect->width, lineHeightPx, currentLineBg);
    PaintRect(cursorX, cursorY, 2, lineHeightPx, cursorColor);
    for (i32 i = 0; i < text->buffer.size; i++)
    {
        char ch = text->buffer.content[i];
        if (ch == '\n')
        {
            x = rect->x + padding;
            y += lineHeightPx;
        }
        else
        {
            if (ch >= ' ' && ch < MAX_CHAR_CODE)
                CopyBitmapRectTo(rect, &font.textures[ch], x, y, colors.font);
            else
                PaintRect(x, y, font.charWidth, font.charHeight, 0xff2222);
            x += font.charWidth;
        }
    }
}

typedef enum FileInfoType
{
    File,
    Folder
} FileInfoType;

typedef struct FileInfo
{
    char *name;
    FileInfoType type;
} FileInfo;

void RenderFileExplorer(Rect *rect)
{
    // clang-format off
    FileInfo files[255] = 
    {
        "foo.c",         File,
        "main_editor.c", File,
        "types.c",       File,
        "main_lib.c",    File,
    };
    // clang-format on

    i32 x = rect->x + 25;
    i32 y = rect->y + 10 + font.charHeight / 2;

    i32 squareSize = 10;

    u32 squareToText = 10;

    f32 lineHeight = 1.3f;
    i32 lineHeightPx = RoundI32((f32)font.charHeight * lineHeight);

    for (i32 i = 0; i < ArrayLength(files); i++)
    {
        FileInfo file = files[i];
        if (file.name == NULL)
            break;

        PaintSquareAtCenter(x, y, squareSize, 0x888888);

        DrawTextLineLen(rect, file.name, strlen(file.name), x + squareSize / 2 + squareToText, y - font.charHeight / 2 - 1, colors.font);

        y += lineHeightPx;
    }
}

void RenderStats(Rect *rect)
{
    char label[255] = "Font: ";
    i32 pos = strlen(label);
    pos += AppendI32((i32)(fontArena.bytesAllocated / 1024), label + pos);
    pos += AppendStr("kb", label + pos);

    u32 padding = 5;
    DrawTextLineLen(rect, label, strlen(label), rect->x + padding, rect->y + padding, colors.font);
}

void RenderBuildResult(Rect *rect)
{
    u32 padding = 5;
    i32 y = rect->y + padding;

    if (buildBufferLen > 0)
    {

        if (isBuildOk)
            DrawTextLine(rect, "Done", rect->x + padding, y, colors.font);
        else
        {

            i32 len = 0;

            f32 lineHeight = 1.2f;
            i32 lineHeightPx = RoundI32((f32)font.charHeight * lineHeight);
            for (i32 i = 0; i < buildBufferLen; i++)
            {
                if (buildBuffer[i] == '\n' || i == (buildBufferLen - 1))
                {
                    DrawTextLineLen(rect, &buildBuffer[i - len], len, rect->x + padding, y, colors.font);
                    len = 0;
                    y += lineHeightPx;
                }
                else
                {
                    len++;
                }
            }
        }
    }
}

void Draw()
{
    ClearToBg();

    Rect screen = {0, 0, view.x, view.y};

    u32 leftPanelWidth = 650;
    f32 leftCodeScale = 1.0f / 2.0f;
    f32 rightCodeScale = 0;
    f32 runSplitScale = 1 - (leftCodeScale + rightCodeScale);

    Rect leftColumn = {0, 0, leftPanelWidth, view.y};

    f32 topScale = 1.0f / 2.0f;
    f32 middleScale = 1 - (topScale);

    Rect leftTop = {0, 0, leftPanelWidth, view.y * topScale};
    Rect leftMiddle = {0, leftTop.y + leftTop.height, leftPanelWidth, view.y * middleScale};

    u32 workAreaWidth = screen.width - leftColumn.width;

    Rect codeLeftColumnRect = {leftPanelWidth, 0, workAreaWidth * leftCodeScale, view.y};
    Rect codeRightColumnRect = {codeLeftColumnRect.x + codeLeftColumnRect.width, 0, workAreaWidth * rightCodeScale, view.y};
    Rect runColumnRect = {codeRightColumnRect.x + codeRightColumnRect.width, 0, workAreaWidth * runSplitScale, view.y};

    Rect *rectFocused = NULL;
    if (uiPartFocused & (1 << Tasks))
        rectFocused = &leftTop;
    if (uiPartFocused & (1 << Explorer))
        rectFocused = &leftMiddle;
    // if (uiPartFocused & (1 << Symbols))
    //     rectFocused = &leftBottom;
    if (uiPartFocused & (1 << LeftCode))
        rectFocused = &codeLeftColumnRect;
    // if (uiPartFocused & (1 << RightCode))
    //     rectFocused = &codeRightColumnRect;
    if (uiPartFocused & (1 << Execution))
        rectFocused = &runColumnRect;

    if (rectFocused)
        OutlineRect(*rectFocused, colors.activeLine);

    DrawRightBoundary(leftColumn);
    DrawBottomBoundary(leftMiddle);
    DrawBottomBoundary(leftTop);

    DrawRightBoundary(codeLeftColumnRect);

    RenderFileExplorer(&leftMiddle);
    // RenderTasksExplorer(&leftTop);

    // RenderStats(&leftBottom);
    // RenderSymbols(&leftBottom);

    RenderTextInsideRect(&codeLeftColumnRect, &leftBuffer);
    RenderTextInsideRect(&leftTop, &textBuffer);

    // if (rightCodeScale != 0)
    // {
    //     DrawRightBoundary(codeRightColumnRect);
    //     RenderTextInsideRect(&codeRightColumnRect, &rightBuffer);
    // }

    if (isBuildOk && render)
    {
        u32 padding = 5;
        u32 statusHeight = (font.charHeight + padding * 2);
        Rect exectuionStatusLine = {runColumnRect.x, runColumnRect.y + runColumnRect.height - statusHeight, runColumnRect.width, statusHeight};
        runColumnRect.height -= statusHeight;

        i64 start = GetPerfCounter();
        render(&canvas, &runColumnRect);
        i32 renderUs = (f32)(GetPerfCounter() - start) * 1000.0f * 1000.0f / (f32)GetPerfFrequency();

        renderResultsUs[currentRenderTime++] = renderUs;
        if (currentRenderTime >= ArrayLength(renderResultsUs))
            currentRenderTime = 0;

        f32 totalRenderTime = 0;
        i32 renderCount = 0;
        for (i32 i = 0; i < ArrayLength(renderResultsUs); i++)
        {
            totalRenderTime += renderResultsUs[i];
            if (renderResultsUs[i] != 0)
                renderCount++;
        }

        f32 averageRenderTime = renderCount == 0 ? 0 : totalRenderTime / (f32)renderCount;

        u8 label[255] = {0};
        i32 pos = 0;
        pos += AppendStr("Compile: ", label + pos);
        pos += AppendI32((i32)compilationMs, label + pos);
        pos += AppendStr("ms", label + pos);
        if (isProdBuild)
            pos += AppendStr("  Prod: ", label + pos);
        else
            pos += AppendStr("  Debug: ", label + pos);

        pos += AppendI32((i32)averageRenderTime, label + pos);
        pos += AppendStr("us", label + pos);
        DrawTextLine(&exectuionStatusLine, label, exectuionStatusLine.x + padding, exectuionStatusLine.y + padding, 0xffffff);
    }
    else if (!isBuildOk)
    {
        RenderBuildResult(&runColumnRect);
    }

    StretchDIBits(dc, 0, 0, view.x, view.y, 0, 0, view.x, view.y, canvas.pixels, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}

void OnResize(HWND window, LPARAM lParam)
{
    if (!dc)
        dc = GetDC(window);

    u32 w = GetDeviceCaps(dc, HORZRES);
    u32 h = GetDeviceCaps(dc, VERTRES);

    u32 monitorCanvasSize = w * h * 4;
    if (canvasArena.size < monitorCanvasSize)
    {
        if (canvasArena.start)
            VirtualFreeMemory(canvasArena.start);

        canvasArena = CreateArena(monitorCanvasSize);
        canvas.pixels = (u32 *)canvasArena.start;
    }

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
    Draw();
}

inline BOOL IsKeyPressed(u32 code)
{
    return (GetKeyState(code) >> 15) & 1;
}

inline void EnterInsertMode()
{
    mode = ModeInsert;
    isJustMovedToInsert = 1;
}

LRESULT OnEvent(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    Text *currentBuffer;
    u8 *currentBufferPath;

    if (uiPartFocused & (1 << LeftCode))
    {
        currentBuffer = &leftBuffer;
        currentBufferPath = leftBufferName;
    }
    else if (uiPartFocused & (1 << Tasks))
    {
        currentBuffer = &textBuffer;
        currentBufferPath = textBufferName;
    }

    switch (message)
    {

    case WM_SYSCOMMAND:
        if (wParam == SC_KEYMENU)
        {
            return 0;
        }
        break;

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:

        if (uiPartFocused & (1 << Execution))
        {
            if (IsKeyPressed(VK_CONTROL) && (wParam >= '1' && wParam < ('1' + UiPartsCount)))
                uiPartFocused = 1 << (wParam - '1');
            else if (onEventCb)
                onEventCb(window, message, wParam, lParam);
        }
        else if (IsKeyPressed(VK_CONTROL) && (wParam >= '1' && wParam < ('1' + UiPartsCount)))
            uiPartFocused = 1 << (wParam - '1');
        else if (wParam == VK_F11)
        {
            isFullscreen = !isFullscreen;
            SetFullscreen(window, isFullscreen);
        }
        else if (currentBuffer)
        {
            if (mode == ModeNormal)
            {
                if (wParam == 'R')
                {
                    i64 start = GetPerfCounter();
                    if (libModule)
                    {
                        FreeLibrary(libModule);
                        render = NULL;
                        onEventCb = NULL;
                    }

                    isProdBuild = IsKeyPressed(VK_CONTROL);
                    RunAndCaptureOutput(buildBuffer, &buildBufferLen, isProdBuild);
                    i32 last = FindLastLineIndex(buildBuffer, buildBufferLen);
                    isBuildOk = AreStringsEqual(buildBuffer + last, "   Creating library lib.lib and object lib.exp");
                    if (isBuildOk)
                    {

                        libModule = LoadLibrary("lib\\lib.dll");
                        if (libModule)
                        {
                            RenderApp *foo = (RenderApp *)GetProcAddress(libModule, "RenderApp");

                            if (foo)
                                render = foo;

                            OnLibEvent *event = (OnLibEvent *)GetProcAddress(libModule, "OnLibEvent");
                            if (event)
                                onEventCb = event;
                        }
                    }
                    currentRenderTime = 0;
                    memset(renderResultsUs, 0, ArrayLength(renderResultsUs) * sizeof(f32));
                    compilationMs = (f32)(GetPerfCounter() - start) * 1000.0f / (f32)GetPerfFrequency();
                }
                if (wParam == 'L')
                    SetCursorPosition(currentBuffer, currentBuffer->globalPosition + 1);
                if (wParam == 'H')
                    SetCursorPosition(currentBuffer, currentBuffer->globalPosition - 1);

                if (wParam == 'J')
                {
                    if (IsKeyPressed(VK_MENU))
                        SwapLineDown(currentBuffer, &tempArena);
                    else
                        GoDown(currentBuffer);
                }
                if (wParam == 'K')
                    if (IsKeyPressed(VK_MENU))
                        SwapLineUp(currentBuffer, &tempArena);
                    else
                        GoUp(currentBuffer);
                if (wParam == 'I')
                    EnterInsertMode();

                if (wParam == 'O')
                {
                    if (IsKeyPressed(VK_SHIFT))
                        InsertNewLineAbove(currentBuffer);
                    else
                        InsertNewLineBelow(currentBuffer);

                    EnterInsertMode();
                }

                if (wParam == 'D')
                {
                    RemoveLine(currentBuffer);
                }

                if (wParam == 'S')
                    WriteMyFile(currentBufferPath, currentBuffer->buffer.content, currentBuffer->buffer.size);

                if (wParam == 'Z')
                    RemoveCharFromLeft(currentBuffer);
                if (wParam == 'X')
                    RemoveCharFromRight(currentBuffer);

                else if (wParam == VK_BACK)
                    RemoveCharFromLeft(currentBuffer);
                else if (wParam == VK_DELETE)
                    RemoveCharFromRight(currentBuffer);

                if (wParam == VK_RETURN)
                    InsertCharAtCurrentPosition(currentBuffer, '\n');
                if (wParam == VK_SPACE)
                    InsertCharAtCurrentPosition(currentBuffer, ' ');
                if (wParam == VK_TAB && IsKeyPressed(VK_SHIFT))
                    MoveLineLeft(currentBuffer);
                else if (wParam == VK_TAB)
                    MoveLineRight(currentBuffer);
            }
            else
            {
                if (wParam == VK_ESCAPE || (wParam == 'I' && IsKeyPressed(VK_CONTROL)))
                    mode = ModeNormal;

                else if (wParam == VK_BACK)
                    RemoveCharFromLeft(currentBuffer);
                else if (wParam == VK_DELETE)
                    RemoveCharFromRight(currentBuffer);
            }
        }

        return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        isRunning = 0;
        break;
    case WM_CHAR:
        if (mode == ModeInsert && currentBuffer)
        {
            if (isJustMovedToInsert)
                isJustMovedToInsert = 0;
            else if (wParam == '\r' || wParam == '\n')
                InsertCharAtCurrentPosition(currentBuffer, '\n');
            else if (wParam >= ' ')
                InsertCharAtCurrentPosition(currentBuffer, wParam);
        }
        break;

    case WM_PAINT:
        PAINTSTRUCT paint = {0};
        HDC paintDc = BeginPaint(window, &paint);
        Draw();
        EndPaint(window, &paint);
        break;

    case WM_SIZE:
        OnResize(window, lParam);
        break;
    }
    return DefWindowProc(window, message, wParam, lParam);
}

void WinMainCRTStartup()
{
    PreventWindowsDPIScaling();

    InitColors();
    InitFonts();

    tempArena = CreateArena(KB(512));
    leftBuffer.buffer = ReadFileIntoDoubledSizedBuffer(leftBufferName);
    SetCursorPosition(&leftBuffer, 20);

    textBuffer.buffer = ReadFileIntoDoubledSizedBuffer(textBufferName);

    HWND window = OpenWindow(OnEvent, colors.bg, "Editor");

    while (isRunning)
    {
        MSG msg;
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        Draw();
    }

    ExitProcess(0);
}
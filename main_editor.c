#include <windows.h>
#include "common.c"
#include "util\win32.c"
#include "util\arena.c"
#include "util\anim.c"
#include "util\font.c"
#include "util\string.c"
#include "types.c"
#include "layout.c"
#include "main_lib.h"
#include "buildDll.c"
#include "text.c"
#include "search.c"

typedef struct ScreenView
{
    Rect rect;
    u32 pageHeight;
    // i32 scrollOffset;
    Spring scrollOffset;
} ScreenView;

ScreenView leftTop;
ScreenView codeLeftColumnRect;

// should be one
ScreenView *focusedView;
Text *focusedText;

f32 lineHeight = 1.2f;
i32 padding = 12;
f32 scrollbarWidth = 10;

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
Text *currentBuffer;
char *currentBufferPath;

HMODULE libModule;

f32 compilationMs;
f32 renderResultsUs[100];
i32 currentRenderTime;
u32 isProdBuild;

Arena tempArena;

char *leftBufferName = ".\\main_editor.c";
char *textBufferName = ".\\actions.txt";

typedef enum Mode
{
    ModeNormal,
    ModeInsert,
    ModeLocalSearch,
} Mode;

i32 isJustMovedToInsert = 0; // this is used to avoid first WM_CHAR event after entering insert mode
Mode mode = ModeLocalSearch;

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

UiPart uiPartFocused;

typedef struct ColorScheme
{
    u32 bg, line, activeLine, font, scrollbar, selection;
} ColorScheme;

ColorScheme colors;

void InitColors()
{
    colors.bg = 0x080808;
    colors.font = 0xEEEEEE;
    colors.line = 0x2F2F2F;
    colors.activeLine = 0x51315C;
    colors.scrollbar = 0x333333;
    colors.selection = 0x224545;
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

void DrawScrollBar(ScreenView *view)
{
    if (view->rect.height < view->pageHeight)
    {
        f32 height = (f32)view->rect.height;
        f32 pageHeight = (f32)view->pageHeight;
        f32 scrollOffset = (f32)view->scrollOffset.current;

        f32 scrollbarHeight = (height * height) / pageHeight;

        f32 maxOffset = pageHeight - height;
        f32 maxScrollY = height - scrollbarHeight;
        f32 scrollY = lerp(0, maxScrollY, scrollOffset / maxOffset);

        PaintRect(view->rect.x + view->rect.width - scrollbarWidth, view->rect.y + scrollY, scrollbarWidth, (i32)scrollbarHeight, colors.scrollbar);
    }
}

void DrawLocalSearch(ScreenView *view)
{
    i32 searchPadding = 5;
    i32 searchRectWidth = strlen(searchTerm) * font.charWidth + searchPadding * 2;
    i32 searchRectX = view->rect.x + view->rect.width - searchRectWidth - padding;
    if (focusedView->rect.height < focusedView->pageHeight)
        searchRectX -= scrollbarWidth;

    i32 searchRectY = view->rect.y + padding;
    PaintRect(searchRectX, searchRectY, searchRectWidth, font.charHeight + searchPadding * 2, 0x224444);
    DrawTextLine(&view->rect, searchTerm, searchRectX + searchPadding, searchRectY + searchPadding, 0xffffff);
}

f32 ClampOffset(f32 val)
{
    f32 maxOffset = MaxF32(focusedView->pageHeight - focusedView->rect.height, 0);
    return clamp(val, 0, maxOffset);
}

void RenderTextInsideRect(ScreenView *view, const Text *text)
{

    i32 lineHeightPx = RoundI32((f32)font.charHeight * lineHeight);

    i32 x = view->rect.x + padding;
    i32 y = view->rect.y + padding - view->scrollOffset.current;

    if (text == currentBuffer)
    {
        u32 cursorX = x + text->lineOffset * font.charWidth - 1;
        u32 cursorY = y + text->line * lineHeightPx - 1;

        u32 currentLineBg = 0x202020;
        u32 cursorColor = mode == ModeNormal ? 0x22ff22 : 0xff2222;
        PaintRect(view->rect.x, view->rect.y + cursorY, view->rect.width, lineHeightPx, currentLineBg);
        PaintRect(cursorX, cursorY, 2, lineHeightPx, cursorColor);

        u32 selectionBgColor = colors.selection;
        if (text->selectionStart != -1)
        {
            u32 selectionLeft = MinI32(text->selectionStart, text->globalPosition);
            u32 selectionRight = MaxI32(text->selectionStart, text->globalPosition);

            CursorPos startPos = GetCursorPositionForGlobal(currentBuffer, selectionLeft);
            CursorPos endPos = GetCursorPositionForGlobal(currentBuffer, selectionRight);

            i32 len = GetLineLength(currentBuffer, startPos.line);
            i32 maxLen = len - startPos.lineOffset;
            i32 firstLineLen = MinI32(selectionRight - selectionLeft, maxLen);

            PaintRect(x + startPos.lineOffset * font.charWidth,
                      y + startPos.line * lineHeightPx,
                      firstLineLen * font.charWidth,
                      lineHeightPx,
                      selectionBgColor);

            if (startPos.line != endPos.line)
            {
                for (i32 l = startPos.line + 1; l < endPos.line; l++)
                {
                    PaintRect(x,
                              y + l * lineHeightPx,
                              GetLineLength(currentBuffer, l) * font.charWidth,
                              lineHeightPx,
                              selectionBgColor);
                }

                PaintRect(x,
                          y + endPos.line * lineHeightPx,
                          endPos.lineOffset * font.charWidth,
                          lineHeightPx,
                          selectionBgColor);
            }
        }
    }

    // highlight search results
    i32 currentSearchEntryIndex = 0;
    EntryFound *found = NULL;

    for (i32 i = 0; i < text->buffer.size; i++)
    {
        if (currentSearchEntryIndex < entriesCount)
            found = &entriesAt[currentSearchEntryIndex];
        else
            found = NULL;

        if (mode == ModeLocalSearch && found && i >= found->at && i < found->at + found->len)
        {
            if (currentSearchEntryIndex == currentEntry)
                PaintRect(x, y, font.charWidth, font.charHeight, 0x448844);
            else
                PaintRect(x, y, font.charWidth, font.charHeight, 0x444444);
        }

        char ch = text->buffer.content[i];
        if (ch == '\n')
        {
            x = view->rect.x + padding;
            y += lineHeightPx;
        }
        else
        {
            x += font.charWidth;
        }

        if (found && i >= found->at + found->len)
        {
            currentSearchEntryIndex++;
        }
    }

    x = view->rect.x + padding;
    y = view->rect.y + padding - view->scrollOffset.current;

    for (i32 i = 0; i < text->buffer.size; i++)
    {
        char ch = text->buffer.content[i];
        if (ch == '\n')
        {
            x = view->rect.x + padding;
            y += lineHeightPx;
        }
        else
        {
            if (ch >= ' ' && ch < MAX_CHAR_CODE)
                CopyBitmapRectTo(&view->rect, &font.textures[ch], x, y, colors.font);
            else
                PaintRect(x, y, font.charWidth, font.charHeight, 0xff2222);
            x += font.charWidth;
        }
    }

    // I subtract scrollOffset initially, I need to think what is the better way to solve this
    view->pageHeight = y + lineHeightPx + padding + view->scrollOffset.current;
    DrawScrollBar(view);
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

    leftTop.rect.width = leftPanelWidth;
    leftTop.rect.height = view.y * topScale;

    Rect leftMiddle = {0, leftTop.rect.y + leftTop.rect.height, leftPanelWidth, view.y * middleScale};

    u32 workAreaWidth = screen.width - leftColumn.width;

    codeLeftColumnRect.rect.x = leftPanelWidth;
    codeLeftColumnRect.rect.width = workAreaWidth * leftCodeScale;
    codeLeftColumnRect.rect.height = screen.height;
    Rect codeRightColumnRect = {codeLeftColumnRect.rect.x + codeLeftColumnRect.rect.width, 0, workAreaWidth * rightCodeScale, view.y};
    Rect runColumnRect = {codeRightColumnRect.x + codeRightColumnRect.width, 0, workAreaWidth * runSplitScale, view.y};

    Rect *rectFocused = NULL;
    if (uiPartFocused == Tasks)
        rectFocused = &leftTop.rect;
    if (uiPartFocused == Explorer)
        rectFocused = &leftMiddle;
    if (uiPartFocused == LeftCode)
        rectFocused = &codeLeftColumnRect.rect;
    if (uiPartFocused == Execution)
        rectFocused = &runColumnRect;

    if (uiPartFocused == Tasks)
    {
        focusedView = &leftTop;
        focusedText = &textBuffer;
    }
    else if (uiPartFocused == LeftCode)
    {
        focusedView = &codeLeftColumnRect;
        focusedText = &leftBuffer;
    }
    else
    {
        focusedView = NULL;
        focusedText = NULL;
    }

    if (rectFocused)
        OutlineRect(*rectFocused, colors.activeLine);

    DrawRightBoundary(leftColumn);
    DrawBottomBoundary(leftMiddle);
    DrawBottomBoundary(leftTop.rect);

    DrawRightBoundary(codeLeftColumnRect.rect);

    RenderFileExplorer(&leftMiddle);

    RenderTextInsideRect(&leftTop, &textBuffer);
    UpdateSpring(&leftTop.scrollOffset, 1.0f / 60.0f);

    RenderTextInsideRect(&codeLeftColumnRect, &leftBuffer);
    UpdateSpring(&codeLeftColumnRect.scrollOffset, 1.0f / 60.0f);

    if (mode == ModeLocalSearch)
        DrawLocalSearch(focusedView);

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

void ScrollIntoView()
{
    if (focusedView && focusedText && focusedView->rect.height < focusedView->pageHeight)
    {
        i32 itemsToLookAhead = 3;

        i32 lineHeightPx = RoundI32((f32)font.charHeight * lineHeight);

        i32 cursorY =
            focusedText->line * lineHeightPx + padding;

        i32 spaceToLookAhead = lineHeightPx * itemsToLookAhead;

        if (
            cursorY + spaceToLookAhead - focusedView->rect.height >
            focusedView->scrollOffset.target)
        {
            focusedView->scrollOffset.target = ClampOffset(cursorY - focusedView->rect.height + spaceToLookAhead);
        }
        else if (cursorY - spaceToLookAhead < focusedView->scrollOffset.target)
        {
            i32 targetOffset = cursorY - spaceToLookAhead;
            focusedView->scrollOffset.target = ClampOffset(targetOffset);
        }
    }
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
}

inline void EnterInsertMode()
{
    mode = ModeInsert;
    isJustMovedToInsert = 1;
}

void SetFocusedPart(UiPart part)
{
    uiPartFocused = part;
    if (uiPartFocused == LeftCode)
    {
        currentBuffer = &leftBuffer;
        currentBufferPath = leftBufferName;
    }
    else if (uiPartFocused == Tasks)
    {
        currentBuffer = &textBuffer;
        currentBufferPath = textBufferName;
    }
}

void Copy(HWND window, Text *text)
{
    i32 start;
    i32 end;
    if (text->selectionStart == -1)
    {
        start = FindLineStart(text, text->globalPosition);
        end = FindLineEnd(text, text->globalPosition) + 1;
    }
    else
    {
        start = MinI32(text->selectionStart, text->globalPosition);
        end = MaxI32(text->selectionStart, text->globalPosition);
    }
    SetClipboard(window, &text->buffer.content[start], end - start);
}

void PasteFromClipboard(HWND window, Text *text)
{
    if (text->selectionStart != -1)
        RemoveSelection(text);

    OpenClipboard(window);
    HANDLE hClipboardData = GetClipboardData(CF_TEXT);
    char *pchData = (char *)GlobalLock(hClipboardData);
    if (pchData)
    {
        i32 len = strlen(pchData);
        InsertChars(&text->buffer, pchData, len, text->globalPosition);
        SetCursorPosition(text, text->globalPosition + len);
        GlobalUnlock(hClipboardData);
    }
    else
    {
        // OutputDebugStringA("Failed to capture clipboard\n");
    }
    CloseClipboard();
    // ScrollIntoView();
}

LRESULT OnEvent(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {

    case WM_SYSCOMMAND:
        if (wParam == SC_KEYMENU)
        {
            return 0;
        }
        break;

    case WM_MOUSEWHEEL:

        if (focusedView && focusedView->rect.height < focusedView->pageHeight)
        {
            focusedView->scrollOffset.target = ClampOffset(focusedView->scrollOffset.target - GET_WHEEL_DELTA_WPARAM(wParam));
        }
        break;

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:

        if (uiPartFocused == Execution)
        {
            if (IsKeyPressed(VK_CONTROL) && (wParam >= '1' && wParam < ('1' + UiPartsCount)))
                SetFocusedPart(wParam - '1');
            else if (onEventCb)
                onEventCb(window, message, wParam, lParam);
        }
        else if (IsKeyPressed(VK_CONTROL) && (wParam >= '1' && wParam < ('1' + UiPartsCount)))
            SetFocusedPart(wParam - '1');
        else if (wParam == VK_F11)
        {
            isFullscreen = !isFullscreen;
            SetFullscreen(window, isFullscreen);
        }
        else if (wParam == VK_F4 && IsKeyPressed(VK_MENU))
        {
            PostQuitMessage(0);
            isRunning = 0;
        }
        else if (currentBuffer)
        {
            if (mode == ModeLocalSearch)
            {
                if (wParam == VK_BACK && searchLen > 0)
                    searchTerm[--searchLen] = '\0';

                else if (IsKeyPressed(VK_MENU) && wParam == 'J')
                {
                    if (currentEntry < entriesCount - 1)
                    {
                        currentEntry++;
                        SetCursorPosition(currentBuffer, entriesAt[currentEntry].at);
                        ScrollIntoView();
                    }
                }
                else if (IsKeyPressed(VK_MENU) && wParam == 'K')
                {
                    if (currentEntry > 0)
                    {
                        currentEntry--;
                        SetCursorPosition(currentBuffer, entriesAt[currentEntry].at);
                        ScrollIntoView();
                    }
                }
            }
            else if (mode == ModeNormal)
            {
                if (wParam == 'V')
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
                    ScrollIntoView();
                }
                if (wParam == 'K')
                {
                    if (IsKeyPressed(VK_MENU))
                        SwapLineUp(currentBuffer, &tempArena);
                    else
                        GoUp(currentBuffer);
                    ScrollIntoView();
                }
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

                if (wParam == 'G')
                {
                    if (IsKeyPressed(VK_CONTROL))
                        SetCursorPosition(currentBuffer, currentBuffer->buffer.size);
                    else
                        SetCursorPosition(currentBuffer, 0);

                    ScrollIntoView();
                }

                if (wParam == 'Y')
                    Copy(window, currentBuffer);
                if (wParam == 'P')
                    PasteFromClipboard(window, currentBuffer);

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

                if (wParam == 'W')
                    JumpWordForward(currentBuffer);

                if (wParam == 'B')
                    JumpWordBackward(currentBuffer);

                if (wParam == 'Q')
                    MoveToLineStart(currentBuffer);

                if (wParam == 'R')
                    MoveToLineEnd(currentBuffer);
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
        if (mode == ModeLocalSearch)
        {
            if (wParam >= ' ' && wParam < MAX_CHAR_CODE)
                searchTerm[searchLen++] = wParam;

            FindEntries(currentBuffer);
            SetCursorPosition(currentBuffer, entriesAt[currentEntry].at);
            ScrollIntoView();
            // CenterOnRowIfNotVisible();
        }
        else if (mode == ModeInsert && currentBuffer)
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
    InitAnimations();

    tempArena = CreateArena(KB(512));
    leftBuffer.buffer = ReadFileIntoDoubledSizedBuffer(leftBufferName);
    textBuffer.buffer = ReadFileIntoDoubledSizedBuffer(textBufferName);

    HWND window = OpenWindow(OnEvent, colors.bg, "Editor");

    SetFocusedPart(LeftCode);

    leftBuffer.selectionStart = -1;
    textBuffer.selectionStart = -1;

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
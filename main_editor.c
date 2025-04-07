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
    Spring scrollOffset;

    i32 isSaved;
    char *path;
    Text textBuffer;

    Arena changesAreana;
    i32 currentChange;
    i32 totalChanges;
} ScreenView;

#define VIEWS_COUNT 4
ScreenView sidebarTop;
ScreenView sidebarMiddle;
ScreenView sidebarBottom;
ScreenView codeView;

// should be one
ScreenView *focusedView;

// this is used to figure out where to load text from the File Explorer
ScreenView *lastSelectedView;

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

HMODULE libModule;

f32 compilationMs;
f32 renderResultsUs[100];
i32 currentRenderTime;
u32 isProdBuild;

Arena tempArena;

typedef struct FileInfo
{
    char *name;
} FileInfo;

FileInfo files[] =
    {
        "main_editor.c",
        "sample.txt",
        "logs.txt",
        "actions.txt",
        "search.c",
        "types.c",
        "main_lib.c",
};

i32 selectedFile = 1;

typedef enum Mode
{
    ModeNormal,
    ModeInsert,
    ModeLocalSearch,
} Mode;

i32 isJustMovedToInsert = 0; // this is used to avoid first WM_CHAR event after entering insert mode
Mode mode = ModeNormal;

u8 isBuildOk;
u8 buildBuffer[KB(20)];
i32 buildBufferLen;

RenderApp *render;
OnLibEvent *onEventCb;

// typedef enum UiPart
// {
//     Tasks = 0,
//     FileExplorerPart,
//     LeftCode,
//     UiPartsCount
// } UiPart;

// UiPart uiPartFocused;

typedef struct ColorScheme
{
    u32 bg, line, activeLine, lastSelectedOutline, font, scrollbar, selection, selectedItem;
} ColorScheme;

ColorScheme colors;

void InitColors()
{
    colors.bg = 0x080808;
    colors.font = 0xEEEEEE;
    colors.line = 0x2F2F2F;
    colors.activeLine = 0x81315C;
    colors.lastSelectedOutline = 0x514122;
    colors.scrollbar = 0x333333;
    colors.selection = 0x224545;
    colors.selectedItem = 0x333333;
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

//
// Undo Redo
//

typedef enum ModificationType
{
    ModificationInsert,
    ModificationRemove,
} ModificationType;
typedef struct TextRange
{
    ModificationType type;
    i32 start;
    i32 len;
} TextRange;

typedef struct Change
{
    i32 size;
    TextRange textModified;
    // i32 cursorPositionBefore;
    // i32 cursorPositionAfter;
    u8 chars[];
} Change;

// Arena changesArena;

Change *FindChangeAtIndex(i32 index)
{
    i32 current = 0;
    Change *change = (Change *)focusedView->changesAreana.start;
    while (current < index)
    {
        change += change->size;
        current++;
    }

    return change;
}

void UndoChange()
{
    if (focusedView->currentChange > 0)
    {
        Change *changeToUndo = FindChangeAtIndex(focusedView->currentChange - 1);
        Text *text = &focusedView->textBuffer;
        i32 start = changeToUndo->textModified.start;
        i32 len = changeToUndo->textModified.len;
        i32 end = start + changeToUndo->textModified.len - 1;

        if (changeToUndo->textModified.type == ModificationInsert)
            RemoveChars(&text->buffer, start, end);
        else
            InsertChars(&text->buffer, changeToUndo->chars, changeToUndo->textModified.len, start);

        SetCursorPosition(text, start);
        focusedView->currentChange--;
    }
}
void RedoChange()
{
    if (focusedView->currentChange < focusedView->totalChanges)
    {
        Change *changeToUndo = FindChangeAtIndex(focusedView->currentChange);
        Text *text = &focusedView->textBuffer;
        i32 start = changeToUndo->textModified.start;
        i32 len = changeToUndo->textModified.len;
        i32 end = start + changeToUndo->textModified.len - 1;

        if (changeToUndo->textModified.type == ModificationInsert)
            InsertChars(&text->buffer, changeToUndo->chars, len, start);
        else
            RemoveChars(&text->buffer, start, end);

        SetCursorPosition(text, start);
        focusedView->currentChange++;
    }
}

void AddChangeOfText(char *textModified, i32 at, ModificationType type)
{
    Change *change = FindChangeAtIndex(focusedView->currentChange);
    change->size = sizeof(Change) + strlen(textModified);
    change->textModified.type = type;
    change->textModified.start = at;
    change->textModified.len = strlen(textModified);

    memmove(change->chars, textModified, strlen(textModified));

    focusedView->currentChange += 1;
    focusedView->totalChanges = focusedView->currentChange;
    focusedView->changesAreana.bytesAllocated += change->size;
}

void InitChanges()
{
    AddChangeOfText("one\n", 0, ModificationInsert);
    AddChangeOfText("two\n", 4, ModificationInsert);
    AddChangeOfText("two\n", 4, ModificationRemove);
    AddChangeOfText("two\n", 4, ModificationInsert);
    AddChangeOfText("three", 8, ModificationInsert);
}

void BeginChange() {}
void EndChange() {}
//
//
//

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

void RenderTextInsideRect(ScreenView *view)
{

    i32 lineHeightPx = RoundI32((f32)font.charHeight * lineHeight);

    i32 x = view->rect.x + padding;
    i32 y = view->rect.y + padding - view->scrollOffset.current;

    Text *text = &view->textBuffer;
    if (focusedView == view)
    {
        u32 cursorX = x + text->lineOffset * font.charWidth - 1;
        u32 cursorY = y + text->line * lineHeightPx - 1;

        u32 currentLineBg = 0x202020;
        u32 cursorColor = mode == ModeNormal ? 0x22ff22 : 0xff2222;
        PaintRect(view->rect.x, cursorY, view->rect.width, lineHeightPx, currentLineBg);
        PaintRect(cursorX, cursorY, 2, lineHeightPx, cursorColor);

        u32 selectionBgColor = colors.selection;
        if (text->selectionStart != -1)
        {
            u32 selectionLeft = MinI32(text->selectionStart, text->globalPosition);
            u32 selectionRight = MaxI32(text->selectionStart, text->globalPosition);

            CursorPos startPos = GetCursorPositionForGlobal(text, selectionLeft);
            CursorPos endPos = GetCursorPositionForGlobal(text, selectionRight);

            i32 len = GetLineLength(text, startPos.line);
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
                              GetLineLength(text, l) * font.charWidth,
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
    if (focusedView == view && mode == ModeLocalSearch)
    {
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
    view->pageHeight = y + lineHeightPx + padding + view->scrollOffset.current - view->rect.y;
    DrawScrollBar(view);
}

void RenderFileExplorer(ScreenView *view)
{
    Rect *rect = &view->rect;
    i32 x = rect->x + 25;
    i32 y = rect->y + 10 + font.charHeight / 2;

    i32 squareSize = 6;

    u32 squareToText = 10;

    f32 lineHeight = 1.3f;
    i32 lineHeightPx = RoundI32((f32)font.charHeight * lineHeight);

    for (i32 i = 0; i < ArrayLength(files); i++)
    {
        FileInfo file = files[i];
        if (file.name == NULL)
            break;

        if (focusedView == view && i == selectedFile)
        {
            PaintRect(rect->x, y - lineHeightPx / 2, rect->width, lineHeightPx, colors.selectedItem);
        }
        PaintSquareAtCenter(x, y, squareSize, 0x888888);

        DrawTextLineLen(rect, file.name, strlen(file.name), x + squareSize / 2 + squareToText, y - font.charHeight / 2 - 1, colors.font);

        y += lineHeightPx;
    }

    i32 padding = 5;
    u32 statusHeight = lineHeightPx * 2;
    PaintRect(rect->x, rect->y + rect->height - statusHeight, rect->width, statusHeight, 0x222222);

    Arena *arena = &lastSelectedView->changesAreana;
    u8 status[500] = {0};
    i32 pos = 0;
    pos += AppendStr("Changes: ", status + pos);
    pos += AppendI32(focusedView->currentChange, status + pos);
    pos += AppendStr(" of ", status + pos);
    pos += AppendI32(focusedView->totalChanges, status + pos);
    DrawTextLine(rect, status, rect->x + padding, rect->y + rect->height - statusHeight + padding, 0xffffff);

    pos = 0;
    memset(status, 0, 500);

    pos += AppendStr("Change arena: ", status + pos);
    pos += AppendI32(arena->bytesAllocated, status + pos);
    pos += AppendStr("b of ", status + pos);
    pos += AppendI32(arena->size, status + pos);
    pos += AppendStr("b", status + pos);

    DrawTextLine(rect, status, rect->x + padding, rect->y + rect->height - statusHeight + padding + lineHeightPx, 0xffffff);
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

void RenderModifiedLabel(ScreenView *view)
{
    Rect *r = &view->rect;
    char *label = "Modified";
    i32 labelPadding = 4;
    i32 width = font.charWidth * strlen(label) + labelPadding * 2;
    i32 height = font.charHeight + labelPadding * 2;
    i32 x = r->x + r->width - width;
    i32 y = r->y + r->height - height;
    PaintRect(x, y, width, height, 0x222222);
    DrawTextLine(r, label, x + labelPadding, y + labelPadding, 0xcccccc);
}
void Draw()
{
    ClearToBg();

    Rect screen = {0, 0, view.x, view.y};

    u32 leftPanelWidth = 650;
    Rect leftColumn = {0, 0, leftPanelWidth, view.y};

    // Left sibeadar
    f32 topHeight = 400;
    f32 middleHeight = (screen.height - topHeight) * 0.5f;
    f32 bottomHeight = middleHeight;

    sidebarTop.rect.width = leftPanelWidth;
    sidebarTop.rect.height = topHeight;

    sidebarMiddle.rect.y = topHeight;
    sidebarMiddle.rect.width = leftPanelWidth;
    sidebarMiddle.rect.height = middleHeight;

    sidebarBottom.rect.y = sidebarMiddle.rect.y + sidebarMiddle.rect.height;
    sidebarBottom.rect.width = leftPanelWidth;
    sidebarBottom.rect.height = middleHeight;

    u32 workAreaWidth = screen.width - leftColumn.width;

    codeView.rect.x = leftPanelWidth;
    codeView.rect.width = workAreaWidth;
    codeView.rect.height = screen.height;

    // if (focusedView)
    //     OutlineRect(focusedView->rect, colors.activeLine);

    if (lastSelectedView != focusedView)
        OutlineRect(lastSelectedView->rect, colors.lastSelectedOutline);

    DrawRightBoundary(leftColumn);

    DrawBottomBoundary(sidebarTop.rect);
    DrawBottomBoundary(sidebarMiddle.rect);
    DrawBottomBoundary(sidebarBottom.rect);

    RenderFileExplorer(&sidebarTop);

    ScreenView *allViews[] = {&sidebarMiddle, &sidebarBottom, &codeView};

    for (i32 i = 0; i < ArrayLength(allViews); i++)
    {
        ScreenView *view = allViews[i];

        if (view->textBuffer.buffer.content)
        {
            RenderTextInsideRect(view);

            if (!view->isSaved)
                RenderModifiedLabel(view);

            UpdateSpring(&view->scrollOffset, 1.0f / 60.0f);
        }
    }

    // RenderTextInsideRect(&leftTop, &textBuffer);
    // UpdateSpring(&leftTop.scrollOffset, 1.0f / 60.0f);

    // RenderTextInsideRect(&codeRect, &leftBuffer);
    // UpdateSpring(&codeRect.scrollOffset, 1.0f / 60.0f);

    if (mode == ModeLocalSearch)
        DrawLocalSearch(focusedView);

    // if (isBuildOk && render)
    // {
    //     u32 padding = 5;
    //     u32 statusHeight = (font.charHeight + padding * 2);
    //     Rect exectuionStatusLine = {runColumnRect.x, runColumnRect.y + runColumnRect.height - statusHeight, runColumnRect.width, statusHeight};
    //     runColumnRect.height -= statusHeight;

    //     i64 start = GetPerfCounter();
    //     render(&canvas, &runColumnRect);
    //     i32 renderUs = (f32)(GetPerfCounter() - start) * 1000.0f * 1000.0f / (f32)GetPerfFrequency();

    //     renderResultsUs[currentRenderTime++] = renderUs;
    //     if (currentRenderTime >= ArrayLength(renderResultsUs))
    //         currentRenderTime = 0;

    //     f32 totalRenderTime = 0;
    //     i32 renderCount = 0;
    //     for (i32 i = 0; i < ArrayLength(renderResultsUs); i++)
    //     {
    //         totalRenderTime += renderResultsUs[i];
    //         if (renderResultsUs[i] != 0)
    //             renderCount++;
    //     }

    //     f32 averageRenderTime = renderCount == 0 ? 0 : totalRenderTime / (f32)renderCount;

    //     u8 label[255] = {0};
    //     i32 pos = 0;
    //     pos += AppendStr("Compile: ", label + pos);
    //     pos += AppendI32((i32)compilationMs, label + pos);
    //     pos += AppendStr("ms", label + pos);
    //     if (isProdBuild)
    //         pos += AppendStr("  Prod: ", label + pos);
    //     else
    //         pos += AppendStr("  Debug: ", label + pos);

    //     pos += AppendI32((i32)averageRenderTime, label + pos);
    //     pos += AppendStr("us", label + pos);
    //     DrawTextLine(&exectuionStatusLine, label, exectuionStatusLine.x + padding, exectuionStatusLine.y + padding, 0xffffff);
    // }
    // else if (!isBuildOk)
    // {
    //     RenderBuildResult(&runColumnRect);
    // }

    StretchDIBits(dc, 0, 0, view.x, view.y, 0, 0, view.x, view.y, canvas.pixels, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}

void ScrollIntoView()
{
    if (focusedView && focusedView->rect.height < focusedView->pageHeight)
    {
        i32 itemsToLookAhead = 3;

        i32 lineHeightPx = RoundI32((f32)font.charHeight * lineHeight);

        i32 cursorY =
            focusedView->textBuffer.line * lineHeightPx + padding;

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

void FocusOnUiPart(i32 orderedIndex)
{
    if (orderedIndex == 0)
        focusedView = &sidebarTop;
    else if (orderedIndex == 1)
        focusedView = &sidebarMiddle;
    else if (orderedIndex == 2)
        focusedView = &sidebarBottom;
    else if (orderedIndex == 3)
        focusedView = &codeView;

    if (focusedView != &sidebarTop)
        lastSelectedView = focusedView;
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

void LoadFileIntoView(ScreenView *view, char *filename)
{
    if (view->changesAreana.size == 0)
        view->changesAreana = CreateArena(KB(1));

    view->currentChange = 0;
    view->totalChanges = 0;

    view->path = filename;
    view->textBuffer.buffer = ReadFileIntoDoubledSizedBuffer(view->path);
    view->isSaved = 1;
    SetCursorPosition(&view->textBuffer, 0);
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

        // if (uiPartFocused == Execution)
        // {
        //     if (IsKeyPressed(VK_CONTROL) && (wParam >= '1' && wParam < ('1' + UiPartsCount)))
        //         SetFocusedPart(wParam - '1');
        //     else if (onEventCb)
        //         onEventCb(window, message, wParam, lParam);
        // }
        // else
        if (IsKeyPressed(VK_CONTROL) && (wParam >= '1' && wParam < ('1' + VIEWS_COUNT)))
            FocusOnUiPart(wParam - '1');
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
        else if (focusedView == &sidebarTop)
        {
            if (wParam == 'J')
                selectedFile = MinI32(selectedFile + 1, ArrayLength(files) - 1);
            else if (wParam == 'K')
                selectedFile = MaxI32(selectedFile - 1, 0);
            else if (wParam == VK_SPACE || wParam == VK_RETURN)
            {
                LoadFileIntoView(lastSelectedView, files[selectedFile].name);
            }
        }
        else if (focusedView)
        {
            Text *currentBuffer = &focusedView->textBuffer;

            if (mode == ModeLocalSearch)
            {
                if ((wParam == VK_BACK || (wParam == 'Z' && IsKeyPressed(VK_CONTROL))) && searchLen > 0)
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
                else if (wParam == 'F' && IsKeyPressed(VK_CONTROL))
                    mode = ModeNormal;
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
                    {
                        BeginChange();
                        SwapLineDown(currentBuffer, &tempArena);
                        EndChange();
                    }
                    else
                        GoDown(currentBuffer);
                    ScrollIntoView();
                }
                if (wParam == 'K')
                {
                    if (IsKeyPressed(VK_MENU))
                    {
                        BeginChange();
                        SwapLineUp(currentBuffer, &tempArena);
                        EndChange();
                    }
                    else
                        GoUp(currentBuffer);
                    ScrollIntoView();
                }
                if (wParam == 'I')
                {
                    BeginChange();
                    EnterInsertMode();
                }

                if (wParam == 'U' && IsKeyPressed(VK_SHIFT))
                {
                    RedoChange();
                    focusedView->textBuffer.selectionStart = -1;
                }
                else if (wParam == 'U')
                    UndoChange();

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

                if (wParam == 'F' && IsKeyPressed(VK_CONTROL))
                    mode = ModeLocalSearch;
                if (wParam == 'Y')
                    Copy(window, currentBuffer);
                if (wParam == 'P')
                    PasteFromClipboard(window, currentBuffer);

                if (wParam == 'S' && focusedView->path)
                {
                    WriteMyFile(focusedView->path, currentBuffer->buffer.content, currentBuffer->buffer.size);
                    focusedView->isSaved = 1;
                }

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
                {
                    BeginChange();
                    MoveLineLeft(currentBuffer);
                    EndChange();
                    currentBuffer->selectionStart = -1;
                }
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
                {
                    mode = ModeNormal;
                    EndChange();
                }

                else if (wParam == VK_BACK)
                    RemoveCharFromLeft(currentBuffer);
                else if (wParam == VK_DELETE)
                    RemoveCharFromRight(currentBuffer);
                else if (wParam == VK_TAB && IsKeyPressed(VK_SHIFT))
                {
                    MoveLineLeft(currentBuffer);
                    currentBuffer->selectionStart = -1;
                }
                else if (wParam == VK_TAB)
                    MoveLineRight(currentBuffer);
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

            FindEntries(&focusedView->textBuffer);
            SetCursorPosition(&focusedView->textBuffer, entriesAt[currentEntry].at);
            ScrollIntoView();
            // CenterOnRowIfNotVisible();
        }
        else if (mode == ModeInsert && focusedView)
        {
            if (isJustMovedToInsert)
                isJustMovedToInsert = 0;
            else if (wParam == '\r' || wParam == '\n')
                InsertCharAtCurrentPosition(&focusedView->textBuffer, '\n');
            else if (wParam >= ' ')
                InsertCharAtCurrentPosition(&focusedView->textBuffer, wParam);
            focusedView->isSaved = 0;

            focusedView->textBuffer.selectionStart = -1;
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

    // TODO: remove this temp

    tempArena = CreateArena(KB(512));

    HWND window = OpenWindow(OnEvent, colors.bg, "Editor");

    LoadFileIntoView(&codeView, files[1].name);
    LoadFileIntoView(&sidebarMiddle, files[2].name);
    LoadFileIntoView(&sidebarBottom, files[3].name);

    FocusOnUiPart(3);
    InitChanges();

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
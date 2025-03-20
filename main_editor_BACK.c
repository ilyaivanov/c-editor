#include <windows.h>

#include "util/font.c"
#include "util/string.c"
#include "./util/win32.c"
#include "./util/anim.c"
#include "./foo.c"

#include "./layout.c"
#include "main_lib.h"

u32 isRunning = 1;
u32 isFullscreen = 0;
V2i view;

Arena fontArena;
BITMAPINFO bitmapInfo;
StringBuffer buffer;
HDC dc;

u64 frequency;
u64 start;

f32 lineHeight = 1.2f;
i32 fontSize = 13;
i32 fontSizeTitle = 24;
Spring scrollOffset = {0};

u64 compileTime;

typedef struct CursorPos
{
    i32 global;
    i32 line;
    i32 lineOffset;
} CursorPos;

i32 selectionStart = -1;
CursorPos cursor;

i32 currentFile = -1;
char *files[] = {
    ".\\main_lib.c",
    ".\\test.txt",
    ".\\main_editor.c",
    ".\\concepts.txt",
    ".\\sample.c",
};

i32 mystrlen(char *str)
{
    i32 len = 0;
    while (str[len] != '\0')
        len++;
    return len;
}

typedef enum Mode
{
    Insert,
    Normal,
    Search
} Mode;

Mode mode = Normal;
i32 isJustMovedToInsert = 0; // this is used to avoid first WM_CHAR event after entering insert mode
u32 isCtrlPressed = 0;
u32 isAltPressed = 0;
u32 isShiftPressed = 0;

u32 isSaved = 1;

u32 linesCount = 0;
f32 pageHeight;
u32 searchLen = 0;

// TODO: use arena
char searchTerm[KB(1)];

u32 entriesCount = 0;
u32 currentEntry = 0;

typedef struct ColorScheme
{
    u32 font;
    u32 searchResult;
    u32 searchResultActive;
    u32 line;
    u32 lineCurrent;
    u32 currentLineBg;
    u32 footerBg;
    u32 bg;
    u32 selectionBg;
} ColorScheme;

typedef struct Foo
{
    int size;
    u8 vals[];
} Foo;

i32 padding = 20;

ColorScheme colorScheme;

FontData font = {0};
FontData bigFont = {0};

typedef struct EntryFound
{
    u32 at;
    u32 len;
} EntryFound;

// TODO: use arena
EntryFound entriesAt[1024 * 10] = {0};
char consoleBuffer[KB(256)];
u32 consoleBufferLen = 0;

HMODULE hDll;
RenderApp *render;

void SetupFonts()
{
    InitFontData(&font, FontInfoAntialiased("Consolas", fontSize), &fontArena);
    InitFontData(&bigFont, FontInfoAntialiased("Consolas", fontSizeTitle), &fontArena);

    colorScheme.bg = 0x080808;
    colorScheme.font = 0xffffff;
    colorScheme.searchResult = 0x7070f0;
    colorScheme.searchResultActive = 0x40f040;
    colorScheme.line = 0x606060;
    colorScheme.lineCurrent = 0xC0C0C0;
    colorScheme.currentLineBg = 0x202020;
    colorScheme.footerBg = 0x202020;
    colorScheme.selectionBg = 0x12448F;
}
void SaveFile()
{
    WriteMyFile(files[currentFile], buffer.content, buffer.size);
    isSaved = 1;
}

void Recompile()
{
    u64 compileStart = GetPerfCounter();

    if (hDll)
    {
        FreeLibrary(hDll);
        render = NULL;
    }

    // RunWithoutOutput();
    RunAndCaptureOutput(consoleBuffer, &consoleBufferLen);

    hDll = LoadLibrary("lib\\lib.dll");
    if (hDll)
    {
        RenderApp *foo = (RenderApp *)GetProcAddress(hDll, "RenderApp");

        if (foo)
            render = foo;
    }

    compileTime = GetPerfCounter() - compileStart;
}

inline char ToCharLower(char ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return ch + ('a' - 'A');

    return ch;
}

void ScrollIntoView();

CursorPos GetCursorPositionForGlobal(i32 pos)
{
    CursorPos res = {0};
    res.global = -1;
    if (pos >= 0 && pos <= buffer.size)
    {
        res.global = pos;
        res.line = 0;

        i32 lineStartedAt = 0;
        for (i32 i = 0; i < pos; i++)
        {
            if (buffer.content[i] == '\n')
            {
                res.line++;
                lineStartedAt = i + 1;
            }
        }

        res.lineOffset = pos - lineStartedAt;
    }
    return res;
}

i32 GetLineLength(i32 line)
{
    i32 currentLine = 0;
    i32 currentLineLength = 0;
    for (i32 i = 0; i < buffer.size; i++)
    {
        if (buffer.content[i] == '\n')
        {
            if (currentLine == line)
                return currentLineLength + 1;

            currentLine++;
            currentLineLength = 0;
        }
        else
        {
            currentLineLength++;
        }
    }
    return currentLineLength;
}

void SetCursorGlobalPos(i32 pos)
{
    if (isShiftPressed && selectionStart == -1)
    {
        selectionStart = cursor.global;
    }
    else if (!isShiftPressed)
    {
        selectionStart = -1;
    }

    CursorPos newCursor = GetCursorPositionForGlobal(pos);
    if (newCursor.global != -1)
        cursor = newCursor;
}

void FindEntries()
{
    i32 currentWordIndex = 0;
    entriesCount = 0;
    for (i32 i = 0; i < buffer.size; i++)
    {

        if (ToCharLower(buffer.content[i]) ==
            ToCharLower(searchTerm[currentWordIndex]))
        {
            currentWordIndex++;
            if (currentWordIndex == searchLen)
            {
                entriesAt[entriesCount].at = i - searchLen + 1;
                entriesAt[entriesCount].len = searchLen;
                entriesCount++;
                currentWordIndex = 0;
            }
        }
        else
        {
            currentWordIndex = 0;
        }
    }

    currentEntry = 0;
}

void OnTextChanged()
{
    isSaved = 0;
    linesCount = 1;
    for (i32 i = 0; i < buffer.size; i++)
    {
        if (buffer.content[i] == '\n')
            linesCount++;
    }

    f32 v = (f32)font.charHeight * lineHeight;
    i32 lineHeightPx = (i32)(v + 0.5);
    pageHeight = linesCount * lineHeightPx + padding * 2;
}

i32 FindLineStart(i32 pos)
{
    while (pos > 0 && buffer.content[pos - 1] != '\n')
        pos--;

    return pos;
}

i32 FindLineEnd(i32 pos)
{
    while (pos < buffer.size && buffer.content[pos] != '\n')
        pos++;

    return pos;
}

inline i32 MinI32(i32 v1, i32 v2)
{
    return v1 < v2 ? v1 : v2;
}

inline i32 MaxI32(i32 v1, i32 v2)
{
    return v1 > v2 ? v1 : v2;
}

inline f32 MaxF32(f32 v1, f32 v2)
{
    return v1 > v2 ? v1 : v2;
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

i32 GetIdentationLevel()
{
    i32 start = FindLineStart(cursor.global);
    i32 level = 0;
    while (buffer.content[start + level] == ' ')
        level++;

    return level;
}

void GoDown()
{
    i32 next = FindLineEnd(cursor.global);
    i32 nextNextLine = FindLineEnd(next + 1);

    SetCursorGlobalPos(MinI32(next + cursor.lineOffset + 1, nextNextLine));
}

void GoUp()
{
    i32 prev = FindLineStart(cursor.global);
    i32 prevPrevLine = FindLineStart(prev - 1);

    i32 pos = prevPrevLine + cursor.lineOffset;

    SetCursorGlobalPos(MinI32(pos, prev));
}

void RemoveSelection()
{
    i32 left = MinI32(selectionStart, cursor.global);
    i32 right = MaxI32(selectionStart, cursor.global);

    RemoveChars(&buffer, left, right - 1);
    SetCursorGlobalPos(left);
    selectionStart = -1;
}

void RemoveCharFromLeft()
{
    if (selectionStart != -1)
        RemoveSelection();
    else if (cursor.global > 0)
    {
        RemoveCharAt(&buffer, cursor.global - 1);
        SetCursorGlobalPos(cursor.global - 1);
    }

    OnTextChanged();
    SaveFile();
    Recompile();
}

void RemoveCharFromRight()
{
    if (selectionStart != -1)
        RemoveSelection();
    else if (cursor.global < buffer.size)
        RemoveCharAt(&buffer, cursor.global);

    OnTextChanged();
    SaveFile();
    Recompile();
}

void RemoveLine()
{
    RemoveChars(&buffer, FindLineStart(cursor.global), FindLineEnd(cursor.global));

    OnTextChanged();
}

void SwapLineDown()
{
    i32 cursorPosition = cursor.global;

    i32 lineStart = FindLineStart(cursorPosition);
    i32 lineEnd = FindLineEnd(lineStart);

    if (lineEnd == buffer.size)
        return; // Last line, nothing to swap

    i32 nextLineStart = lineEnd + 1;
    i32 nextLineEnd = FindLineEnd(nextLineStart);

    i32 lineLen = lineEnd - lineStart;
    i32 nextLineLen = nextLineEnd - nextLineStart;

    char *temp = ArenaPush(&fontArena, lineLen + 1);

    memcpy(temp, buffer.content + lineStart, lineLen);
    temp[lineLen] = '\0';

    memmove(buffer.content + lineStart, buffer.content + nextLineStart, nextLineLen);
    buffer.content[lineStart + nextLineLen] = '\n';
    memcpy(buffer.content + lineStart + nextLineLen + 1, temp, lineLen);

    SetCursorGlobalPos(lineStart + nextLineLen + cursor.lineOffset + 1);

    ArenaPop(&fontArena, lineLen + 1);
}

void SwapLineUp()
{
    i32 cursorPosition = cursor.global;

    i32 lineStart = FindLineStart(cursorPosition);
    if (lineStart == 0)
        return; // First line, nothing to swap

    i32 prevLineEnd = lineStart - 1;
    i32 prevLineStart = FindLineStart(prevLineEnd);

    i32 prevLineLen = prevLineEnd - prevLineStart;
    i32 lineEnd = FindLineEnd(lineStart);
    i32 lineLen = lineEnd - lineStart;

    char *temp = ArenaPush(&fontArena, lineLen + 1);

    memcpy(temp, buffer.content + lineStart, lineLen);
    temp[lineLen] = '\0';

    memmove(buffer.content + prevLineStart + lineLen + 1, buffer.content + prevLineStart, prevLineLen);
    memcpy(buffer.content + prevLineStart, temp, lineLen);
    buffer.content[prevLineStart + lineLen] = '\n';

    SetCursorGlobalPos(prevLineStart + cursor.lineOffset);

    ArenaPop(&fontArena, lineLen + 1);
}

void LoadFile(i32 fileNumber)
{
    if (currentFile != fileNumber && fileNumber < ArrayLength(files))
    {
        if (buffer.content)
        {
            if (!isSaved)
                SaveFile();

            VirtualFreeMemory(buffer.content);
        }

        buffer = ReadFileIntoDoubledSizedBuffer(files[fileNumber]);
        scrollOffset.target = 0;
        scrollOffset.current = 0;
        SetCursorGlobalPos(0);
        OnTextChanged();
        currentFile = fileNumber;
        isSaved = 1;
    }
}

void InsertCharAtCurrentPosition(char ch)
{
    InsertCharAt(&buffer, cursor.global, ch);
    SetCursorGlobalPos(cursor.global + 1);

    OnTextChanged();
    SaveFile();
    Recompile();
}

char whitespaceChars[] = {' ', '\n', ':', '.', '(', ')'};

u32 IsWhitespace(char ch)
{
    for (i32 i = 0; i < ArrayLength(whitespaceChars); i++)
    {
        if (whitespaceChars[i] == ch)
            return 1;
    }

    return 0;
}

void JumpWordBack()
{
    i32 i = cursor.global == 0 ? 0 : cursor.global - 1;

    while (i > 0 && IsWhitespace(buffer.content[i]))
        i--;

    while (i > 0 && !IsWhitespace(buffer.content[i - 1]))
        i--;

    SetCursorGlobalPos(i);
}

void JumpWordForward()
{
    i32 i = cursor.global;
    while (i < buffer.size && !IsWhitespace(buffer.content[i]))
        i++;

    while (i < buffer.size && IsWhitespace(buffer.content[i]))
        i++;

    SetCursorGlobalPos(i);
}

void JumpToStartOfFile()
{
    i32 firstLineLen = FindLineEnd(0);

    SetCursorGlobalPos(MinI32(firstLineLen, cursor.lineOffset));
}

void JumpToEndOfFile()
{
    i32 lastLineStart = FindLineStart(buffer.size);

    SetCursorGlobalPos(MinI32(lastLineStart + cursor.lineOffset, buffer.size));
}

void JumpToStartOfLine()
{
    SetCursorGlobalPos(FindLineStart(cursor.global));
}

void JumpToEndOfLine()
{
    SetCursorGlobalPos(FindLineEnd(cursor.global));
}

void JumpToFirstNonBlackCharOfLine()
{
    i32 start = FindLineStart(cursor.global);

    while (buffer.content[start] == ' ')
        start++;

    SetCursorGlobalPos(start);
}

void InsertNewLineAbove()
{
    i32 identation = GetIdentationLevel();
    i32 lineStart = FindLineStart(cursor.global);
    InsertCharAt(&buffer, lineStart, '\n');
    OnTextChanged();
    SetCursorGlobalPos(lineStart);
    mode = Insert;
    isJustMovedToInsert = 1;

    while (identation > 0)
    {
        InsertCharAtCurrentPosition(' ');
        identation--;
    }
}

void InsertNewLineBelow()
{
    i32 lineEnd = FindLineEnd(cursor.global);
    i32 identation = GetIdentationLevel();

    i32 target = lineEnd;
    if (lineEnd != buffer.size)
        target++;

    InsertCharAt(&buffer, target, '\n');
    OnTextChanged();
    SetCursorGlobalPos(lineEnd + 1);
    mode = Insert;
    isJustMovedToInsert = 1;

    while (identation > 0)
    {
        InsertCharAtCurrentPosition(' ');
        identation--;
    }
}

inline f32 lerp(f32 from, f32 to, f32 factor)
{
    return from * (1 - factor) + to * factor;
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

// Antialised fonts only
inline void CopyBitmapRectTo(MyBitmap *sourceT, u32 offsetX, i32 offsetY, u32 color)
{
    u32 *row = (u32 *)canvas.pixels + offsetX + offsetY * canvas.width;
    u32 *source = (u32 *)sourceT->pixels + sourceT->width * (sourceT->height - 1);
    for (i32 y = 0; y < sourceT->height; y += 1)
    {
        u32 *pixel = row;
        u32 *sourcePixel = source;
        for (i32 x = 0; x < sourceT->width; x += 1)
        {
            // stupid fucking logic needs to replaced
            if (*sourcePixel != 0 &&
                (y + offsetY) > 0 &&
                (x + offsetX) > 0 &&
                y + offsetY < canvas.height &&
                x + offsetX < canvas.width)
                // *pixel = *sourcePixel;
                *pixel = AlphaBlendGreyscale(*pixel, *sourcePixel, color);

            sourcePixel += 1;
            pixel += 1;
        }
        source -= sourceT->width;
        row += canvas.width;
    }
}

void RenderTextLine(char *str, i32 len, i32 x, i32 y, u32 color)
{
    for (i32 i = 0; i < len; i++)
    {
        MyBitmap *bitmap = &font.textures[*(str + i)];
        if (x >= 0 && x - bitmap->width < view.x && y >= 0 && y - bitmap->height < view.y)
            CopyBitmapRectTo(bitmap, x, y, color);

        x += font.charWidth;
    }
}

void DrawScrollBar()
{
    if ((f32)view.y < pageHeight)
    {
        f32 scrollbarHeight = ((f32)view.y * (f32)view.y) / pageHeight;

        f32 maxOffset = pageHeight - view.y;
        f32 maxScrollY = (f32)view.y - scrollbarHeight;
        f32 scrollY = lerp(0, maxScrollY, (f32)scrollOffset.current / maxOffset);

        f32 scrollWidth = 10;
        PaintRect(view.x - scrollWidth, scrollY, scrollWidth, (i32)scrollbarHeight, 0xff888888);
    }
}

f32 clamp(f32 val, f32 min, f32 max)
{
    if (val < min)
        return min;
    if (val > max)
        return max;
    return val;
}

f32 ClampOffset(f32 val)
{
    f32 maxOffset = MaxF32(pageHeight - view.y, 0);
    return clamp(val, 0, maxOffset);
}

void ScrollIntoView()
{
    i32 itemsToLookAhead = 3;

    i32 lineHeightPx = RoundI32((f32)font.charHeight * lineHeight);

    i32 cursorY =
        cursor.line * lineHeightPx + padding;

    i32 spaceToLookAhead = lineHeightPx * itemsToLookAhead;

    if (view.y < linesCount * lineHeightPx)
    {
        if (
            cursorY + spaceToLookAhead + (i32)lineHeightPx - view.y >
            scrollOffset.target)
        {
            scrollOffset.target = ClampOffset(cursorY - view.y + spaceToLookAhead);
        }
        else if (cursorY - spaceToLookAhead < scrollOffset.target)
        {
            i32 targetOffset = cursorY - spaceToLookAhead;
            scrollOffset.target = ClampOffset(targetOffset);
        }
    }
}

void CenterOnRowIfNotVisible()
{
    i32 itemsToLookAhead = 3;

    i32 lineHeightPx = RoundI32((f32)font.charHeight * lineHeight);

    i32 cursorY =
        cursor.line * lineHeightPx + padding;

    i32 spaceToLookAhead = lineHeightPx * itemsToLookAhead;

    if (view.y < linesCount * lineHeightPx)
    {
        if (
            cursorY + spaceToLookAhead + (i32)lineHeightPx - view.y >
            scrollOffset.target)
        {
            scrollOffset.target = ClampOffset(cursorY - view.y / 2);
        }
        else if (cursorY - spaceToLookAhead < scrollOffset.target)
        {
            scrollOffset.target = ClampOffset(cursorY - view.y / 2);
        }
    }
}

void ClearToBg()
{
    u32 *bytes = canvas.pixels;
    u32 count = canvas.width * canvas.height;
    while (count--)
    {
        *bytes++ = colorScheme.bg;
    }
}

void Copy(HWND window)
{
    i32 start;
    i32 end;
    if (selectionStart == -1)
    {
        start = FindLineStart(cursor.global);
        end = FindLineEnd(cursor.global) + 1;
    }
    else
    {
        start = MinI32(selectionStart, cursor.global);
        end = MaxI32(selectionStart, cursor.global);
    }
    SetClipboard(window, &buffer.content[start], end - start);
}

i32 leftPadding;

void PaintText(i32 line, i32 offset, i32 len, u32 color)
{
    f32 offsetY = ((lineHeight - 1.0f) / 2.0f) * font.charHeight;
    i32 lineHeightPx = RoundI32((f32)font.charHeight * lineHeight);

    PaintRect(leftPadding + offset * font.charWidth, padding + line * lineHeightPx - offsetY - scrollOffset.current, len * font.charWidth, lineHeightPx, colorScheme.selectionBg);
}

char testLabel[255] = "hello";
char bufferFacisto[4096];

inline void PaintViewNumber(MyRect2 rect, i32 val, u32 color)
{
    i32 padding = 10;
    CopyBitmapRectTo(&bigFont.textures['0' + val],
                     rect.x + rect.width - bigFont.charWidth - padding - 4,
                     rect.y + padding,
                     color);
}

char *entries[] = {
    "error C2146: syntax error: missing ';' before identifier 'PaintAppRect' and some of the move involved calamities has been traced to your source code",
    "foo",
    "error C2146: syntax error: missing ';' before identifier 'PaintAppRect'",
    "bar",
    "bar",
    "buzz",
};

void Draw2()
{
    MyRect2 screenRect = {0, 0, view.x, view.y};

    MyRect2 left = {0, 0, view.x / 2, view.y};

    MyRect2 right = {view.x / 2, 0, view.x / 2, view.y};

    i32 appFooterHeight = 40;

    MyRect2 main = ShrinkFromBottom(left, appFooterHeight);
    PaintAppRect(left, 0x992222);
    PaintViewNumber(left, 1, 0xffffff);

    MyRect2 footer = AppendAfterBottom(main, appFooterHeight);
    PaintAppRect(footer, 0x225522);

    PaintAppRect(right, 0x221122);
    PaintViewNumber(right, 2, 0x888888);

    i32 rowPadding = 5;
    i32 leftPadding = 5;
    i32 startX = right.x + leftPadding;
    i32 startY = right.y + rowPadding;
    i32 x = startX;
    i32 y = startY;

    for (i32 i = 0; i < consoleBufferLen; i++)
    {
        char ch = consoleBuffer[i];
        if (ch != '\n')
        {
            CopyBitmapRectTo(&font.textures[ch], x, y, 0xffffff);
            x += font.charWidth;

            if (ch == ' ')
            {
                i32 nextSpace = i + 1;
                while (consoleBuffer[nextSpace] != '\0' && consoleBuffer[nextSpace] != ' ')
                    nextSpace++;

                if (x + (nextSpace - i) * font.charWidth >= right.x + right.width)
                {
                    x = startX;
                    y += font.charHeight;
                }
            }
            ch++;
        }

        else
        {

            x = startX;
            y += font.charHeight + rowPadding;

            if (i != ArrayLength(entries) - 1)
            {
                PaintRect(right.x, y - 1, right.width, 2, 0x552255);
            }
            y += rowPadding;
        }
    }
}

void DrawConsoleOutput()
{
    i32 consoleX = view.x / 2;
    i32 consoleY = view.y / 2;

    i32 lineHeightPx = RoundI32((f32)font.charHeight * lineHeight);

    for (i32 i = 0; i < consoleBufferLen; i++)
    {
        char ch = consoleBuffer[i];
        if (ch == '\n')
        {
            consoleX = view.x / 2;
            consoleY += RoundI32(font.charHeight * lineHeight);
        }
        else
        {

            MyBitmap *bitmap = &font.textures[ch];
            i32 by = consoleY;
            if (by > -lineHeightPx && by < view.y)
                CopyBitmapRectTo(bitmap, consoleX, by, colorScheme.font);
            consoleX += font.charWidth;
        }
    }
}

void Draw()
{
    u64 end = GetPerfCounter();

    f32 deltaMs = ((f32)(end - start) / frequency * 1000);

    UpdateSpring(&scrollOffset, deltaMs / 1000);
    ClearToBg();

    MyRect2 screenRect = {0, 0, view.x, view.y};

    MyRect2 left = {0, 0, view.x / 2, view.y};

    MyRect2 right = {view.x / 2, 0, view.x / 2, view.y};
    // Draw2();

    DrawConsoleOutput();

    // StretchDIBits(dc, 0, 0, view.x, view.y, 0, 0, view.x, view.y, canvas.pixels, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
    // return;

    i32 linesCountTemp = linesCount;
    i32 digitsForLines = 0;
    while (linesCountTemp > 0)
    {
        digitsForLines++;
        linesCountTemp /= 10;
    }

    char *bufferP = buffer.content;

    leftPadding = padding + font.charWidth * 2 + digitsForLines * font.charWidth;

    i32 x = leftPadding;
    i32 y = padding;

    i32 charRendered = 0;

    u32 color = colorScheme.font;

    u32 currentSearchEntryIndex = 0;
    EntryFound *found = NULL;

    i32 lineHeightPx = RoundI32((f32)font.charHeight * lineHeight);

    f32 offsetY = ((lineHeight - 1.0f) / 2.0f) * font.charHeight;

    u32 cursorX = leftPadding + cursor.lineOffset * font.charWidth - 1;
    u32 cursorY = padding + cursor.line * lineHeightPx - offsetY - scrollOffset.current;

    PaintRect(0, cursorY, left.width, lineHeightPx, colorScheme.currentLineBg);

    if (selectionStart != -1)
    {
        u32 selectionLeft = MinI32(selectionStart, cursor.global);
        u32 selectionRight = MaxI32(selectionStart, cursor.global);

        CursorPos startPos = GetCursorPositionForGlobal(selectionLeft);
        CursorPos endPos = GetCursorPositionForGlobal(selectionRight);

        i32 len = GetLineLength(startPos.line);
        i32 maxLen = len - startPos.lineOffset;
        i32 firstLineLen = MinI32(selectionRight - selectionLeft, maxLen);
        PaintText(startPos.line, startPos.lineOffset, firstLineLen, colorScheme.selectionBg);

        if (startPos.line != endPos.line)
        {
            for (i32 l = startPos.line + 1; l < endPos.line; l++)
            {
                PaintText(l, 0, GetLineLength(l), colorScheme.selectionBg);
            }

            PaintText(endPos.line, 0, endPos.lineOffset, colorScheme.selectionBg);
        }
    }

    for (i32 i = 0; i < buffer.size; i++)
    {

        if (currentSearchEntryIndex < entriesCount)
            found = &entriesAt[currentSearchEntryIndex];
        else
            found = NULL;

        if (mode == Search && found && charRendered >= found->at && charRendered < found->at + found->len)
        {
            if (currentSearchEntryIndex == currentEntry)
                color = colorScheme.searchResultActive;
            else
                color = colorScheme.searchResult;
        }
        else
        {
            color = colorScheme.font;
        }

        char ch = buffer.content[i];
        // char ch = *bufferP;

        if (ch == '\n')
        {
            x = leftPadding;
            y += lineHeightPx;
        }
        else
        {
            MyBitmap *bitmap;
            if (*bufferP >= ' ' && *bufferP < MAX_CHAR_CODE)
            {
                bitmap = &font.textures[*bufferP];
                i32 by = y - scrollOffset.current;
                if (by > -lineHeightPx && by < view.y)
                    CopyBitmapRectTo(bitmap, x, y - scrollOffset.current, color);
            }
            else
            {
                PaintRect(x, y - scrollOffset.current, font.charWidth, font.charHeight, 0xff0000);
            }

            x += font.charWidth;
        }

        bufferP++;
        charRendered++;

        if (found && charRendered >= found->at + found->len)
        {
            currentSearchEntryIndex++;
        }
    }

    char digits[20] = {0};
    i32 lineLength = 0;
    for (i32 i = 0; i < linesCount; i++)
    {
        u32 color = (i == cursor.line) ? colorScheme.lineCurrent : colorScheme.line;

        lineLength = AppendI32(i + 1, digits);
        for (i32 j = 0; j < lineLength; j++)
        {

            MyBitmap *bitmap = &font.textures[digits[j]];
            i32 by = padding + i * lineHeightPx - scrollOffset.current;
            if (by > -lineHeightPx && by < view.y)
                CopyBitmapRectTo(bitmap, leftPadding - font.charWidth * (3 + (lineLength - j - 1)), by, color);
        }
    }

    u32 cursorColor = mode == Normal   ? 0xff22ff22
                      : mode == Insert ? 0xffff2222
                                       : 0xff777777;

    PaintRect(cursorX, cursorY, 2, lineHeightPx, cursorColor);

    char label[255] = {0};
    u32 pos = 0;
    if (isSaved)
        pos += AppendStr("saved ", label + pos);
    else
        pos += AppendStr("not saved ", label + pos);

    pos += AppendStr("fontArena:", label + pos);
    pos += AppendI32(fontArena.bytesAllocated / 1024, label + pos);
    pos += AppendStr("kB ", label + pos);

    pos += AppendStr("pos:", label + pos);
    pos += AppendI32(cursor.global, label + pos);
    pos += AppendStr(" ", label + pos);

    pos += AppendStr("line:", label + pos);
    pos += AppendI32(cursor.line, label + pos);
    pos += AppendStr(" ", label + pos);

    pos += AppendStr("offset:", label + pos);
    pos += AppendI32(cursor.lineOffset, label + pos);
    pos += AppendStr(" ", label + pos);

    pos += AppendStr("capacity:", label + pos);
    pos += AppendI32(buffer.capacity, label + pos);
    pos += AppendStr(" size:", label + pos);
    pos += AppendI32(buffer.size, label + pos);
    pos += AppendStr(" ", label + pos);

    if (deltaMs < 10)
        pos += AppendStr(" ", label + pos);

    pos += AppendI32((i32)deltaMs, label + pos);
    pos += AppendStr("ms", label + pos);

    i32 footerPadding = 5;
    i32 footerHeight = font.charHeight + footerPadding * 2;
    PaintRect(0, view.y - footerHeight, view.x, footerHeight, colorScheme.footerBg);
    RenderTextLine(label, pos - 1, view.x - 5 - pos * font.charWidth, view.y - 5 - font.charHeight, colorScheme.font);
    char *filename = files[currentFile];
    RenderTextLine(filename, mystrlen(filename), footerPadding, view.y - font.charHeight - footerPadding, 0xffffff);

    if (mode == Search)
    {
        RenderTextLine(searchTerm, searchLen, view.x - 200, 4, colorScheme.searchResult);

        pos = 0;
        pos += AppendI32(currentEntry + 1, label + pos);
        pos += AppendStr(" of ", label + pos);
        pos += AppendI32(entriesCount, label + pos);

        RenderTextLine(label, pos, view.x - (pos + 1) * font.charWidth - 4, 4, colorScheme.font);
    }

    RenderTextLine(testLabel, mystrlen(testLabel), view.x - (mystrlen(testLabel) + 2) * font.charWidth, view.y - 100, colorScheme.font);

    pos = 0;
    pos += AppendStr("compile:", label + pos);
    u32 compileMc = (u32)((f32)(compileTime) / frequency * 1000);
    pos += AppendI32(compileMc, label + pos);
    pos += AppendStr("ms", label + pos);

    RenderTextLine(label, pos, view.x - (pos + 2) * font.charWidth, view.y - 200, colorScheme.font);
    DrawScrollBar();

    if (render)
        render(&canvas, &right);

    StretchDIBits(dc, 0, 0, view.x, view.y, 0, 0, view.x, view.y, canvas.pixels, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);

    start = end;
}

void OnEnter()
{
    i32 identation = GetIdentationLevel();
    InsertCharAtCurrentPosition('\n');

    while (identation > 0)
    {
        InsertCharAtCurrentPosition(' ');
        identation--;
    }
}

// TODO: this about how to extract this into win32, maybe use Arena to copy data there before closing clipboard
void PasteFromClipboard(HWND window)
{
    OpenClipboard(window);
    HANDLE hClipboardData = GetClipboardData(CF_TEXT);
    char *pchData = (char *)GlobalLock(hClipboardData);
    if (pchData)
    {

        i32 len = mystrlen(pchData);
        InsertChars(&buffer, pchData, len, cursor.global);
        SetCursorGlobalPos(cursor.global + len);
        GlobalUnlock(hClipboardData);

        OnTextChanged();
    }
    else
    {
        OutputDebugStringA("Failed to capture clipboard\n");
    }
    CloseClipboard();
    ScrollIntoView();
}

typedef char *GetFoo();

#define BUFSIZE 4096

HANDLE g_hChildStd_OUT_Rd = NULL;
HANDLE g_hChildStd_OUT_Wr = NULL;

void ReadFromPipe(void)

// Read output from the child process's pipe for STDOUT
// and write to the parent process's pipe for STDOUT.
// Stop when there is no more data.
{
    DWORD dwRead, dwWritten;
    CHAR chBuf[BUFSIZE];
    BOOL bSuccess = FALSE;
    HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

    for (;;)
    {
        bSuccess = ReadFile(g_hChildStd_OUT_Rd, chBuf, BUFSIZE, &dwRead, NULL);
        if (!bSuccess || dwRead == 0)
            break;

        bSuccess = WriteFile(hParentStdOut, chBuf,
                             dwRead, &dwWritten, NULL);
        if (!bSuccess)
            break;
    }
}

LRESULT OnEvent(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    HKL keyboardLayout = GetKeyboardLayout(0);
    BYTE keyState[256];

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

    case WM_CHAR:
        if (mode == Insert)
        {
            if (isJustMovedToInsert)
                isJustMovedToInsert = 0;
            else if (wParam == '\r' || wParam == '\n')
                OnEnter();
            else if (wParam >= ' ')
                InsertCharAtCurrentPosition(wParam);
        }

        if (mode == Search)
        {
            if (isJustMovedToInsert)
                isJustMovedToInsert = 0;
            else if (wParam >= ' ')
            {
                searchTerm[searchLen++] = wParam;
                FindEntries();

                SetCursorGlobalPos(entriesAt[currentEntry].at);
                CenterOnRowIfNotVisible();
            }
        }

        break;

    case WM_MOUSEWHEEL:
        // zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (view.y < pageHeight)
            scrollOffset.target = ClampOffset(scrollOffset.target - GET_WHEEL_DELTA_WPARAM(wParam));
        break;

    case WM_SYSCOMMAND:
        if (wParam == SC_KEYMENU)
        {
            return 0;
        }
        break;

    case WM_CLIPBOARDUPDATE:
        // MessageBoxA(NULL, "Clipboard updated!", "Notification", MB_OK);
        break;

    case WM_SYSKEYDOWN:
        if (wParam == VK_MENU)
            isAltPressed = 1;

        if (mode == Normal)
        {
            if (isAltPressed && wParam == 'J')
                SwapLineDown();
            else if (isAltPressed && wParam == 'K')
                SwapLineUp();
        }
        else if (mode == Insert)
        {
            if (wParam == 'P' && isAltPressed)
            {
                PasteFromClipboard(window);
            }
        }
        else if (mode == Search)
        {
            if (isAltPressed && wParam == 'J')
            {
                if (currentEntry < entriesCount - 1)
                {
                    currentEntry++;
                    SetCursorGlobalPos(entriesAt[currentEntry].at);
                    CenterOnRowIfNotVisible();
                }
            }
            else if (isAltPressed && wParam == 'K')
            {
                if (currentEntry > 0)
                {
                    currentEntry--;
                    SetCursorGlobalPos(entriesAt[currentEntry].at);
                    CenterOnRowIfNotVisible();
                }
            }
        }

        break;
    case WM_KEYDOWN:

        if (wParam == VK_CONTROL)
            isCtrlPressed = 1;
        if (wParam == VK_SHIFT)
            isShiftPressed = 1;

        if (wParam == VK_F11)
        {
            isFullscreen = !isFullscreen;
            SetFullscreen(window, isFullscreen);
        }

        if (mode == Normal)
        {
            if (isCtrlPressed && wParam >= '1' && wParam <= '9')
                LoadFile(wParam - '1');
            if (wParam == '0')
                JumpToStartOfLine();
            if (wParam == '4' && isShiftPressed)
                JumpToEndOfLine();
            if (wParam == '6' && isShiftPressed)
                JumpToFirstNonBlackCharOfLine();

            if (wParam == 'G' && isShiftPressed)
            {
                JumpToEndOfFile();
                ScrollIntoView();
            }
            else if (wParam == 'G')
            {
                JumpToStartOfFile();
                ScrollIntoView();
            }

            if (wParam == 'O' && isShiftPressed)
                InsertNewLineAbove();
            else if (wParam == 'O')
                InsertNewLineBelow();

            if (wParam == 'L')
            {
                SetCursorGlobalPos(cursor.global + 1);
                ScrollIntoView();
            }

            if (wParam == 'H')
            {
                SetCursorGlobalPos(cursor.global - 1);
                ScrollIntoView();
            }

            if (wParam == 'Z')
                RemoveCharFromLeft();

            if (wParam == 'X')
                RemoveCharFromRight();

            if (wParam == 'J')
            {
                GoDown();
                ScrollIntoView();
            }

            if (wParam == 'Y')
            {
                Copy(window);
            }

            if (wParam == 'K')
            {
                GoUp();
                ScrollIntoView();
            }

            if (wParam == 'D')
                RemoveLine();

            if (wParam == 'W')
                JumpWordForward();

            if (wParam == 'B')
                JumpWordBack();

            if (wParam == ' ')
                InsertCharAtCurrentPosition(' ');

            if (wParam == 'I')
            {
                mode = Insert;
                isJustMovedToInsert = 1;
            }

            if (wParam == VK_RETURN)
            {
                OnEnter();
            }

            if (wParam == VK_BACK)
            {
                if (cursor.global > 0)
                    RemoveCharFromLeft();
            }

            if (wParam == 'S' && isCtrlPressed)
            {
                SaveFile();

                Recompile();
            }

            if ((wParam == 'F' && isCtrlPressed))
            {
                mode = Search;
                entriesCount = 0;
                searchLen = 0;
            }

            if (wParam == 'P')
            {
                PasteFromClipboard(window);
            }
        }
        else if (mode == Insert)
        {
            if (wParam == VK_ESCAPE || (wParam == 'I' && isCtrlPressed))
            {
                mode = Normal;
            }

            if (wParam == VK_BACK)
            {
                if (cursor.global > 0)
                    RemoveCharFromLeft();
            }
        }
        else if (mode == Search)
        {
            if (wParam == VK_ESCAPE || (wParam == 'F' && isCtrlPressed))
            {
                mode = Normal;
            }

            if (wParam == VK_BACK && searchLen > 0)
            {
                searchLen--;
                FindEntries();

                SetCursorGlobalPos(entriesAt[currentEntry].at);
                CenterOnRowIfNotVisible();
            }
        }

        break;

    case WM_KEYUP:
        if (wParam == VK_CONTROL)
            isCtrlPressed = 0;
        if (wParam == VK_SHIFT)
            isShiftPressed = 0;

        break;

    case WM_SYSKEYUP:
        if (wParam == VK_MENU)
            isAltPressed = 0;

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

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    i32 res = CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0);

    // CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    HINSTANCE instance = GetModuleHandle(0);

    fontArena = CreateArena(MB(2));

    InitAnimations();
    InitFontSystem();

    SetupFonts();

    LoadFile(0);
    //    buffer = ReadFileIntoDoubledSizedBuffer(filename);
    timeBeginPeriod(1);
    OnTextChanged();

    FindEntries();
    isSaved = 1;

    frequency = GetPerfFrequency();
    start = GetPerfCounter();

    HWND window = OpenWindow(OnEvent, colorScheme.bg, "Editor");

    AddClipboardFormatListener(window);

    dc = GetDC(window);

    SetCursorGlobalPos(entriesAt[currentEntry].at);
    ScrollIntoView();

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
#pragma once

#include <windows.h>
#include "math.c"
#include "win32.c"
#include "arena.c"

// this dependency is questionable
// #include <gl/gl.h>

// this dependency is questionable
// #include "layout.c"

#define MAX_CHAR_CODE 126

typedef struct FontKerningPair
{
    u16 left;
    u16 right;
    i8 val;
} FontKerningPair;

typedef struct FontInfo
{
    u8 *name;
    i32 size;
    i32 isClearType;
    u32 background;
    u32 foreground;
} FontInfo;

inline FontInfo FontInfoClearType(char *name, i32 size, u32 fg, u32 bg)
{
    return (FontInfo){.name = name, .size = size, .background = bg, .foreground = fg, .isClearType = 1};
}

inline FontInfo FontInfoAntialiased(char *name, i32 size)
{
    return (FontInfo){.name = name, .size = size, .background = 0x000000, .foreground = 0xffffff, .isClearType = 0};
}

void InitBitmapInfo(BITMAPINFO *bitmapInfo, u32 width, u32 height)
{
    bitmapInfo->bmiHeader.biSize = sizeof(bitmapInfo->bmiHeader);
    bitmapInfo->bmiHeader.biBitCount = 32;
    bitmapInfo->bmiHeader.biWidth = width;
    bitmapInfo->bmiHeader.biHeight = height; // makes rows go up, instead of going down by default
    bitmapInfo->bmiHeader.biPlanes = 1;
    bitmapInfo->bmiHeader.biCompression = BI_RGB;
}

typedef struct FontData
{
    MyBitmap textures[MAX_CHAR_CODE];
    // GLuint cachedTextures[MAX_CHAR_CODE];

    // Need to use ABC structure for this
    // https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-getcharabcwidthsa

    TEXTMETRIC textMetric;
    u32 isMonospaced;
    u32 charWidth;
    u32 charHeight;
    FontKerningPair *pairsHash;
} FontData;

FontData *currentFont;

// Segoe UI has around 8k pairs, monospace has none pairs
#define PAIRS_HASH_LENGTH 16 * 1024
inline int HashAndProbeIndex(const FontData *font, u16 left, u16 right)
{
    i32 keysMask = PAIRS_HASH_LENGTH - 1;
    i32 index = (left * 19 + right * 7) & keysMask;
    int i = 1;
    while (font->pairsHash[index].left != 0 && font->pairsHash[index].left != left && font->pairsHash[index].right != right)
    {
        index += (i * i);
        i++;
        if (index >= PAIRS_HASH_LENGTH)
            index = index & keysMask;
    }
    return index;
}

// takes dimensions of destinations, reads rect from source at (0,0)
inline void CopyRectTo(MyBitmap *sourceT, MyBitmap *destination, FontInfo info)
{
    u32 *row = (u32 *)destination->pixels + destination->width * (destination->height - 1);
    u32 *source = (u32 *)sourceT->pixels + sourceT->width * (sourceT->height - 1);
    for (u32 y = 0; y < destination->height; y += 1)
    {
        u32 *pixel = row;
        u32 *sourcePixel = source;
        for (u32 x = 0; x < destination->width; x += 1)
        {
            // u8 r = (*sourcePixel & 0xff0000) >> 16;
            // u8 g = (*sourcePixel & 0x00ff00) >> 8;
            // u8 b = (*sourcePixel & 0x0000ff) >> 0;

            if (info.isClearType)
            {
                // u32 alpha = 0xff000000;
                // u32 alpha = 0x00000000;
                u32 alpha = *sourcePixel == info.background ? 0x00000000 : 0xff000000;
                *pixel = *sourcePixel | alpha;
            }
            else
            {
                // taking one channel and making alpha as a difference. I assume all colors are grey if not clear type. Black bg is also assumed
                u32 alpha = (*sourcePixel & 0xff) << 24;
                // u32 alpha = *sourcePixel == info.background ? 0x00000000 : 0xff000000;
                *pixel = *sourcePixel | alpha;
            }
            sourcePixel += 1;
            pixel += 1;
        }
        source -= sourceT->width;
        row -= destination->width;
    }
}
HDC deviceContext;
MyBitmap fontCanvas;
HBITMAP bitmap;
void InitFontSystem()
{
    deviceContext = CreateCompatibleDC(0);
    BITMAPINFO info = {0};
    int textureSize = 256;
    InitBitmapInfo(&info, textureSize, textureSize);

    void *bits;
    bitmap = CreateDIBSection(deviceContext, &info, DIB_RGB_COLORS, &bits, 0, 0);
    fontCanvas = (MyBitmap){.bytesPerPixel = 4, .height = textureSize, .width = textureSize, .pixels = bits};
}

void InitFontData(FontData *fontData, FontInfo fontInfo, Arena *arena)
{
    int h = -MulDiv(fontInfo.size, GetDeviceCaps(deviceContext, LOGPIXELSY), USER_DEFAULT_SCREEN_DPI);
    HFONT font = CreateFontA(h, 0, 0, 0,
                             FW_DONTCARE, // Weight
                             0,           // Italic
                             0,           // Underline
                             0,           // Strikeout
                             DEFAULT_CHARSET,
                             OUT_TT_ONLY_PRECIS,
                             CLIP_DEFAULT_PRECIS,

                             // I've experimented with the Chrome and it doesn't render LCD quality for fonts above 32px
                             fontInfo.isClearType ? CLEARTYPE_QUALITY : ANTIALIASED_QUALITY,

                             DEFAULT_PITCH,
                             fontInfo.name);

    SelectObject(deviceContext, bitmap);
    SelectObject(deviceContext, font);

    i32 kerningPairCount = GetKerningPairsW(deviceContext, 0, 0);

    if (kerningPairCount > 1)
    {

        fontData->pairsHash = (FontKerningPair *)ArenaPush(arena, PAIRS_HASH_LENGTH * sizeof(FontKerningPair));

        i32 pairsSizeAllocated = sizeof(KERNINGPAIR) * kerningPairCount;
        KERNINGPAIR *pairs = VirtualAllocateMemory(pairsSizeAllocated);
        GetKerningPairsW(deviceContext, kerningPairCount, pairs);

        for (int i = 0; i < kerningPairCount; i++)
        {
            KERNINGPAIR *pair = pairs + i;
            i32 index = HashAndProbeIndex(fontData, pair->wFirst, pair->wSecond);

            fontData->pairsHash[index].left = pair->wFirst;
            fontData->pairsHash[index].right = pair->wSecond;
            fontData->pairsHash[index].val = pair->iKernAmount;
        }
        VirtualFreeMemory(pairs);
    }

    // The high-order byte must be zero
    // https://learn.microsoft.com/en-us/windows/win32/gdi/colorref
    fontInfo.background &= 0x00ffffff;
    fontInfo.foreground &= 0x00ffffff;
    SetBkColor(deviceContext, fontInfo.background);
    SetTextColor(deviceContext, fontInfo.foreground);

    SIZE size;
    for (wchar_t ch = ' '; ch < MAX_CHAR_CODE; ch += 1)
    {
        int len = 1;
        GetTextExtentPoint32W(deviceContext, &ch, len, &size);

        TextOutW(deviceContext, 0, 0, &ch, len);

        MyBitmap *texture = &fontData->textures[ch];
        texture->width = size.cx;
        texture->height = size.cy;
        texture->bytesPerPixel = 4;

        texture->pixels = (u32 *)ArenaPush(arena, texture->height * texture->width * texture->bytesPerPixel);

        CopyRectTo(&fontCanvas, texture, fontInfo);
    }

    // {
    //     wchar_t dotCh = 0xb7; // ·
    //     int len = 1;
    //     GetTextExtentPoint32W(deviceContext, &dotCh, len, &size);

    //     TextOutW(deviceContext, 0, 0, &dotCh, len);

    //     MyBitmap *texture = &fontData->textures[1];
    //     texture->width = size.cx;
    //     texture->height = size.cy;
    //     texture->bytesPerPixel = 4;

    //     texture->pixels = (u32 *)ArenaPush(arena, texture->height * texture->width * texture->bytesPerPixel);

    //     CopyRectTo(&fontCanvas, texture, fontInfo);
    // }

    // {
    //     wchar_t dotCh = 0x00B6; // ¶
    //     int len = 1;
    //     GetTextExtentPoint32W(deviceContext, &dotCh, len, &size);

    //     TextOutW(deviceContext, 0, 0, &dotCh, len);

    //     MyBitmap *texture = &fontData->textures[2];
    //     texture->width = size.cx;
    //     texture->height = size.cy;
    //     texture->bytesPerPixel = 4;

    //     texture->pixels = (u32 *)ArenaPush(arena, texture->height * texture->width * texture->bytesPerPixel);

    //     CopyRectTo(&fontCanvas, texture, fontInfo);
    // }

    GetTextMetricsA(deviceContext, &fontData->textMetric);

    if (fontData->textures['i'].width == fontData->textures['W'].width)
    {
        fontData->isMonospaced = 1;
        fontData->charWidth = fontData->textures['W'].width;
    }
    fontData->charHeight = fontData->textMetric.tmHeight;

    // DeleteObject(bitmap);
    DeleteObject(font);
    // DeleteDC(deviceContext);
}

void InitFont(FontData *font, FontInfo info, Arena *arena)
{
    InitFontData(font, info, arena);
}

inline int GetKerningValue(u16 left, u16 right)
{
    if (currentFont->isMonospaced)
        return 0;

    i32 index = HashAndProbeIndex(currentFont, left, right);

    if (currentFont->pairsHash[index].left != left && currentFont->pairsHash[index].right != right)
    {
        return 0;
    }

    return currentFont->pairsHash[index].val;
}

i32 GetTextWidth(const u8 *text)
{
    i32 res = 0;
    while (*text)
    {
        u8 ch = *text;
        // skips new lines and other control symbols
        if (ch >= ' ')
            res += currentFont->textures[ch].width + (GetKerningValue(ch, *(text + 1)));

        text++;
    }
    return res;
}
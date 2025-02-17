#pragma once
#include <windows.h>
#include "win32.c"

#pragma pack(push, 1)
typedef struct
{
    WORD bfType;      // specifies the file type
    DWORD bfSize;     // specifies the size in bytes of the bitmap file
    WORD bfReserved1; // reserved; must be 0
    WORD bfReserved2; // reserved; must be 0
    DWORD bfOffBits;  // specifies the offset in bytes from the bitmapfileheader to the bitmap bits

    DWORD biSize;         // specifies the number of bytes required by the struct
    LONG biWidth;         // specifies width in pixels
    LONG biHeight;        // specifies height in pixels
    WORD biPlanes;        // specifies the number of color planes, must be 1
    WORD biBitCount;      // specifies the number of bits per pixel
    DWORD biCompression;  // specifies the type of compression
    DWORD biSizeImage;    // size of image in bytes
    LONG biXPelsPerMeter; // number of pixels per meter in x axis
    LONG biYPelsPerMeter; // number of pixels per meter in y axis
    DWORD biClrUsed;      // number of colors used by the bitmap
    DWORD biClrImportant; // number of colors that are important

    u32 rMask;
    u32 gMask;
    u32 bMask;

} MyBMPFileFormat;
#pragma pack(pop)

inline u32 GetShift(u32 value)
{
    u32 res = 0;
    _BitScanForward((DWORD *)&res, value);
    return res;

    // if (!value)
    //     return 0;

    // while (!(value & 1))
    // {
    //     res += 1;
    //     value = value >> 1;
    // }

    // return res;
}
void ParseBmpFile(FileContent *fileInfo, MyBitmap *texture)
{

    MyBMPFileFormat *file = (MyBMPFileFormat *)fileInfo->content; //(input->readFile(path)).content;

    texture->width = file->biWidth;
    texture->height = file->biHeight;
    texture->pixels = (u32 *)((char *)file + file->bfOffBits);
    texture->bytesPerPixel = 4;

    u32 alphaMask = ~(file->rMask | file->gMask | file->bMask);

    u32 alphaShift = GetShift(alphaMask);
    u32 redShift = GetShift(file->rMask);
    u32 greenShift = GetShift(file->gMask);
    u32 blueShift = GetShift(file->bMask);

    u32 *pixel = (u32 *)texture->pixels;
    for (u32 y = 0; y < texture->height; y += 1)
    {
        for (u32 x = 0; x < texture->width; x += 1)
        {
            u32 value = *pixel;

            u32 alpha = (value & alphaMask) >> alphaShift;
            u32 red = (value & file->rMask) >> redShift;
            u32 green = (value & file->gMask) >> greenShift;
            u32 blue = (value & file->bMask) >> blueShift;
            *pixel = (alpha << 24) | (red) | (green << 8) | (blue << 16);

            pixel += 1;
        }
    }
}

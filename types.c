#pragma once
#include "util/math.c"

#define ArrayLength(array) (sizeof(array) / sizeof(array[0]))

#define Assert(cond)   \
    if (!(cond))       \
    {                  \
        *(u32 *)0 = 0; \
    }
#define Fail(msg) Assert(0)

typedef struct MyBitmap
{
    i32 width;
    i32 height;
    i32 bytesPerPixel;
    u32 *pixels;
} MyBitmap;

inline u32 Vec3fToHex(V3f vec)
{
    // Little endian reverses order
    u32 b = (u32)(vec.r * 255) << 0;
    u32 g = (u32)(vec.g * 255) << 8;
    u32 r = (u32)(vec.b * 255) << 16;
    return r | g | b;
}

void ReverseString(char *str)
{
    if (str == NULL)
        return;

    int length = 0;
    while (str[length] != '\0')
    {
        length++;
    }

    int start = 0;
    int end = length - 1;
    char temp;

    while (start < end)
    {
        // Swap characters at start and end indices
        temp = str[start];
        str[start] = str[end];
        str[end] = temp;

        // Move towards the center
        start++;
        end--;
    }
}

void Formati32(i32 val, char *buff)
{
    char *start = buff;
    if (val < 0)
    {
        *buff = '-';
        val = -val;
        buff++;
    }

    while (val != 0)
    {
        *buff = '0' + val % 10;
        val /= 10;
        buff++;
    }
    ReverseString(start);
    *buff = '\n';
    buff++;
    *buff = '\0';
}

u32 AppendI32(i32 val, char *buff)
{
    char *start = buff;
    if (val < 0)
    {
        *buff = '-';
        val = -val;
        buff++;
    }

    if (val == 0)
    {
        *buff = '0';
        buff++;
    }
    else
    {
        while (val != 0)
        {
            *buff = '0' + val % 10;
            val /= 10;
            buff++;
        }
        ReverseString(start + 1);
    }

    return buff - start;
}

// void ReverseStringLen(char *str, int len)
// {
//     if (str == NULL)
//         return;

//     int start = 0;
//     int end = len - 1;
//     char temp;

//     while (start < end)
//     {
//         // Swap characters at start and end indices
//         temp = str[start];
//         str[start] = str[end];
//         str[end] = temp;

//         // Move towards the center
//         start++;
//         end--;
//     }
// }

// int AppendNumber(i32 val, char *buff)
// {
//     int symbolsWrote = 0;
//     char *start = buff;
//     if (val < 0)
//     {
//         buff[symbolsWrote] = '-';
//         val = -val;
//         symbolsWrote++;
//     }

//     while (val != 0)
//     {
//         buff[symbolsWrote] = '0' + val % 10;
//         val /= 10;
//         symbolsWrote++;
//     }

//     ReverseString(start);
// }

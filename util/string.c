#pragma once

#include "types.c"
#include "win32.c"

typedef struct StringBuffer
{
    char *content;
    i32 size;
    i32 capacity;
} StringBuffer;

inline void PlaceLineEnd(StringBuffer *buffer)
{
    if (buffer->content)
        *(buffer->content + buffer->size) = '\0';
}

inline void MoveBytesLeft(char *ptr, int length)
{
    for (int i = 0; i < length - 1; i++)
    {
        ptr[i] = ptr[i + 1];
    }
}

inline void MoveBytesRight(char *ptr, int length)
{
    for (int i = length - 1; i > 0; i--)
    {
        ptr[i] = ptr[i - 1];
    }
}

inline void MoveMyMemory(char *source, char *dest, int length)
{
    for (int i = 0; i < length; i++)
    {
        *dest = *source;
        source++;
        dest++;
    }
}

void DoubleCapacityIfFull(StringBuffer *buffer)
{
    char *currentStr = buffer->content;
    buffer->capacity = (buffer->capacity == 0) ? 4 : (buffer->capacity * 2);
    buffer->content = VirtualAllocateMemory(buffer->capacity);
    MoveMyMemory(currentStr, buffer->content, buffer->size);
    VirtualFreeMemory(currentStr);
}

void InsertCharAt(StringBuffer *buffer, i32 at, i32 ch)
{
    if (buffer->size >= buffer->capacity)
    {
        DoubleCapacityIfFull(buffer);
    }

    buffer->size += 1;
    MoveBytesRight(buffer->content + at, buffer->size - at);
    *(buffer->content + at) = ch;
    PlaceLineEnd(buffer);
}

void RemoveCharAt(StringBuffer *buffer, i32 at)
{
    MoveBytesLeft(buffer->content + at, buffer->size - at);
    buffer->size--;
    PlaceLineEnd(buffer);
}

void RemoveChars(StringBuffer *buffer, i32 start, i32 end)
{
    char *from = buffer->content + end + 1;
    char *to = buffer->content + start;
    i32 len = buffer->size - end + 1;
    while (len >= 0)
    {
        *to = *from;

        to++;
        from++;
        len--;
    }

    // MoveBytesLeft(buffer->content + from, buffer->size - to);
    buffer->size -= (end - start + 1);
    PlaceLineEnd(buffer);
}

void InsertChars(StringBuffer *buffer, char *chars, i32 len, i32 at)
{
    while (buffer->size + len > buffer->capacity)
    {
        DoubleCapacityIfFull(buffer);
    }

    buffer->size += len;

    char *from = buffer->content + at;
    char *to = buffer->content + at + len;
    memmove(to, from, buffer->size - at);

    for (i32 i = at; i < at + len; i++)
    {
        buffer->content[i] = chars[i - at];
    }

    PlaceLineEnd(buffer);
}

StringBuffer ReadFileIntoDoubledSizedBuffer(char *path)
{
    u32 fileSize = GetMyFileSize(path);
    StringBuffer res = {
        .capacity = fileSize * 2,
        .size = fileSize,
        .content = 0};
    res.content = VirtualAllocateMemory(res.capacity);
    ReadFileInto(path, fileSize, res.content);

    // removing windows new lines delimeters, assuming no two CR are next to each other
    for (int i = 0; i < fileSize; i++)
    {
        if (*(res.content + i) == '\r')
            RemoveCharAt(&res, i);
    }

    PlaceLineEnd(&res);
    return res;
}

i32 IndexAfter(StringBuffer *buffer, i32 after, char ch)
{
    for (int i = after + 1; i < buffer->size; i++)
    {
        if (*(buffer->content + i) == ch)
            return i;
    }
    return -1;
}

i32 IndexBefore(StringBuffer *buffer, i32 before, char ch)
{
    for (int i = before - 1; i >= 0; i--)
    {
        if (*(buffer->content + i) == ch)
            return i;
    }
    return -1;
}

u32 AreStringsEqual(char *s1, char *s2)
{

    while (*s1 == *s2 && (*s1 != '\0' && *s2 != '\0'))
    {
        s1++;
        s2++;
    }
    if (*s1 == '\0' || *s2 == '\0')
        return 1;

    return 0;
}

u32 FindLastLineIndex(char *start, i32 len)
{
    char *last = start + len - 2;
    while (*last != '\n' && last >= start)
        last--;

    if (*last == '\n')
        return last - start + 1;

    return last - start;
}
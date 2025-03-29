#pragma once

#include "types.c"
#include "util\string.c"
#include "util\arena.c"

typedef struct Text
{
    StringBuffer buffer;
    u32 globalPosition;

    // these are cached values, such that I don't need to find them on each frame
    u32 line;
    u32 lineOffset;
} Text;

void UpdateCursorOffset(Text *text)
{
    i32 line = 0;

    i32 lineStartedAt = 0;
    for (i32 i = 0; i < text->globalPosition; i++)
    {
        if (text->buffer.content[i] == '\n')
        {
            line++;
            lineStartedAt = i + 1;
        }
    }
    text->line = line;
    text->lineOffset = text->globalPosition - lineStartedAt;
}

void SetCursorPosition(Text *text, u32 pos)
{
    if (pos >= 0 && pos <= text->buffer.size)
    {
        text->globalPosition = pos;
        UpdateCursorOffset(text);
    }
}

i32 FindLineStart(Text *text, i32 pos)
{
    while (pos > 0 && text->buffer.content[pos - 1] != '\n')
        pos--;

    return pos;
}

i32 FindLineEnd(Text *text, i32 pos)
{
    while (pos < text->buffer.size && text->buffer.content[pos] != '\n')
        pos++;

    return pos;
}

void GoDown(Text *text)
{
    i32 next = FindLineEnd(text, text->globalPosition);
    i32 nextNextLine = FindLineEnd(text, next + 1);

    SetCursorPosition(text, MinI32(next + text->lineOffset + 1, nextNextLine));
}

void GoUp(Text *text)
{
    i32 prev = FindLineStart(text, text->globalPosition);
    i32 prevPrevLine = FindLineStart(text, prev - 1);

    i32 pos = prevPrevLine + text->lineOffset;

    SetCursorPosition(text, MinI32(pos, prev));
}

void RemoveCharFromLeft(Text *text)
{
    if (text->globalPosition > 0)
    {
        RemoveCharAt(&text->buffer, text->globalPosition - 1);
        SetCursorPosition(text, text->globalPosition - 1);
    }
}

void RemoveCharFromRight(Text *text)
{
    if (text->globalPosition < text->buffer.size)
        RemoveCharAt(&text->buffer, text->globalPosition);
}

void InsertCharAtCurrentPosition(Text *text, char ch)
{
    InsertCharAt(&text->buffer, text->globalPosition, ch);
    SetCursorPosition(text, text->globalPosition + 1);
}

void SwapLineDown(Text *text, Arena *tmpArena)
{
    i32 lineStart = FindLineStart(text, text->globalPosition);
    i32 lineEnd = FindLineEnd(text, lineStart);

    if (lineEnd == text->buffer.size)
        return; // Last line, nothing to swap

    i32 nextLineStart = lineEnd + 1;
    i32 nextLineEnd = FindLineEnd(text, nextLineStart);

    i32 lineLen = lineEnd - lineStart;
    i32 nextLineLen = nextLineEnd - nextLineStart;

    char *temp = ArenaPush(tmpArena, lineLen + 1);

    memcpy(temp, text->buffer.content + lineStart, lineLen);
    temp[lineLen] = '\0';

    memmove(text->buffer.content + lineStart, text->buffer.content + nextLineStart, nextLineLen);
    text->buffer.content[lineStart + nextLineLen] = '\n';
    memcpy(text->buffer.content + lineStart + nextLineLen + 1, temp, lineLen);

    SetCursorPosition(text, lineStart + nextLineLen + text->lineOffset + 1);

    ArenaPop(tmpArena, lineLen + 1);
}

void SwapLineUp(Text *text, Arena *tmpArena)
{
    i32 lineStart = FindLineStart(text, text->globalPosition);
    if (lineStart == 0)
        return; // First line, nothing to swap

    i32 prevLineEnd = lineStart - 1;
    i32 prevLineStart = FindLineStart(text, prevLineEnd);

    i32 prevLineLen = prevLineEnd - prevLineStart;
    i32 lineEnd = FindLineEnd(text, lineStart);
    i32 lineLen = lineEnd - lineStart;

    char *temp = ArenaPush(tmpArena, lineLen + 1);

    memcpy(temp, text->buffer.content + lineStart, lineLen);
    temp[lineLen] = '\0';

    memmove(text->buffer.content + prevLineStart + lineLen + 1, text->buffer.content + prevLineStart, prevLineLen);
    memcpy(text->buffer.content + prevLineStart, temp, lineLen);
    text->buffer.content[prevLineStart + lineLen] = '\n';

    SetCursorPosition(text, prevLineStart + text->lineOffset);

    ArenaPop(tmpArena, lineLen + 1);
}

i32 GetIdentationLevel(Text *text)
{
    i32 start = FindLineStart(text, text->globalPosition);
    i32 level = 0;
    while (text->buffer.content[start + level] == ' ')
        level++;

    return level;
}

void InserSameCharAtCurrentPositionSeveralTimes(Text *text, char ch, i32 times)
{
    while (times > 0)
    {
        InsertCharAtCurrentPosition(text, ch);
        times--;
    }
}

void InsertNewLineAbove(Text *text)
{
    i32 identation = GetIdentationLevel(text);
    i32 lineStart = FindLineStart(text, text->globalPosition);
    InsertCharAt(&text->buffer, lineStart, '\n');
    SetCursorPosition(text, lineStart);

    InserSameCharAtCurrentPositionSeveralTimes(text, ' ', identation);
}

void InsertNewLineBelow(Text *text)
{
    i32 lineEnd = FindLineEnd(text, text->globalPosition);
    i32 identation = GetIdentationLevel(text);

    i32 target = lineEnd;
    if (lineEnd != text->buffer.size)
        target++;

    InsertCharAt(&text->buffer, target, '\n');
    SetCursorPosition(text, lineEnd + 1);

    InserSameCharAtCurrentPositionSeveralTimes(text, ' ', identation);
}

void RemoveLine(Text *text)
{
    RemoveChars(&text->buffer, FindLineStart(text, text->globalPosition), FindLineEnd(text, text->globalPosition));
}

#define TAB_WIDTH 3
void MoveLineRight(Text *text)
{
    i32 lineStart = FindLineStart(text, text->globalPosition);
    for (i32 i = 0; i < TAB_WIDTH; i++)
        InsertCharAt(&text->buffer, lineStart, ' ');

    SetCursorPosition(text, text->globalPosition + TAB_WIDTH);
}

void MoveLineLeft(Text *text)
{
    i32 lineStart = FindLineStart(text, text->globalPosition);
    i32 charsRemoved = 0;
    while (charsRemoved < TAB_WIDTH && (text->buffer.content[lineStart] == ' '))
    {
        RemoveCharAt(&text->buffer, lineStart);
        charsRemoved++;
    }

    SetCursorPosition(text, MaxI32(text->globalPosition - charsRemoved, lineStart));
}
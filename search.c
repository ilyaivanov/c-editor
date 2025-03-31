#pragma once
#include "types.c"
#include "util\string.c"
#include "text.c"

typedef struct EntryFound
{
    u32 at;
    u32 len;
} EntryFound;

// TODO: use arena
EntryFound entriesAt[1024 * 10] = {0};
i32 currentEntry;
i32 entriesCount;

u8 searchTerm[255];
i32 searchLen;

void FindEntries(Text *text)
{
    i32 currentWordIndex = 0;
    entriesCount = 0;
    for (i32 i = 0; i < text->buffer.size; i++)
    {

        if (ToCharLower(text->buffer.content[i]) ==
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

void ClearEntries()
{
    entriesCount = 0;
    memset(&searchTerm, 0, ArrayLength(searchTerm));
}
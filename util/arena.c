#pragma once

#include "math.c"

typedef struct Arena
{
    u8 *start;
    u32 bytesAllocated;
    u32 size;
} Arena;

inline Arena CreateArena(u32 size)
{
    return (Arena){
        .size = size,
        .bytesAllocated = 0,
        .start = VirtualAllocateMemory(size)};
}

u8 *ArenaPush(Arena *arena, u32 size)
{
    u8 *res = arena->start + arena->bytesAllocated;
    arena->bytesAllocated += size;

    if (arena->bytesAllocated > arena->size)
        Fail("Exceeded arena size");

    return res;
}

void ArenaPop(Arena *arena, u32 size)
{
    arena->bytesAllocated -= size;

    if (arena->bytesAllocated > arena->size)
        Fail("Exceeded arena size");
}

void ArenaClear(Arena *arena)
{
    arena->bytesAllocated = 0;
}
#pragma once
#include "util\win32.c"

int worldWidth = 0;
int worldHeight = 0;
typedef enum
{
    Floor,
    Wall,
    Hole
} TileType;

TileType *tiles;

void InitTiles()
{

    FileContent map = ReadMyFileImp(".\\map.txt");

    int tileX = 0;
    int tileY = 0;

    int playerTileX = 0;
    int playerTileY = 0;
    for (i32 i = 0; i <= map.size; i++)
    {
        char ch = map.content[i];
        if (ch == '\r')
            continue;

        if (map.content[i] == '\n')
        {
            tileY++;
            if (tileX > worldWidth)
                worldWidth = tileX;
            tileX = 0;
        }
        else
        {
            tileX++;
        }
    }

    worldHeight = tileY + 1;

    tiles = VirtualAllocateMemory(worldWidth * worldHeight * sizeof(TileType));

    tileX = 0;
    tileY = 0;

    for (i32 i = 0; i <= map.size; i++)
    {
        char ch = map.content[i];
        if (ch == 'X')
            tiles[(worldHeight - 1 - tileY) * worldWidth + tileX] = Hole;

        if (ch == '\n')
        {
            tileY++;
            tileX = 0;
        }
        else
        {
            tileX++;
        }
    }

    VirtualFreeMemory(map.content);
}
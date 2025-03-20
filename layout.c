#include "common.c"
#include "types.c"

MyBitmap canvas;

void PaintRect(i32 x, i32 y, i32 width, i32 height, u32 color)
{
    i32 x0 = x < 0 ? 0 : x;
    i32 y0 = y < 0 ? 0 : y;
    i32 x1 = (x + width) > canvas.width ? canvas.width : (x + width);
    i32 y1 = (y + height) > canvas.height ? canvas.height : (y + height);

    for (i32 j = y0; j < y1; j++)
    {
        for (i32 i = x0; i < x1; i++)
        {
            canvas.pixels[j * canvas.width + i] = color;
        }
    }
}

inline void PaintAppRect(MyRect2 rect, u32 color)
{
    PaintRect(rect.x, rect.y, rect.width, rect.height, color);
}

inline MyRect2 ShrinkFromBottom(MyRect2 rect, i32 val)
{
    return (MyRect2){rect.x, rect.y, rect.width, rect.height - val};
}

inline MyRect2 AppendAfterBottom(MyRect2 rect, i32 height)
{
    return (MyRect2){rect.x, rect.y + rect.height, rect.width, height};
}

// inline void SplitVectically(AppRect rect, AppRect *left, AppRect *right)
// {
//     left->x = rect.x;
//     left->y = rect.y;

//     // return (AppRect){rect.x, rect.y + rect.height, rect.width, height};
// }

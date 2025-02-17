#pragma once
#include <stdint.h>
#include "sincos.c"

#define PI 3.14159265359f
#define ONE_OVER_SQUARE_ROOT_OF_TWO 0.70710678118f

typedef int8_t i8;

typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef struct V2i
{
    i32 x, y;
} V2i;
typedef struct V3i
{
    i32 x, y, z;
} V3i;
typedef struct V2f
{
    f32 x, y;
} V2f;
typedef struct V3f

{
    union
    {
        struct
        {
            f32 x, y, z;
        };
        struct
        {
            f32 r, g, b;
        };
    };
} V3f;

typedef struct V4f
{
    union
    {
        struct
        {
            f32 x, y, z, t;
        };
        struct
        {
            f32 r, g, b, a;
        };
    };
} V4f;

typedef struct V4u
{
    union
    {
        struct
        {
            u32 x, y, z, t;
        };
    };
} V4u;

V2f V2fAdd(V2f v1, V2f v2)
{
    return (V2f){v1.x + v2.x, v1.y + v2.y};
}
V2f V2fDiff(V2f v1, V2f v2)
{
    return (V2f){v1.x - v2.x, v1.y - v2.y};
}

V2f V2fSub(V2f v1, V2f v2)
{
    return (V2f){v1.x - v2.x, v1.y - v2.y};
}

V3f V3fSub(V3f v1, V3f v2)
{
    return (V3f){v1.x - v2.x, v1.y - v2.y, v1.z - v2.z};
}

V3f V3fNormalize(V3f v)
{
    float len = mysqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    return (V3f){v.x / len, v.y / len, v.z / len};
}

V2f V2fMulScalar(V2f v1, float scalar)
{
    return (V2f){v1.x * scalar, v1.y * scalar};
}

V3f V3fCross(V3f v1, V3f v2)
{
    V3f result;

    result.x = (v1.y * v2.z) - (v1.z * v2.y);
    result.y = (v1.z * v2.x) - (v1.x * v2.z);
    result.z = (v1.x * v2.y) - (v1.y * v2.x);

    return result;
}

float V3fDot(V3f v1, V3f v2)
{
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

typedef struct Mat4
{
    float values[16];
} Mat4;

// I've ChatGPTed this function like a dummy
inline Mat4 Mult(Mat4 m1, Mat4 m2)
{
    Mat4 result;

    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            result.values[i * 4 + j] = 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                result.values[i * 4 + j] += m1.values[i * 4 + k] * m2.values[k * 4 + j];
            }
        }
    }

    return result;
}

// taken from https://registry.khronos.org/OpenGL-Refpages/gl2.1/xhtml/gluPerspective.xml algo
inline Mat4 CreatePerspective(float fovyDeg, V2i screen, float zNear, float zFar)
{
    float aspect = ((float)screen.x / (float)screen.y);
    float f = 1.0f / (tanfdeg(fovyDeg / 2));

    // clang-format off
    return (Mat4){
        f / aspect,      0,        0,        0,
        0,               f,        0,        0,
        0,        0,        (zFar + zNear) / (zNear - zFar),        (2 * zFar * zNear) / (zNear - zFar),
        0,        0,        -1,        0,
    };
    // clang-format on
}

inline Mat4 CreateScreenProjection(V2i screen)
{
    // allows me to set vecrtex coords as 0..width/height, instead of -1..+1
    // 0,0 is bottom left, not top left
    // matrix in code != matrix in math notation, details at https://youtu.be/kBuaCqaCYwE?t=3084
    // in short: rows in math are columns in code

    float w = 2.0f / (f32)screen.x;
    float h = 2.0f / (f32)screen.y;
    // clang-format off
    return (Mat4){
        w,   0,   0,   -1,
        0,   h,   0,   -1,
        0,   0,   1,    0,
        0,   0,   0,    1,
    };
    // clang-format on
}

inline Mat4 RotateAroundZ(Mat4 base, float rads)
{
    // float rads = degrees * PI / 180.0f;

    // clang-format off
    Mat4 rotat = {
        cosf(rads), -sinf(rads),        0,        0,
        sinf(rads),  cosf(rads),        0,        0,
        0,                    0,        1,        0,
        0,                    0,        0,        1,
    };
    // clang-format on

    return Mult(base, rotat);
}

inline Mat4 Mat4Identity()
{
    // clang-format off
    return (Mat4){
        1,   0,   0,   0,
        0,   1,   0,   0,
        0,   0,   1,   0,
        0,   0,   0,   1,
    };
    // clang-format on
}

inline Mat4 Mat4ScaleV3f(Mat4 mat, V3f v)
{
    mat.values[0 + 0 * 4] *= v.x;
    mat.values[1 + 1 * 4] *= v.y;
    mat.values[2 + 2 * 4] *= v.z;
    return mat;
}

inline Mat4 Mat4TranslateV3f(Mat4 mat, V3f v)
{
    mat.values[3 + 0 * 4] += v.x;
    mat.values[3 + 1 * 4] += v.y;
    mat.values[3 + 2 * 4] += v.z;
    return mat;
}

V3f up = {0, 1.0f, 0};

// taken from \glm\ext\matrix_transform.inl:99 (lookAtRH)
// https://github.com/g-truc/glm/blob/master/glm/ext/matrix_transform.inl#L153

// NO NEED TO TRANSPOSE MATRIX WHEN PASSING TO OPENGL
inline Mat4 LookAt(V3f eye, V3f center)
{
    V3f f = V3fNormalize(V3fSub(center, eye));
    V3f s = V3fNormalize(V3fCross(f, up));
    V3f u = V3fCross(s, f);

    // NEED TO TRANSPOSE this
    Mat4 res = {
        s.x, u.x, -f.x, 0,
        s.y, u.y, -f.y, 0,
        s.z, u.z, -f.z, 0,
        -V3fDot(s, eye), -V3fDot(u, eye), V3fDot(f, eye), 1};

    return res;
}

//
//
//
//
//
//

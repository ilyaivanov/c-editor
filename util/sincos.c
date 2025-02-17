#pragma once

#include <xmmintrin.h>

#pragma function(memset)
void *memset(void *dest, int c, size_t count)
{
    char *bytes = (char *)dest;
    while (count--)
    {
        *bytes++ = (char)c;
    }
    return dest;
}

// sin cos taken from https://guide.handmadehero.org/code/day440
#define R32_EPSILON 1.19209290e-7f

float XSinCosX[][3] = {
    {-3.00001740e+00f, -1.41102776e-01f, -9.89994943e-01f},
    {-2.49998403e+00f, -5.98484933e-01f, -8.01134050e-01f},
    {-1.99999201e+00f, -9.09300745e-01f, -4.16139573e-01f},
    {-1.50000191e+00f, -9.97495115e-01f, 7.07352981e-02f},
    {-1.00000119e+00f, -8.41471612e-01f, 5.40301323e-01f},
    {-5.00000119e-01f, -4.79425639e-01f, 8.77582490e-01f},
    {0.00000000e+00f, 0.00000000e+00f, 1.00000000e+00f},
    {5.00002027e-01f, 4.79427308e-01f, 8.77581596e-01f},
    {1.00000119e+00f, 8.41471612e-01f, 5.40301323e-01f},
    {1.49999249e+00f, 9.97494459e-01f, 7.07446933e-02f},
    {1.99999309e+00f, 9.09300327e-01f, -4.16140556e-01f},
    {2.49999976e+00f, 5.98472357e-01f, -8.01143467e-01f},
    {3.00001740e+00f, 1.41102776e-01f, -9.89994943e-01f}};

void SinCos(float x, float *sinX, float *cosX)
{
    const float RoundToInteger = 1.5f / R32_EPSILON;

    float OneOverTwoPi = 0.15915494309189535f;
    float K1 = 6.28125f;
    float K2 = 1.9352435546875e-3f;
    float K3 = 6.3624898976925e-8f;

    float n = (x * OneOverTwoPi + RoundToInteger) - RoundToInteger;
    float XPrime = ((x - K1 * n) - K2 * n);

    float xi = (XPrime * 2.0f + RoundToInteger) - RoundToInteger;

    int i = (int)xi + 6;

    float xd = XPrime - XSinCosX[i][0] - K3 * n;
    float sin_i = XSinCosX[i][1];
    float cos_i = XSinCosX[i][2];
    float xd2 = xd * xd;

    float C0 = 1.0f;
    float C2 = -0.499999999337977967373255617f;
    float C4 = 0.041666581944080381709011165f;
    float C6 = -0.001386177814655217640748956f;

    float S1 = 0.999999999999669874100913195f;
    float S3 = -0.166666666498050625629552137f;
    float S5 = 0.008333319863964268480960374f;
    float S7 = -0.000198068192813108213205250f;

    float CosIXd = cos_i * xd;
    float SinIXd = sin_i * xd;

    float CosXdC2C6 = C2 + xd2 * (C4 + xd2 * C6);
    float SinXdS3S7 = S3 + xd2 * (S5 + xd2 * S7);

    *sinX = xd2 * (CosIXd * SinXdS3S7 + sin_i * CosXdC2C6) + sin_i * C0 + CosIXd * S1;
    *cosX = xd2 * (cos_i * CosXdC2C6 - SinIXd * SinXdS3S7) - SinIXd * S1 + cos_i * C0;
}

float sinf(float x)
{
    float cos = 0.0f;
    float sin = 0.0f;
    SinCos(x, &sin, &cos);
    return sin;
}

float cosf(float x)
{
    float cos = 0.0f;
    float sin = 0.0f;
    SinCos(x, &sin, &cos);
    return cos;
}

// float mytanf(float x)
// {
//     float cos = 0.0f;
//     float sin = 0.0f;
//     SinCos(x, &sin, &cos);
//     return sin / cos;
// }

inline float mysqrtf(float x)
{
    float result;
    _mm_store_ss(&result, _mm_sqrt_ss(_mm_set_ss(x)));

    return result;
}
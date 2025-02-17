// https://github.com/libsdl-org/SDL/blob/e2a5c1d203ba2ab3917075bcd31e4b81b3a85676/src/stdlib/SDL_stdlib.c

/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

#pragma once

#ifndef __HAIKU__ /* already defined in a system header. */
typedef unsigned int u_int32_t;
#endif

/* The original fdlibm code used statements like:
    n0 = ((*(int*)&one)>>29)^1;		* index of high word *
    ix0 = *(n0+(int*)&x);			* high word of x *
    ix1 = *((1-n0)+(int*)&x);		* low word of x *
   to dig two 32 bit words out of the 64 bit IEEE floating point
   value.  That is non-ANSI, and, moreover, the gcc instruction
   scheduler gets it wrong.  We instead use the following macros.
   Unlike the original code, we determine the endianness at compile
   time, not at run time; I don't see much benefit to selecting
   endianness at run time.  */

/* A union which permits us to convert between a double and two 32 bit
   ints.  */

/*
 * Math on arm is special:
 * For FPA, float words are always big-endian.
 * For VFP, floats words follow the memory system mode.
 */

typedef union
{
    double value;
    struct
    {
        u_int32_t lsw;
        u_int32_t msw;
    } parts;
} ieee_double_shape_type;

#define GET_HIGH_WORD(i, d)          \
    do                               \
    {                                \
        ieee_double_shape_type gh_u; \
        gh_u.value = (d);            \
        (i) = gh_u.parts.msw;        \
    } while (0)

#define GET_LOW_WORD(i, d)           \
    do                               \
    {                                \
        ieee_double_shape_type gl_u; \
        gl_u.value = (d);            \
        (i) = gl_u.parts.lsw;        \
    } while (0)

#define SET_HIGH_WORD(d, v)          \
    do                               \
    {                                \
        ieee_double_shape_type sh_u; \
        sh_u.value = (d);            \
        sh_u.parts.msw = (v);        \
        (d) = sh_u.value;            \
    } while (0)

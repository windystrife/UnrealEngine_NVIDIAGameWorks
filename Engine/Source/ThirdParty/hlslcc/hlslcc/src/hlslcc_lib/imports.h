// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// This code is modified from that in the Mesa3D Graphics library available at
// http://mesa3d.org/
// The license for the original code follows:

/*
 * Mesa 3-D graphics library
 * Version:  7.5
 *
 * Copyright (C) 1999-2008  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/** mesa/main/imports.h */

/**
 * \file imports.h
 * Standard C library function wrappers.
 *
 * This file provides wrappers for all the standard C library functions
 * like malloc(), free(), printf(), getenv(), etc.
 */


#ifndef IMPORTS_H
#define IMPORTS_H

#include "compiler.h"

/**********************************************************************/
/** Memory macros */
/*@{*/

/** Allocate \p BYTES bytes */
#define MALLOC(BYTES)      malloc(BYTES)
/** Allocate and zero \p BYTES bytes */
#define CALLOC(BYTES)      calloc(1, BYTES)
/** Allocate a structure of type \p T */
#define MALLOC_STRUCT(T)   (struct T *) malloc(sizeof(struct T))
/** Allocate and zero a structure of type \p T */
#define CALLOC_STRUCT(T)   (struct T *) calloc(1, sizeof(struct T))
/** Free memory */
#define FREE(PTR)          free(PTR)

/*@}*/


/*
 * For GL_ARB_vertex_buffer_object we need to treat vertex array pointers
 * as offsets into buffer stores.  Since the vertex array pointer and
 * buffer store pointer are both pointers and we need to add them, we use
 * this macro.
 * Both pointers/offsets are expressed in bytes.
 */
#define ADD_POINTERS(A, B)  ( (GLubyte *) (A) + (uintptr_t) (B) )


/**
 * Sometimes we treat GLfloats as GLints.  On x86 systems, moving a float
 * as a int (thereby using integer registers instead of FP registers) is
 * a performance win.  Typically, this can be done with ordinary casts.
 * But with gcc's -fstrict-aliasing flag (which defaults to on in gcc 3.0)
 * these casts generate warnings.
 * The following union typedef is used to solve that.
 */
typedef union { float f; int32_t i; } fi_type;



/**********************************************************************
 * Math macros
 */

#define MAX_GLUSHORT	0xffff
#define MAX_GLUINT	0xffffffff

/* Degrees to radians conversion: */
#define DEG2RAD (M_PI/180.0)


/***
 *** SQRTF: single-precision square root
 ***/
#if 0 /* _mesa_sqrtf() not accurate enough - temporarily disabled */
#  define SQRTF(X)  _mesa_sqrtf(X)
#else
#  define SQRTF(X)  (float) sqrt((float) (X))
#endif


/***
 *** INV_SQRTF: single-precision inverse square root
 ***/
#if 0
#define INV_SQRTF(X) _mesa_inv_sqrt(X)
#else
#define INV_SQRTF(X) (1.0F / SQRTF(X))  /* this is faster on a P4 */
#endif


/**
 * \name Work-arounds for platforms that lack C99 math functions
 */
/*@{*/
#include <math.h>

#if defined(_MSC_VER)
static inline float truncf(float x) { return x < 0.0f ? ceilf(x) : floorf(x); }
static inline float exp2f(float x) { return powf(2.0f, x); }
static inline float log2f(float x) { return logf(x) * 1.442695041f; }
static inline float asinhf(float x) { return logf(x + sqrtf(x * x + 1.0f)); }
static inline float acoshf(float x) { return logf(x + sqrtf(x * x - 1.0f)); }
static inline float atanhf(float x) { return (logf(1.0f + x) - logf(1.0f - x)) / 2.0f; }
static inline int isblank(int ch) { return ch == ' ' || ch == '\t'; }
#define strtoll(p, e, b) _strtoi64(p, e, b)
#endif
/*@}*/

/***
 *** LOG2: Log base 2 of float
 ***/
#ifdef USE_IEEE

/* Pretty fast, and accurate.
 * Based on code from http://www.flipcode.com/totd/
 */
static inline float LOG2(float val)
{
   fi_type num;
   int32_t log_2;
   num.f = val;
   log_2 = ((num.i >> 23) & 255) - 128;
   num.i &= ~(255 << 23);
   num.i += 127 << 23;
   num.f = ((-1.0f/3) * num.f + 2) * num.f - 2.0f/3;
   return num.f + log_2;
}
#else
/*
 * NOTE: log_base_2(x) = log(x) / log(2)
 * NOTE: 1.442695 = 1/log(2).
 */
#define LOG2(x)  ((float) (log(x) * 1.442695F))
#endif


/***
 *** IS_INF_OR_NAN: test if float is infinite or NaN
 ***/
#ifdef USE_IEEE
static inline int IS_INF_OR_NAN( float x )
{
   fi_type tmp;
   tmp.f = x;
   return !(int)((unsigned int)((tmp.i & 0x7fffffff)-0x7f800000) >> 31);
}
#elif defined(isfinite)
#define IS_INF_OR_NAN(x)        (!isfinite(x))
#elif defined(finite)
#define IS_INF_OR_NAN(x)        (!finite(x))
#elif defined(__VMS)
#define IS_INF_OR_NAN(x)        (!finite(x))
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define IS_INF_OR_NAN(x)        (!isfinite(x))
#else
#define IS_INF_OR_NAN(x)        (!finite(x))
#endif


/***
 *** IS_NEGATIVE: test if float is negative
 ***/
#if defined(USE_IEEE)
static inline int GET_FLOAT_BITS( float x )
{
   fi_type fi;
   fi.f = x;
   return fi.i;
}
#define IS_NEGATIVE(x) (GET_FLOAT_BITS(x) < 0)
#else
#define IS_NEGATIVE(x) (x < 0.0F)
#endif


/***
 *** DIFFERENT_SIGNS: test if two floats have opposite signs
 ***/
#if defined(USE_IEEE)
#define DIFFERENT_SIGNS(x,y) ((GET_FLOAT_BITS(x) ^ GET_FLOAT_BITS(y)) & (1<<31))
#else
/* Could just use (x*y<0) except for the flatshading requirements.
 * Maybe there's a better way?
 */
#define DIFFERENT_SIGNS(x,y) ((x) * (y) <= 0.0F && (x) - (y) != 0.0F)
#endif


/***
 *** CEILF: ceiling of float
 *** FLOORF: floor of float
 *** FABSF: absolute value of float
 *** LOGF: the natural logarithm (base e) of the value
 *** EXPF: raise e to the value
 *** LDEXPF: multiply value by an integral power of two
 *** FREXPF: extract mantissa and exponent from value
 ***/
#if defined(__gnu_linux__)
/* C99 functions */
#define CEILF(x)   ceilf(x)
#define FLOORF(x)  floorf(x)
#define FABSF(x)   fabsf(x)
#define LOGF(x)    logf(x)
#define EXPF(x)    expf(x)
#define LDEXPF(x,y)  ldexpf(x,y)
#define FREXPF(x,y)  frexpf(x,y)
#else
#define CEILF(x)   ((GLfloat) ceil(x))
#define FLOORF(x)  ((GLfloat) floor(x))
#define FABSF(x)   ((GLfloat) fabs(x))
#define LOGF(x)    ((GLfloat) log(x))
#define EXPF(x)    ((GLfloat) exp(x))
#define LDEXPF(x,y)  ((GLfloat) ldexp(x,y))
#define FREXPF(x,y)  ((GLfloat) frexp(x,y))
#endif


/***
 *** IROUND: return (as an integer) float rounded to nearest integer
 ***/
#if defined(USE_X86_ASM) && defined(__GNUC__) && defined(__i386__)
static inline int iround(float f)
{
   int r;
   __asm__ ("fistpl %0" : "=m" (r) : "t" (f) : "st");
   return r;
}
#define IROUND(x)  iround(x)
#elif defined(USE_X86_ASM) && defined(_MSC_VER)
static inline int iround(float f)
{
   int r;
   _asm {
	 fld f
	 fistp r
	}
   return r;
}
#define IROUND(x)  iround(x)
#elif defined(__WATCOMC__) && defined(__386__)
long iround(float f);
#pragma aux iround =                    \
	"push   eax"                        \
	"fistp  dword ptr [esp]"            \
	"pop    eax"                        \
	parm [8087]                         \
	value [eax]                         \
	modify exact [eax];
#define IROUND(x)  iround(x)
#else
#define IROUND(f)  ((int) (((f) >= 0.0F) ? ((f) + 0.5F) : ((f) - 0.5F)))
#endif

#define IROUND64(f)  ((GLint64) (((f) >= 0.0F) ? ((f) + 0.5F) : ((f) - 0.5F)))

/***
 *** IROUND_POS: return (as an integer) positive float rounded to nearest int
 ***/
#ifdef DEBUG
#define IROUND_POS(f) (check((f) >= 0.0F), IROUND(f))
#else
#define IROUND_POS(f) (IROUND(f))
#endif


/***
 *** IFLOOR: return (as an integer) floor of float
 ***/
#if defined(USE_X86_ASM) && defined(__GNUC__) && defined(__i386__)
/*
 * IEEE floor for computers that round to nearest or even.
 * 'f' must be between -4194304 and 4194303.
 * This floor operation is done by "(iround(f + .5) + iround(f - .5)) >> 1",
 * but uses some IEEE specific tricks for better speed.
 * Contributed by Josh Vanderhoof
 */
static inline int ifloor(float f)
{
   int ai, bi;
   double af, bf;
   af = (3 << 22) + 0.5 + (double)f;
   bf = (3 << 22) + 0.5 - (double)f;
   /* GCC generates an extra fstp/fld without this. */
   __asm__ ("fstps %0" : "=m" (ai) : "t" (af) : "st");
   __asm__ ("fstps %0" : "=m" (bi) : "t" (bf) : "st");
   return (ai - bi) >> 1;
}
#define IFLOOR(x)  ifloor(x)
#elif defined(USE_IEEE)
static inline int ifloor(float f)
{
   int ai, bi;
   double af, bf;
   fi_type u;

   af = (3 << 22) + 0.5 + (double)f;
   bf = (3 << 22) + 0.5 - (double)f;
   u.f = (float) af;  ai = u.i;
   u.f = (float) bf;  bi = u.i;
   return (ai - bi) >> 1;
}
#define IFLOOR(x)  ifloor(x)
#else
static inline int ifloor(float f)
{
   int i = IROUND(f);
   return (i > f) ? i - 1 : i;
}
#define IFLOOR(x)  ifloor(x)
#endif


/***
 *** ICEIL: return (as an integer) ceiling of float
 ***/
#if defined(USE_X86_ASM) && defined(__GNUC__) && defined(__i386__)
/*
 * IEEE ceil for computers that round to nearest or even.
 * 'f' must be between -4194304 and 4194303.
 * This ceil operation is done by "(iround(f + .5) + iround(f - .5) + 1) >> 1",
 * but uses some IEEE specific tricks for better speed.
 * Contributed by Josh Vanderhoof
 */
static inline int iceil(float f)
{
   int ai, bi;
   double af, bf;
   af = (3 << 22) + 0.5 + (double)f;
   bf = (3 << 22) + 0.5 - (double)f;
   /* GCC generates an extra fstp/fld without this. */
   __asm__ ("fstps %0" : "=m" (ai) : "t" (af) : "st");
   __asm__ ("fstps %0" : "=m" (bi) : "t" (bf) : "st");
   return (ai - bi + 1) >> 1;
}
#define ICEIL(x)  iceil(x)
#elif defined(USE_IEEE)
static inline int iceil(float f)
{
   int ai, bi;
   double af, bf;
   fi_type u;
   af = (3 << 22) + 0.5 + (double)f;
   bf = (3 << 22) + 0.5 - (double)f;
   u.f = (float) af; ai = u.i;
   u.f = (float) bf; bi = u.i;
   return (ai - bi + 1) >> 1;
}
#define ICEIL(x)  iceil(x)
#else
static inline int iceil(float f)
{
   int i = IROUND(f);
   return (i < f) ? i + 1 : i;
}
#define ICEIL(x)  iceil(x)
#endif


/**********************************************************************
 * Functions
 */

extern void *
_mesa_align_malloc( size_t bytes, unsigned long alignment );

extern void *
_mesa_align_calloc( size_t bytes, unsigned long alignment );

extern void
_mesa_align_free( void *ptr );

extern void *
_mesa_align_realloc(void *oldBuffer, size_t oldSize, size_t newSize,
                    unsigned long alignment);

extern void *
_mesa_realloc( void *oldBuffer, size_t oldSize, size_t newSize );

extern void
_mesa_memset16( unsigned short *dst, unsigned short val, size_t n );

extern double
_mesa_sqrtd(double x);

extern float
_mesa_sqrtf(float x);

extern float
_mesa_inv_sqrtf(float x);

extern void
_mesa_init_sqrt_table(void);


#if defined(__GNUC__) && ((__GNUC__ * 100 + __GNUC_MINOR__) >= 304) /* gcc 3.4 or later */
#define _mesa_bitcount(i) __builtin_popcount(i)
#define _mesa_bitcount_64(i) __builtin_popcountll(i)
#else
extern unsigned int
_mesa_bitcount(unsigned int n);
extern unsigned int
_mesa_bitcount_64(uint64_t n);
#endif

extern void *
_mesa_bsearch( const void *key, const void *base, size_t nmemb, size_t size, 
               int (*compar)(const void *, const void *) );

extern char *
_mesa_getenv( const char *var );

extern char *
_mesa_strdup( const char *s );

extern float
_mesa_strtof( const char *s, char **end );

extern unsigned int
_mesa_str_checksum(const char *str);

extern int
_mesa_snprintf( char *str, size_t size, const char *fmt, ... );

extern int
_mesa_vsnprintf(char *str, size_t size, const char *fmt, va_list arg);


#if defined(_MSC_VER) && !defined(snprintf)
#define snprintf _snprintf
#endif

#endif /* IMPORTS_H */

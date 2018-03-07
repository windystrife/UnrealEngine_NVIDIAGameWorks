// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// This code is modified from that in the Mesa3D Graphics library available at
// http://mesa3d.org/
// The license for the original code follows:

/*
 * Mesa 3-D graphics library
 * Version:  7.5
 *
 * Copyright (C) 1999-2008  Brian Paul   All Rights Reserved.
 * Copyright (C) 2009  VMware, Inc.  All Rights Reserved.
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

/** mesa/main/compiler.h */

/**
 * \file compiler.h
 * Compiler-related stuff.
 */


#ifndef COMPILER_H
#define COMPILER_H


//@todo-rco: Remove STL!
#include <ctype.h>
#include <stdarg.h>


/**
 * Get standard integer types
 */
#include <stdint.h>

/**
 * Functions that have different names in Visual Studio
 */
#ifndef _MSC_VER
#define strnicmp strncasecmp
#define stricmp strcasecmp
#define _strdup strdup
#endif

/**
 * Function inlining
 */
#ifndef inline
#  ifdef __cplusplus
     /* C++ supports inline keyword */
#  elif defined(__GNUC__)
#    define inline __inline__
#  elif defined(_MSC_VER)
#    define inline __inline
#  elif defined(__ICL)
#    define inline __inline
#  elif defined(__INTEL_COMPILER)
     /* Intel compiler supports inline keyword */
#  elif defined(__WATCOMC__) && (__WATCOMC__ >= 1100)
#    define inline __inline
#  elif defined(__SUNPRO_C) && defined(__C99FEATURES__)
     /* C99 supports inline keyword */
#  elif (__STDC_VERSION__ >= 199901L)
     /* C99 supports inline keyword */
#  else
#    define inline
#  endif
#endif
#ifndef INLINE
#  define INLINE inline
#endif

#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

#ifndef M_LOG2E
#define M_LOG2E     (1.4426950408889634074)
#endif

/**
 * USE_IEEE: Determine if we're using IEEE floating point
 */
#if defined(__i386__) || defined(__386__) || defined(__sparc__) || \
    defined(__s390x__) || defined(__powerpc__) || \
    defined(__x86_64__) || \
    defined(ia64) || defined(__ia64__) || \
    defined(__hppa__) || defined(hpux) || \
    defined(__mips) || defined(_MIPS_ARCH) || \
    defined(__arm__) || \
    defined(__sh__) || defined(__m32r__) || \
    (defined(__sun) && defined(_IEEE_754)) || \
    (defined(__alpha__) && (defined(__IEEE_FLOAT) || !defined(VMS)))
#define USE_IEEE
#define IEEE_ONE 0x3f800000
#endif

#ifndef Elements
#define Elements(x) (sizeof(x)/sizeof(*(x)))
#endif

#endif /* COMPILER_H */

/* e_17.c:  Ill-formed group in a source file.  */

#define MACRO_1     1

/* 17.1:    Error of #endif without #if.    */
#endif

/* 17.2:    Error of #else without #if. */
#else

/* 17.3:    Error of #else after #else. */
#if     MACRO_1
#else
#else
#endif

/* 17.4:    Error of #elif after #else. */
#if     MACRO_1 == 1
#else
#elif   MACRO_1 == 0
#endif

/* 17.5:    Error of #endif without #if in an included file.    */
#if     1
#include    "unbal1.h"

/* 17.6:    Error of unterminated #if section in an included file.  */
#include    "unbal2.h"
#endif

/* 17.7:    Error of unterminated #if section.  */
#if     MACRO_1 == 0
#else


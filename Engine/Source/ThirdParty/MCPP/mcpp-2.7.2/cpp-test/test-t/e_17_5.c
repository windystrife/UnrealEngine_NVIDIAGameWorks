/* e_17_5.c:    Imbalance of #if - #endif in included files.    */

/* { dg-do preprocess } */

/* 17.5:    Error of #endif without #if in an included file.    */
#if     1
#include    "unbal1.h"
/* { dg-error "e_17_5.c:7:\n\[\^ \]*( error:|) (#endif without #if|unbalanced `#endif')| Not in a #if \\(#ifdef\\) section in a source file" "" { target *-*-* } 0 } */

/* 17.6:    Error of unterminated #if section in an included file.  */
#include    "unbal2.h"
/* { dg-error "e_17_5.c:11:\n\[\^ \]*( error:|) unterminated (#else|`#else')| End of file within #if \\(#ifdef\\) section" "" { target *-*-* } 0 } */
#endif


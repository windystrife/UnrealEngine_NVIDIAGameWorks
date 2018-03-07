/* e_14_10.c:   Overflow of constant expression in #if directive.    */

/* { dg-do preprocess }
   { dg-options "-ansi -pedantic-errors" }
   { dg-excess-errors "" }
 */

/* 14.10:   */
/* In C99, #if expression is evaluated in intmax_t  */
#if __STDC_VERSION__ < 199901L
#include    <limits.h>

#if     LONG_MAX - LONG_MIN     /* { dg-error "integer overflow in preprocessor expression| Result of \"-\" is out of range" } */
#endif
#if     LONG_MAX + 1 > SHRT_MAX     /* { dg-error "integer overflow in preprocessor expression| Result of \"\\+\" is out of range" } */
#endif
#if     LONG_MIN - 1    /* { dg-error "integer overflow in preprocessor expression| Result of \"-\" is out of range" } */
#endif
#if     LONG_MAX * 2    /* { dg-error "integer overflow in preprocessor expression| Result of \"\\*\" is out of range" } */
#endif
#endif


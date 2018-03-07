/* e_intmax.c:  Overflow of constant expression in #if directive.    */

/* { dg-do preprocess } */
/* { dg-options "-std=c99 -pedantic-errors -w" } */

#define	INTMAX_MAX	0x7FFFFFFFFFFFFFFF
#define INTMAX_MIN	(-INTMAX_MAX-1)
#define SHRT_MAX    0x7FFF

#if     INTMAX_MAX - INTMAX_MIN /* { dg-error "integer overflow in preprocessor expression| Result of \"-\" is out of range" } */
#endif
#if     INTMAX_MAX + 1 > SHRT_MAX   /* { dg-error "integer overflow in preprocessor expression| Result of \"\\+\" is out of range" } */
#endif
#if     INTMAX_MIN - 1  /* { dg-error "integer overflow in preprocessor expression| Result of \"-\" is out of range" } */
#endif
#if     INTMAX_MAX * 2  /* { dg-error "integer overflow in preprocessor expression| Result of \"\\*\" is out of range" } */
#endif


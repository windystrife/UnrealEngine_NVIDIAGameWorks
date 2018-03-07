/* e_16.c:  Trailing junk of #else, #endif. */

/* { dg-do preprocess } */

/* 16.1:    Trailing junk of #else. */
#define MACRO_0     0
#if     MACRO_0
#else   MACRO_0     /* { dg-error "extra tokens|text following| Excessive token sequence" } */

/* 16.2:    Trailing junk of #endif.    */
#endif  MACRO_0     /* { dg-error "extra tokens|text following| Excessive token sequence" } */


/* e_31_3.c:    Macro call in control line should complete in the line. */

/* { dg-do preprocess } */

#define glue( a, b)     a ## b
#define str( s)         # s
#define xstr( s)        str( s)

/* 31.3:    Unterminated macro call.    */
#include    xstr( glue( header,
    .h))
/* [ dg-error "unterminated argument list| (U|u)nterminated macro call" "" { target *-*-* } 10 } */
/* { dg-excess-errors "" } */


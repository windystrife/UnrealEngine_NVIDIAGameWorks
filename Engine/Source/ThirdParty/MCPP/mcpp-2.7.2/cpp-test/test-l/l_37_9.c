/* l_37_9.c:    Translation limits larger than C99 / 9. */

/* 37.9L:   Number of macro definitions.    */

#include    "m8191.h"

#ifdef  X0FFF
/*  0x0fff; */
    GBM;
#else
/*  0x1fff; */
    MDA;
#endif

/* { dg-do preprocess }
   { dg-options "-std=c99 -pedantic" }
   { dg-warning "| More than 4095 macros defined" "translation limit" { target *-*-* } 0 }
   { dg-final { if ![file exist l_37_9_.i] { return }        } }
   { dg-final { if \{ [grep l_37_9_.i "0x1fff"] != "" \} \{   } }
   { dg-final { return \}                          } }
   { dg-final { fail "l_37_9_.c: defining more than 4095 macros" } }
 */


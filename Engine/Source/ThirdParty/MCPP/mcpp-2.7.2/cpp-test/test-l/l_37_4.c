/* l_37_4.c:    Translation limits larger than C99 / 4. */

/*  37.4L:  Nested conditional inclusion.   */
/*
 *  Define one of the macros X7F or XFF, or 255 nesting of #ifdef will be
 * tested.
 */
#include    "ifdef127.h"

/* { dg-do preprocess }
   { dg-options "-std=c99 -pedantic" }
   { dg-warning "| More than 63 nesting of #if" "translation limit" { target *-*-* } 0 }
   { dg-final { if ![file exist l_37_4_.i] { return }                   } }
   { dg-final { if \{ [grep l_37_4_.i "ifdef_nest = 0xff"] != "" \} \{  } }
   { dg-final { return \}                                               } }
   { dg-final { fail "l_37_4_.c: more than 63 nesting of #if"           } }
 */


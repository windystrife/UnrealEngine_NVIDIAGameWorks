/* l_37_5.c:    Translation limits larger than C99 / 5. */

/*  37.5L:  Nested source file inclusion.   */
/*  Define one of the macros X1F, X3F or it will test 127 levels of
        #include    */
#include    "nest1.h"

/* { dg-do preprocess }
   { dg-options "-std=c99 -pedantic" }
   { dg-warning "| More than 15 nesting of #include" "translation limit" { target *-*-* } 0 }
   { dg-final { if ![file exist l_37_5_.i] { return }               } }
   { dg-final { if \{ [grep l_37_5_.i "nest = 0x7f"] != "" \} \{    } }
   { dg-final { return \}                                           } }
   { dg-final { fail "l_37_5_.c: more than 15 nesting of #include"  } }
 */


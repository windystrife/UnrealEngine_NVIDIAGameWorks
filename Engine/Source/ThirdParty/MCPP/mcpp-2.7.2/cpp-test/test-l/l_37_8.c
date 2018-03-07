/* l_37_8.c:    Translation limits larger than C99 / 8. */

/* 37.8L:   Length of logical source line.  */
/*  Define one of the macros X03FF, X07FF, X0FFF, X1FFF or it will test
        the line of 0x3fff bytes long.  */
#include    "longline.t"

/* { dg-do preprocess }
   { dg-options "-std=c99 -pedantic" }
   { dg-warning "| Logical source line longer than 4095 bytes" "translation limit" { target *-*-* } 673 }
   { dg-warning "| Logical source line longer than 4095 bytes" "translation limit" { target *-*-* } 1532 }
   { dg-final { if ![file exist l_37_8_.i] { return }        } }
   { dg-final { if \{ [grep l_37_8_.i "pushback"] != "" \} \{   } }
   { dg-final { if \{ [grep l_37_8_.i "s_name"] != "" \} \{   } }
   { dg-final { return \} \}                          } }
   { dg-final { fail "l_37_8_.c: logical source line longer than 4095 bytes" } }
 */


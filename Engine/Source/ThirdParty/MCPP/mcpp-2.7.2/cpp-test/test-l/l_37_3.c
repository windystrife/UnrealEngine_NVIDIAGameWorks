/* l_37_3.c:    Translation limits larger than C99 / 3. */

/* 37.3L:   Significant initial characters in an internal identifier or a
        macro name. */

/*  Name of 127 bytes long. */
    int
A123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
B123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde = 127;
#ifndef X7F
/*  Name of 255 bytes long. */
    int
A123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
B123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
C123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
D123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde = 255;
#endif

/* { dg-do preprocess }
   { dg-options "-std=c99 -pedantic" }
   { dg-warning "| Identifier longer than 63 bytes" "translation limit" { target *-*-* } 9  }
   { dg-warning "| Identifier longer than 63 bytes" "translation limit" { target *-*-* } 16 }
   { dg-final { if ![file exist l_37_3.i] { return }                } }
   { dg-final { if \{ [grep l_37_3.i "A123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdefB123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdefC123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdefD123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde"] != "" \} \{   } }
   { dg-final { return \}                                           } }
   { dg-final { fail "l_37_3.c: identifier longer than 63 bytes"    } }
 */


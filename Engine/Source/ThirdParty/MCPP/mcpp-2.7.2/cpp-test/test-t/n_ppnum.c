/* n_ppnum.c:   Preprocessing number token *p+* */
/* Undefined on C90, because '+A' is not a valid pp-token.  */

#define A   3
#define glue( a, b) a ## b
/*  12p+A;  */
    glue( 12p+, A);

/* { dg-do preprocess }
   { dg-options "-std=c99" }
   { dg-final { if ![file exist n_ppnum.i] { return }                   } }
   { dg-final { if \{ [grep n_ppnum.i "12p\\+A"] != ""          \} \{   } }
   { dg-final { return \}                                               } }
   { dg-final { fail "n_ppnum.c: pp-number including p+"                } }
 */


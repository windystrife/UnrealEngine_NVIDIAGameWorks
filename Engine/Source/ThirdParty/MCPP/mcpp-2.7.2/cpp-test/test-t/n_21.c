/* n_21.c:  Tokenization (No preprocessing tokens are merged implicitly).   */

/* 21.1:    */
/*  - - -a; */
#define MINUS   -
    -MINUS-a;

/* 21.2:    */
#define sub( a, b)  a-b     /* '(a)-(b)' is better  */
#define Y   -y              /* '(-y)' is better     */
/*  x- -y;  */
    sub( x, Y);

/* { dg-do preprocess }
   { dg-final { if ![file exist n_21.i] { return }                      } }
   { dg-final { if \{ [grep n_21.i "- +- +- *a"] != ""          \} \{   } }
   { dg-final { if \{ [grep n_21.i "x *- +- *y"] != ""          \} \{   } }
   { dg-final { return \} \}                                            } }
   { dg-final { fail "n_21.c: tokenization of expanded macro"           } }
 */


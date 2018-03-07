/* e_pragma.c:  Erroneous use of _Pragma() operator */

/* { dg-do preprocess } */
/* { dg-options "-std=c99 -pedantic-errors" } */

/* Operand of _Pragma() should be a string literal  */
    _Pragma( This is not a string literal)
/* { dg-error "_Pragma takes a parenthesized string literal| Operand of _Pragma\\(\\) is not a string literal" "" { target *-*-* } 7 } */


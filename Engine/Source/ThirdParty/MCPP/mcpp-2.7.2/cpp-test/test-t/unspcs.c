/*
 *  unspcs.c:
 *  1998/08             kmatsui
 *
 *   These texts are unportable ones, because the order of the evaluation is
 * unspecified.  Good preprocessor will warn at these texts even if the
 * results are valid.  Good preprocessor will also document the order of
 * evaluation and the behavior on invalid results.
 *   Note: Order of evaluation of sub-expressions (other than operands of &&,
 * ||, ? :) of #if expression is also unspecified.  The order, however, never
 * affects the result, because #if expression never cause side effect, so no
 * warning is necessary.  Precedence and grouping rules of operators are other
 * things than order of evaluation, and shall be obeyed by preprocessor.
 *
 *  2003/03 Rewritten for testsuite.        kmatsui
 */

/* { dg-do preprocess } */
/* { dg-options "-ansi -pedantic -Wall" }   */

#define str( a)     # a
#define xstr( a)    str( a)

/* s.1.1:   Order of evaluation of #, ## operators. */
#define MAKWIDESTR( s)  L ## # s    /* { dg-warning "Macro with mixing of ## and # operators isn't portable" } */
/*  Either of L"name"; or L# name; ("L#" is not a valid pp-token).  */
    MAKWIDESTR( name);

/* s.1.2:   Order of evaluation of ## operators.    */
#define glue3( a, b, c)     a ## b ## c     /* { dg-warning "Macro with multiple ## operators isn't portable" } */
/*  "1.a" or undefined, since .a is not a valid pp-token, while 1. and 1.a are
        valid pp-tokens.    */
    xstr( glue3( 1, ., a));


/* e_14_3.c:    Illegal #if expressions-3.  */

/* { dg-do preprocess } */
/* { dg-options "-ansi -pedantic-errors -w" } */

#define A   1
#define B   1

/* 14.3:    Unterminated #if expression.    */
#if     0 <     /* { dg-error "no right operand| (parse|syntax) error| Unterminated expression" } */
#endif
#if     ( (A == B)  /* { dg-error "missing '\\)' in expression| (parse|syntax) error| Missing \"\\)\"" } */
#endif

/* 14.4:    Unbalanced parenthesis in #if defined operator. */
#if     defined ( MACRO     /* { dg-error "missing '\\)' after \"defined\"| Bad defined syntax" } */
#endif

/* 14.5:    No argument.    */
#if             /* { dg-error "#if with no expression| (parse|syntax) error| No argument" } */
#endif

/* 14.6:    Macro expanding to 0 token in #if expression.   */
#define ZERO_TOKEN
#if     ZERO_TOKEN  /* { dg-error "#if with no expression| (parse|syntax) error| Unterminated expression" } */
#endif


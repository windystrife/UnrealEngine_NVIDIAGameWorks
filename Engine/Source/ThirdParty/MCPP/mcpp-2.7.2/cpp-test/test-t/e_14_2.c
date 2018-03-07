/* e_14_2.c:    Illegal #if expressions-2.  */

/* { dg-do preprocess } */
/* { dg-options "-ansi -pedantic-errors -w" } */

#define A   1
#define B   1

/* 14.2:    Operators =, +=, ++, etc. are not allowed in #if expression.*/

#if     A = B   /* { dg-error "is not valid| (parse|syntax) error| Can't use the operator" } */
#endif
#if     A++ B   /* { dg-error "is not (valid|allowed)| Can't use the operator" } */
#endif
#if     A.B     /* { dg-error "is not valid| (parse|syntax) error| empty #if expression| Can't use the operator" } */
#endif


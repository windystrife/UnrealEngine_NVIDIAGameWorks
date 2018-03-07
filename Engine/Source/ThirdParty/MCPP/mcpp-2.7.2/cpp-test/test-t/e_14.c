/* e_14.c:  Illegal #if expressions.    */

/* { dg-do preprocess } */
/* { dg-options "-ansi -pedantic-errors -w" } */

#define A   1
#define B   1

/* 14.1:    String literal is not allowed in #if expression.    */
#if     "string"    /* { dg-error "not (valid|allowed) in (#if|preprocessor)( expressions|)| Can't use a string literal" } */
#endif      /* The second error ?   */


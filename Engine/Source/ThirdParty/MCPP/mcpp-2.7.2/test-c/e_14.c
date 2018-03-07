/* e_14.c:  Illegal #if expressions.    */

#define A   1
#define B   1

/* 14.1:    String literal is not allowed in #if expression.    */
#if     "string"
#endif      /* The second error ?   */

/* 14.2:    Operators =, +=, ++, etc. are not allowed in #if expression.    */
#if     A = B
#endif
#if     A++ B
#endif
#if     A --B
#endif
#if     A.B
#endif

/* 14.3:    Unterminated #if expression.    */
#if     0 <
#endif
#if     ( (A == B)
#endif

/* 14.4:    Unbalanced parenthesis in #if defined operator. */
#if     defined ( MACRO
#endif

/* 14.5:    No argument.    */
#if
#endif

/* 14.6:    Macro expanding to 0 token in #if expression.   */
#define ZERO_TOKEN
#if     ZERO_TOKEN
#endif

main( void)
{
    return  0;
}


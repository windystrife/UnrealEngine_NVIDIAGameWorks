/* e_24_6.c:    Operand of # operator in function-like macro definition shall
        be a parameter name.    */

/* { dg-do preprocess } */

/* 24.6:    */
#define FUNC( a)    # b     /* { dg-error "not followed by a macro parameter| should be followed by a macro argument name| Not a formal parameter" } */


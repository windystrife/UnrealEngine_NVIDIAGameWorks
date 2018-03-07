/* e_23_3.c:    ## operator shall not occur at the beginning or at the end of
        replacement list for either form of macro definition.   */

/* { dg-do preprocess } */

/* 23.3:    In object-like macro.   */
#define con     ## name     /* { dg-error "'##' cannot appear at either end of a macro expansion| `##' at start of macro definition| No token before ##" } */
#define cat     12 ##       /* { dg-error "'##' cannot appear at either end of a macro expansion| No token after ##" } */

/* 23.4:    In function-like macro. */
#define CON( a, b)  ## a ## b   /* { dg-error "'##' cannot appear at either end of a macro expansion| `##' at start of macro definition| No token before ##" } */
#define CAT( b, c)  b ## c ##   /* { dg-error "'##' cannot appear at either end of a macro expansion| No token after ##" } */


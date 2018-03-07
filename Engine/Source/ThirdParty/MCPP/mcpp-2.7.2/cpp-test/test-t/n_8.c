/* n_8.c:   #error directive.   */

/* 8.1: Argument of #error directive is not a subject of macro expansion.
    Output to stderr as an example:
        Preprocessing error directive: MACRO is not a positive number.
        from line 10 of file "n_8.c"
 */
#define MACRO   0
#if MACRO <= 0
#error MACRO is not a positive number.
#endif

/* { dg-do preprocess }
   { dg-error "MACRO is not a positive number." "n_8.c" { target *-*-* } 10 }
 */


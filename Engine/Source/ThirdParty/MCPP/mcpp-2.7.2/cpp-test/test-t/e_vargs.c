/* e_vargs.c: Error of variable arguments macro */

/* { dg-do preprocess } */
/* { dg-options "-std=c99 -pedantic-errors" } */

/* e_vargs1:    Erroneous usage of __VA_ARGS__  */

/* __VA_ARGS__ should not be defined.   */
    #define __VA_ARGS__ (x, y, z)
/* { dg-error "__VA_ARGS__| shouldn't be defined" "" { target *-*-* } 9 } */

/*
 * __VA_ARGS__ should be the parameter name in replacement list
 * corresponding to '...'.
 */
    #define wrong_macro( a, b, __VA_ARGS__) (a + b - __VA_ARGS__)
/* { dg-error "variadic macro\n\[\^ \]*( error:|) __VA_ARGS__| reserved name `__VA_ARGS__'| Illegal parameter" "" { target *-*-* } 16 } */

/* e_vargs2:    Erroneous macro invocation of variable arguments    */
    /* No argument to correspond __VA_ARGS__    */
    #define debug( ...) fprintf( stderr, __VA_ARGS__)
    debug();
/* { dg-warning "Empty argument" "" { target *-*-* } 22 } */
/* dg-warning, not dg-error to avoid a problem of GCC 4.3 testsuite */

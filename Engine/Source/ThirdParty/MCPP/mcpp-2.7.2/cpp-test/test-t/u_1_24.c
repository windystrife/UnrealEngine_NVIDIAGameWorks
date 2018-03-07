/* u_1_24.c:    Undefined behaviors on empty argument of macro call.    */

/* { dg-do preprocess } */
/* { dg-options "-ansi -pedantic -Wall" }   */

/* u.1.24:  Empty argument of macro call.   */
/*
 *   Note: Since no argument and one empty argument cannot be distinguished
 * syntactically, additional dummy argument may be necessary for an
 * intermediate macro to process one empty argument (if possible).
 */

#define ARG( a, dummy)      # a
#define EMPTY
#define SHOWN( n)       printf( "%s : %d\n", # n, n)
#define SHOWS( s)       printf( "%s : %s\n", # s, ARG( s, dummy))
#define add( a, b)      (a + b)
#define sub( a, b)      (a - b)
#define math( op, a, b)     op( a, b)
#define APPEND( a, b)       a ## b

/*  printf( "%s : %d\n", "math( sub, , y)", ( - y));
        or other undefined behavior.    */
    SHOWN( math( sub, , y));    /* { dg-warning "(e|E)mpty argument" } */

/*  printf( "%s : %s\n", "EMPTY", "");
        or other undefined behavior.    */
    SHOWS( EMPTY);              /* { dg-warning "(e|E)mpty argument" } */

/*  printf( "%s : %s\n", "APPEND( CON, 1)", "CON1");    */
    SHOWS( APPEND( CON, 1));

/*  printf( "%s : %s\n", "APPEND( , )", "");
        or other undefined behavior.    */
    SHOWS( APPEND( , ));        /* { dg-warning "(e|E)mpty argument" } */


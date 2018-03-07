/* u_line_s.c:  Undefined behavior on out-of-range #line number.    */

/* { dg-do preprocess } */
/* { dg-options "-std=c99 -pedantic-errors" } */

/*  C99: Line number argument of #line directive should be in range of
    [1..2147483647] */

#line   2147483648  /* { dg-error "out of range" }  */

/*
 * Note: DejaGnu sometimes fails to handle one of the dg-error lines of
 *  #line 0 and #line 2147483648, when those are tested in a single testcase
 *  file.
 *  So, we separated this testcase from u_line.c.
 */

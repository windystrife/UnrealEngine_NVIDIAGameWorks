/* u_line.c:    Undefined behaviors on out-of-range #line number.   */

/* { dg-do preprocess } */
/* { dg-options "-std=c99 -pedantic-errors" } */

/*  C99: Line number argument of #line directive should be in range of
    [1..2147483647] */

#line   0   /* { dg-error "out of range" }  */
#line   11  /* Restore to correct line number   */
#line   2147483647  /* valid here   */
/* line 2147483647  */
/* line 2147483648 ? : out of range */
    __LINE__;   /* 2147483649 ? or -2147483647 ?,
                maybe warned as an out-of-range */
/* { dg-warning "out of range| got beyond range\n\[\^ \]* warning: Line number \"\[\-0-9\]*\" is out of range" "" { target *-*-* } 0 }  */


/* e_31.c:  Illegal macro calls.    */

/* { dg-do preprocess } */

#define sub( a, b)      (a - b)

/* 31.1:    Too many arguments error.   */
    sub( x, y, z);  /* { dg-error "passed 3 arguments, but takes just 2| used with too many \\(3\\) args| More than necessary 2 argument\\(s\\) in macro call" } */

/* 31.2:    Too few arguments error.    */
    sub( x);    /* { dg-error "requires 2 arguments, but only 1 given| used with just one arg| Less than necessary 2 argument\\(s\\) in macro call" } */


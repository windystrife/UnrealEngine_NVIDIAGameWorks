/* e_7_4.c:     #line error.    */

/* { dg-do preprocess } */

/* 7.4:     string literal in #line directive shall be a character string
        literal.    */

#line   123     L"wide"     /* { dg-error "not a valid filename| invalid format| Not a file name" } */
/*  10; "e_7_4.c";   */
    __LINE__; __FILE__;


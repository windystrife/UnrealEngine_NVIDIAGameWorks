/* e_32_5.c:    Range error of character constant.  */

/* { dg-do preprocess } */

/* 32.5:    Value of a numerical escape sequence in character constant should
        be in the range of char.    */
/* Out of range */
#if     '\x123' == 0x123    /* { dg-error "escape sequence out of range| hex character constant does not fit in a byte| 8 bits can't represent escape sequence" } */
#endif


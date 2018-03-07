/* e_33_2.c:    Out of range of numerical escape sequence in wide-char. */

/* { dg-do preprocess } */
/* { dg-options "-ansi -pedantic-errors -Wall" } */

/* 33.2:    Value of a numerical escape sequence in wide-character constant
        should be in the range of wchar_t.  */
#if     L'\xabcdef012' == 0xbcdef012        /* Perhaps out of range.    */
/* { dg-error "escape sequence out of range| (16|32) bits can't represent escape sequence" "" { target *-*-* } 8 } */
#endif


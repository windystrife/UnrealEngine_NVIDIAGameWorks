/* e_33_2.c:    Out of range of numerical escape sequence in wide-char. */

/* 33.2:    Value of a numerical escape sequence in wide-character constant
        should be in the range of wchar_t.  */
#if     L'\xabcdef012' == 0xbcdef012        /* Perhaps out of range.    */
#endif

main( void)
{
    return  0;
}


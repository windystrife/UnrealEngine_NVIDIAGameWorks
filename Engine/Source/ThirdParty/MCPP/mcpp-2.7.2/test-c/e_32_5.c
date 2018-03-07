/* e_32_5.c:    Range error of character constant.  */

/* 32.5:    Value of a numerical escape sequence in character constant should
        be in the range of char.    */
#if     '\x123' == 0x123        /* Out of range.    */
#endif

main( void)
{
    return  0;
}


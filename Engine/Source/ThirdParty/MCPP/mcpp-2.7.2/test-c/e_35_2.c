/* e_35_2.c:    Out of range of character constant. */

/* In ASCII character set.  */
/* 35.2:    */
#if     'abcdefghi' == '\x61\x62\x63\x64\x65\x66\x67\x68\x69'
        /* Perhaps out of range.    */
#endif

main( void)
{
    return  0;
}


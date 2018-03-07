/* e_4_3.c:     Illegal pp-token.   */

/* 4.3:     Empty character constant is an error.   */
#if     '' == 0     /* This line is invalid, maybe skipped. */
#endif              /* This line maybe the second error.    */

main( void)
{
    return  0;
}


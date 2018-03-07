/* u_1_7_big5.c:    Invalid multi-byte character sequence (in string literal,
        character constant, header-name or comment).    */

#define str( a)     # a
#pragma setlocale( "chinese-traditional")   /* For Visual C */
#pragma __setlocale( "big5")                /* For MCPP     */

main( void)
{
    char *  cp = str( "°Å");  /* 0xa181   */
    return  0;
}


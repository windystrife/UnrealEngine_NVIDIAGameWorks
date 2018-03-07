/* u_1_7_gb.c:  Invalid multi-byte character sequence (in string literal,
        character constant, header-name or comment).    */

#define str( a)     # a
#pragma setlocale( "chinese-simplified")    /* For Visual C */
#pragma __setlocale( "gb2312")              /* For MCPP     */

main( void)
{
    char *  cp = str( "± ");  /* 0xb1a0   */
    return  0;
}


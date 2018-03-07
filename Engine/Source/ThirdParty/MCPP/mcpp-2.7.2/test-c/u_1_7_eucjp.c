/* u_1_7_eucjp.c:   Invalid multi-byte character sequence (in string literal,
        character constant, header-name or comment).    */

#define str( a)     # a
#pragma __setlocale( "eucjp")               /* For MCPP     */

main( void)
{
    char *  cp = str( "± ");  /* 0xb1a0   */
    return  0;
}


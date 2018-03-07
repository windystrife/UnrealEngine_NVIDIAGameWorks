/* u_1_7_jis.c: Invalid multi-byte character sequence (in string literal,
        character constant, header-name or comment).    */

#define str( a)     # a
#pragma __setlocale( "jis")                 /* For MCPP     */

main( void)
{
    char *  cp = str( "$B1 (B");  /* 0x3120   */
    return  0;
}


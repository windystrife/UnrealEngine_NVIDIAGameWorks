/* u_1_7_sjis.c:    Invalid multi-byte character sequence (in string literal,
        character constant, header-name or comment).    */

#define str( a)     # a
#pragma setlocale( "japanese")              /* For Visual C */
#pragma __setlocale( "sjis")                /* For MCPP     */

main( void)
{
    char *  cp = str( "‘8");  /* 0x9138   */
    return  0;
}


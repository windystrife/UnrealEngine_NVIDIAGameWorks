/* u_1_7_ksc.c: Invalid multi-byte character sequence (in string literal,
        character constant, header-name or comment).    */

#define str( a)     # a
#pragma setlocale( "korean")                /* For Visual C */
#pragma __setlocale( "ksc5601")             /* For MCPP     */

main( void)
{
    char *  cp = str( "± ");  /* 0xb1a0   */
    return  0;
}


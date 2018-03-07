/* u_1_7_utf8.c:    Invalid multi-byte character sequence (in string literal,
        character constant, header-name or comment).    */

#define str( a)     # a
#pragma __setlocale( "utf8")                /* For MCPP     */

main( void)
{
    char *  cp = str( "å­—");   /* 0xe5ad97 : legal */
    char *  ecp1 = str( "À¯");   /* 0xc0af   : overlong  */
    char *  ecp2 = str( "àŸ¿");   /* 0xe09fbf : overlong  */
    char *  ecp3 = str( "í €");   /* 0xeda080 : UTF-16 surrogate  */

    return  0;
}


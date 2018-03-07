/* u_1_25.c:    Undefined behaviors on undefined macro argument.    */

#include    <stdio.h>
#define str( a)     # a
#define sub( x, y)  (x - y)
#define SUB         sub

main( void)
{
    int     a = 1, b = 2;

/* u.1.25:  Macro argument otherwise parsed as a directive. */
/*  "#define NAME"; or other undefined behaviour.   */
    puts( str(
#define NAME
    ));

#if 0   /* Added by C90: Corrigendum 1 (1994) and deleted by C99    */
/* u.1.26:  Expanded macro replacement list end with name of function-like
        macro.  */
    SUB( a, b);
#endif

    return  0;
}


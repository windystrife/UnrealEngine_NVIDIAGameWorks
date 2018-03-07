/* u_1_23.c:    Undefined behaviors on generating invalid pp-token by #
        operator.   */

#include    <stdio.h>
#define str( a)     # a

main( void)
{

/*  "\\"\"";    This sequence is parsed to three tokens "\\" \ "", and will be
        diagnosed by compiler-proper unless diagnosed by preprocessor.  */
    puts( str( \""));

    return  0;
}


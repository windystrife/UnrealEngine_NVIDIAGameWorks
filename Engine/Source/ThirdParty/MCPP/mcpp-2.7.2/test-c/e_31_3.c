/* e_31_3.c:    Macro call in control line should complete in the line. */

#define glue( a, b)     a ## b
#define str( s)         # s
#define xstr( s)        str( s)

/* 31.3:    Unterminated macro call.    */
#include    xstr( glue( header,
    .h))

main( void)
{
    return  0;
}


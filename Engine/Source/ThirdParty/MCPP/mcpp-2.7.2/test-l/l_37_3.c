/* l_37_3.c:    Translation limits larger than Standard / 3.    */

/* 37.3L:   Significant initial characters in an internal identifier or a
        macro name. */

#include    "defs.h"

main( void)
{
/*  Name of 127 bytes long. */
    int
A123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
B123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde = 127;
    int
A123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
B123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdE = -127;

#ifndef X7F
/*  Name of 255 bytes long. */
    int
A123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
B123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
C123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
D123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde = 255;
    int
A123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
B123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
C123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
D123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdE = -255;
#endif

    fputs( "started\n", stderr);
    assert(
A123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
B123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde
        == 127);
#ifndef X7F
    assert(
A123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
B123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
C123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
D123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde
        == 255);
#endif
    fputs( "success\n", stderr);
    return 0;
}


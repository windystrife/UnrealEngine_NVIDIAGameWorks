/* u_1_11.c:    Undefined behaviors on undefined #include syntax or header-
        name.   */

/* u.1.11:  Header-name containing ', ", \ or "/*". */
/*  Probably illegal filename and fails to open.    */
#include    "../*line.h"

main( void)
{
/*  \ is a legal path-delimiter in MS-DOS or some other OS's.   */
#include    "..\test-t\line.h"

    return  0;
}


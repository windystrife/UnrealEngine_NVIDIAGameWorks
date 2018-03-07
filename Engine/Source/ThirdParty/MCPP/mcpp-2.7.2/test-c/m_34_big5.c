/* m_34_big5.t: Multi-byte character constant encoded in Big-Five.  */

#include    "defs.h"

main( void)
{
    char *  ptr;

    fputs( "started\n", stderr);

/* 34.1:    */

#pragma __setlocale( "big5")                /* For MCPP     */
#pragma setlocale( "chinese-traditional")   /* For Visual C */

#if     '¦r' == '\xa6\x72'
    ptr = "Multi-byte character is encoded in Big-Five.";
#else
    ptr = "I cannot understand Big-Five.";
#endif

    assert( strcmp( ptr, "I cannot understand Big-Five.") != 0);
    fputs( "success\n", stderr);
    return  0;
}


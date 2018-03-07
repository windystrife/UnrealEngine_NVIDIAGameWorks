/* m_34_utf8.c: Multi-byte character constant encoded in UTF-8. */

#include    "defs.h"

main( void)
{
    char *  ptr;

    fputs( "started\n", stderr);

/* 34.1:    */

#pragma __setlocale( "utf8")                /* For MCPP     */
#pragma setlocale( "utf8")                  /* For MCPP on VC  */

#if     '字' == '\xe5\xad\x97'
    ptr = "Multi-byte character is encoded in UTF-8.";
#else
    ptr = "I cannot understand UTF-8.";
#endif

    assert( strcmp( ptr, "I cannot understand UTF-8.") != 0);
    fputs( "success\n", stderr);
    return  0;
}


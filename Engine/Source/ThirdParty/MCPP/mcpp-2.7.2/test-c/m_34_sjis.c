/* m_34_sjis.c: Multi-byte character constant encoded in shift-JIS. */

#include    "defs.h"

main( void)
{
    char *  ptr;

    fputs( "started\n", stderr);

/* 34.1:    */

#pragma __setlocale( "sjis")                /* For MCPP     */
#pragma setlocale( "japanese")              /* For Visual C */

#if     'Žš' == '\x8e\x9a'
    ptr = "Multi-byte character is encoded in shift-JIS.";
#else
    ptr = "I cannot understand shift-JIS.";
#endif

    assert( strcmp( ptr, "I cannot understand shift-JIS.") != 0);
    fputs( "success\n", stderr);
    return  0;
}


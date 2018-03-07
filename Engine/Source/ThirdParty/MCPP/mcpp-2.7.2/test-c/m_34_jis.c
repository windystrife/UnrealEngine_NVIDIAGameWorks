/* m_34_jis.c:  Multi-byte character constant encoded in ISO-2022-JP.   */

#include    "defs.h"

main( void)
{
    char *  ptr;

    fputs( "started\n", stderr);

/* 34.1:    */

#pragma __setlocale( "jis")                 /* For MCPP     */
#pragma setlocale( "jis")                   /* For MCPP on VC   */

#if     '字' == '\x3b\x7a'
    /* This line doesn't work unless "shift states" are processed.  */
    ptr = "Multi-byte character is encoded in ISO-2022-JP.";
#else
    ptr = "I cannot understand ISO-2022-JP.";
#endif

    assert( strcmp( ptr, "I cannot understand ISO-2022-JP.") != 0);
    fputs( "success\n", stderr);
    return  0;
}


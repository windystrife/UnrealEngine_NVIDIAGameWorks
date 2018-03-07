/* m_34_eucjp.c:    Multi-byte character constant encoded in EUC-JP.    */

#include    "defs.h"

main( void)
{
    char *  ptr;

    fputs( "started\n", stderr);

/* 34.1:    */

#pragma __setlocale( "eucjp")               /* For MCPP     */
#pragma setlocale( "eucjp")                 /* For MCPP on VC   */

#if     '»ú' == '\xbb\xfa'
    ptr = "Multi-byte character is encoded in EUC-JP.";
#else
    ptr = "I cannot understand EUC-JP.";
#endif

    assert( strcmp( ptr, "I cannot understand EUC-JP.") != 0);
    fputs( "success\n", stderr);
    return  0;
}


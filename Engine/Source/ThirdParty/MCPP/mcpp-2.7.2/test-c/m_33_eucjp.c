/* m_33_eucjp.c:    Wide character constant encoded in EUC-JP.  */

#include    "defs.h"
#include    <limits.h>
#define     BYTES_VAL   (1 << CHAR_BIT)

main( void)
{
    char *  ptr;

    fputs( "started\n", stderr);

/* 33.1:    L'ch'.  */

#pragma __setlocale( "eucjp")               /* For MCPP     */
#pragma setlocale( "eucjp")                 /* For MCPP on VC   */

#if     L'»ú' == '\xbb' * BYTES_VAL + '\xfa'
    ptr = "Wide character is encoded in EUC-JP.";
#elif   L'»ú' == '\xfa' * BYTES_VAL + '\xbb'
    ptr = "Wide character is encoded in EUC-JP."
    "Inverted order of evaluation.";
#else
    ptr = "I cannot understand EUC-JP.";
#endif

    assert( strcmp( ptr, "I cannot understand EUC-JP.") != 0);
    fputs( "success\n", stderr);
    return  0;
}


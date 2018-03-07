/* m_33_sjis.c: Wide character constant encoded in shift-JIS.   */

#include    "defs.h"
#include    <limits.h>
#define     BYTES_VAL   (1 << CHAR_BIT)

main( void)
{
    char *  ptr;

    fputs( "started\n", stderr);

/* 33.1:    L'ch'.  */

#pragma __setlocale( "sjis")                /* For MCPP     */
#pragma setlocale( "japanese")              /* For Visual C */

#if     L'Žš' == '\x8e' * BYTES_VAL + '\x9a'
    ptr = "Wide character is encoded in shift-JIS.";
#elif   L'Žš' == '\x9a' * BYTES_VAL + '\x8e'
    ptr = "Wide character is encoded in shift-JIS."
    "Inverted order of evaluation.";
#else
    ptr = "I cannot understand shift-JIS.";
#endif

    assert( strcmp( ptr, "I cannot understand shift-JIS.") != 0);
    fputs( "success\n", stderr);
    return  0;
}


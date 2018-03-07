/* m_33_jis.c:  Wide character constant encoded in ISO-2022-JP. */

#include    "defs.h"
#include    <limits.h>
#define     BYTES_VAL   (1 << CHAR_BIT)

main( void)
{
    char *  ptr;

    fputs( "started\n", stderr);

/* 33.1:    L'ch'.  */

#pragma __setlocale( "jis")                 /* For MCPP     */
#pragma setlocale( "jis")                   /* For MCPP on VC   */

#if     L'字' == 0x3b * BYTES_VAL + 0x7a
    /* This line doesn't work unless "shift states" are processed.  */
    ptr = "Wide character is encoded in ISO-2022-JP.";
#elif   L'字' == 0x7a * BYTES_VAL + 0x3b
    /* This line doesn't work unless "shift states" are processed.  */
    ptr = "Wide character is encoded in ISO-2022-JP."
    "Inverted order of evaluation.";
#else
    ptr = "I cannot understand ISO-2022-JP.";
#endif

    assert( strcmp( ptr, "I cannot understand ISO-2022-JP.") != 0);
    fputs( "success\n", stderr);
    return  0;
}


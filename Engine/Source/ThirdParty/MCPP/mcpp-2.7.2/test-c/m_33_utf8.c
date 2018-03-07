/* m_33_utf8.c: Wide character constant encoded in UTF-8.   */

#include    "defs.h"
#include    <limits.h>
#define     BYTES_VAL   (1 << CHAR_BIT)

main( void)
{
    char *  ptr;

    fputs( "started\n", stderr);

/* 33.1:    L'ch'.  */

#pragma __setlocale( "utf8")                /* For MCPP     */
#pragma setlocale( "utf8")                  /* For MCPP on VC   */

#if     L'字' == '\xe5' * BYTES_VAL * BYTES_VAL + '\xad' * BYTES_VAL + '\x97'
    ptr = "Wide character is encoded in UTF-8.";
#elif   L'字' == '\x97' * BYTES_VAL * BYTES_VAL + '\xad' * BYTES_VAL + '\xe5'
    ptr = "Wide character is encoded in UTF-8."
    "Inverted order of evaluation.";
#else
    ptr = "I cannot understand UTF-8.";
#endif

    assert( strcmp( ptr, "I cannot understand UTF-8.") != 0);
    fputs( "success\n", stderr);
    return  0;
}


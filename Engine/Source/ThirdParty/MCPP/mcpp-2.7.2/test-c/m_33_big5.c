/* m_33_big5.c: Wide character constant encoded in Big-Five.    */

#include    "defs.h"
#include    <limits.h>
#define     BYTES_VAL   (1 << CHAR_BIT)

main( void)
{
    char *  ptr;

    fputs( "started\n", stderr);

/* 33.1:    L'ch'.  */

#pragma __setlocale( "big5")                /* For MCPP     */
#pragma setlocale( "chinese-traditional")   /* For Visual C */

#if     L'¦r' == '\xa6' * BYTES_VAL + '\x72'
    ptr = "Wide character is encoded in Big-Five.";
#elif   L'¦r' == '\x72' * BYTES_VAL + '\xa6'
    ptr = "Wide character is encoded in Big-Five."
    "Inverted order of evaluation.";
#else
    ptr = "I cannot understand Big-Five.";
#endif

    assert( strcmp( ptr, "I cannot understand Big-Five.") != 0);
    fputs( "success\n", stderr);
    return  0;
}


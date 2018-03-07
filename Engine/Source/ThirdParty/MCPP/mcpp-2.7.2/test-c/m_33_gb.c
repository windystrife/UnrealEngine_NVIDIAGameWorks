/* m_33_gb.c:   Wide character constant encoded in GB-2312. */

#include    "defs.h"
#include    <limits.h>
#define     BYTES_VAL   (1 << CHAR_BIT)

main( void)
{
    char *  ptr;

    fputs( "started\n", stderr);

/* 33.1:    L'ch'.  */

#pragma __setlocale( "gb2312")              /* For MCPP     */
#pragma setlocale( "chinese-simplified")    /* For Visual C */

#if     L'×Ö' == '\xd7' * BYTES_VAL + '\xd6'
    ptr = "Wide character is encoded in GB 2312.";
#elif   L'×Ö' == '\xd6' * BYTES_VAL + '\xd7'
    ptr = "Wide character is encoded in GB 2312."
    "Inverted order of evaluation.";
#else
    ptr = "I cannot understand GB-2312.";
#endif

    assert( strcmp( ptr, "I cannot understand GB-2312.") != 0);
    fputs( "success\n", stderr);
    return  0;
}


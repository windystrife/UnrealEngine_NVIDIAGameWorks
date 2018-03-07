/* m_33_ksc.c:  Wide character constant encoded in KSC-5601.    */

#include    "defs.h"
#include    <limits.h>
#define     BYTES_VAL   (1 << CHAR_BIT)

main( void)
{
    char *  ptr;

    fputs( "started\n", stderr);

/* 33.1:    L'ch'.  */

#pragma __setlocale( "ksc5601")             /* For MCPP     */
#pragma setlocale( "korean")                /* For Visual C */

#if     L'í®' == '\xed' * BYTES_VAL + '\xae'
    ptr = "Wide character is encoded in KSC-5601.";
#elif   L'í®' == '\xae' * BYTES_VAL + '\xed'
    ptr = "Wide character is encoded in KSC-5601."
    "Inverted order of evaluation.";
#else
    ptr = "I cannot understand KSC-5601.";
#endif

    assert( strcmp( ptr, "I cannot understand KSC-5601.") != 0);
    fputs( "success\n", stderr);
    return  0;
}


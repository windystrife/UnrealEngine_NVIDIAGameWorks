/* m_34_ksc.c:  Multi-byte character constant encoded in KSC-5601.  */

#include    "defs.h"

main( void)
{
    char *  ptr;

    fputs( "started\n", stderr);

/* 34.1:    */

#pragma __setlocale( "ksc5601")             /* For MCPP     */
#pragma setlocale( "korean")                /* For Visual C */

#if     'í®' == '\xed\xae'
    ptr = "Multi-byte character is encoded in KSC-5601.";
#else
    ptr = "I cannot understand KSC-5601.";
#endif

    assert( strcmp( ptr, "I cannot understand KSC-5601.") != 0);
    fputs( "success\n", stderr);
    return  0;
}


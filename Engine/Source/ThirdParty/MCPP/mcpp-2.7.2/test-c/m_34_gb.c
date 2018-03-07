/* m_34_gb.c:   Multi-byte character constant encoded in GB-2312.   */

#include    "defs.h"

main( void)
{
    char *  ptr;

    fputs( "started\n", stderr);

/* 34.1:    */

#pragma __setlocale( "gb2312")              /* For MCPP     */
#pragma setlocale( "chinese-simplified")    /* For Visual C */

#if     '×Ö' == '\xd7\xd6'
    ptr = "Multi-byte character is encoded in GB-2312.";
#else
    ptr = "I cannot understand GB-2312.";
#endif

    assert( strcmp( ptr, "I cannot understand GB-2312.") != 0);
    fputs( "success\n", stderr);
    return  0;
}


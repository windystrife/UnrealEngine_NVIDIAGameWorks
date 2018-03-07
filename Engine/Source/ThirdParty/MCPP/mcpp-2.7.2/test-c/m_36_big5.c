/* m_36_big5.c: Handling of '\\' in BigFive multi-byte character.    */

#include    "defs.h"

#define     str( a)     # a

main( void)
{
    fputs( "started\n", stdout);

/* 36.1:    0x5c in multi-byte character is not an escape character */

#pragma __setlocale( "big5")                /* For MCPP     */
#pragma setlocale( "chinese-traditional")   /* For Visual C */

#if     '¦r' == '\xa6\x72' && '¥\' != '\xa5\x5c'
    fputs( "Bad handling of '\\' in multi-byte character", stdout);
    exit( 1);
#endif

/* 36.2:    # operater should not insert '\\' before 0x5c in multi-byte
        character   */
    assert( strcmp( str( "¥\ÁZ"), "\"¥\ÁZ\"") == 0);
    fputs( "¥\ÁZ" "\"¥\ÁZ\"\n", stdout);

    fputs( "success\n", stdout);
    return  0;
}


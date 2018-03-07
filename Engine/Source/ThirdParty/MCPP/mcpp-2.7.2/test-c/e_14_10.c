/* e_14_10.c:   Overflow of constant expression in #if directive.   */

/* 14.10:   */
#include    <limits.h>

#if     LONG_MAX - LONG_MIN
#endif
#if     LONG_MAX + 1
#endif
#if     LONG_MIN - 1
#endif
#if     LONG_MAX * 2
#endif

int main( void)
{
    return  0;
}


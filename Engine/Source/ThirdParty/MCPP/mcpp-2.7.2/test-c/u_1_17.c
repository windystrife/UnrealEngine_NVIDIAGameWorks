/* u_1_17.c:    Undefined behaviors on out-of-range #line number.   */

#include    <stdio.h>

main( void)
{

/* u.1.17:  Line number argument of #line directive should be in range of
        [1,32767].  */
#line   32767   /* valid here   */
/* line 32767   */
/* line 32768 ? : out of range  */
    printf( "%d\n", __LINE__);
                    /* 32769 ? or -32767 ?, maybe warned as an out-of-range */
#line   0
#line   32768

/* u.1.18:  Line number argument of #line directive should be written in
        decimal digits. */
#line   0x1000

/*  23, u_1_17.c or other undefined results.    */
    printf( "%d, %s\n", __LINE__, __FILE__);

    return  0;
}


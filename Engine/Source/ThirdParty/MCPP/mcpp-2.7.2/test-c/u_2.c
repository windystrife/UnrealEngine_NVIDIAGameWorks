/* u_2.c:   Undefined behaviors on undefined constant expression.   */

/* u.2.1:   Undefined escape sequence.  */
#if     '\x'
#endif

/* u.2.2:   Illegal bit shift count.    */
#if     1 << -1
#endif
#if     1 << 64
#endif

main( void)
{
    return  0;
}


/* u_1_5.c:     Undefined behaviors on illegal characters.  */

/* u.1.5:   Illegal characters (in other than string literal, character
        constant, header-name or comment).  */
#if     1 ||2
/*    0x01e ^ ^ 0x1f    */
#endif  /* Maybe the second error.  */

/* u.1.6:   [VT], [FF] in directive line.   */
#if     1 ||2
/*     [VT] ^ ^ [FF]    */
#endif  /* Maybe the second error.  */

main( void)
{
    return  0;
}


/* e_15_3.c:    #ifdef, #ifndef syntax errors.  */

/* 15.3:    Not an identifier.  */
#ifdef  "string"
#endif
#ifdef  123
#endif

/* 15.4:    Excessive token sequence.   */
#ifdef  MACRO   Junk
#endif

/* 15.5:    No argument.    */
#ifndef
#endif

main( void)
{
    return  0;
}


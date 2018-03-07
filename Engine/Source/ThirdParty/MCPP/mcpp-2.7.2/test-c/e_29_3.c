/* e_29_3.c:    #undef errors.  */

/* 29.3:    Not an identifier.  */
#undef  "string"
#undef  123

/* 29.4:    Excessive token sequence.   */
#undef  MACRO_0     Junk

/* 29.5:    No argument.    */
#undef

main( void)
{
    return  0;
}


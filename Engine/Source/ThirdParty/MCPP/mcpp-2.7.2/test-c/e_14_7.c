/* e_14_7.c:    There is no keyword in #if expression.  */

/* 14.7:    sizeof operator is disallowed.  */
/*  Evaluated as: 0 (0)
    Constant expression syntax error.   */
#if     sizeof (int)
#endif

/* 14.8:    type cast is disallowed.    */
/*  Evaluated as: (0)0x8000
    Also a constant expression error.   */
#if     (int)0x8000 < 0
#endif

main( void)
{
    return  0;
}


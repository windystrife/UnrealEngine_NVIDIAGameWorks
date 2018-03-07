/* e_24_6.c:    Operand of # operator in function-like macro definition should
        be a parameter. */

/* 24.6:    */
#define FUNC( a)    # b

main( void)
{
    return  0;
}


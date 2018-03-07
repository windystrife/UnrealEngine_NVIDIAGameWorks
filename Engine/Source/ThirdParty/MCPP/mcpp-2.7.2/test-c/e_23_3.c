/* e_23_3.c:    ## operator shall not occur at the beginning or at the end of
        replacement list for either form of macro definition.   */

/* 23.3:    In object-like macro.   */
#define con     ## name
#define cat     12 ##

/* 23.4:    In function-like macro. */
#define CON( a, b)  ## a ## b
#define CAT( b, c)  b ## c ##

main( void)
{
    return  0;
}


/* e_31.c:  Illegal macro calls.    */

#define sub( x, y)      (x - y)

/* 31.1:    Too many arguments error.   */
    sub( x, y, z);

/* 31.2:    Too few arguments error.    */
    sub( x);

main( void)
{
    return  0;
}


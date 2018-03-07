/* e_27_7.c:    Error of rescanning.    */

#define sub( x, y)      (x - y)

/* 27.7:    */
#define TWO_TOKENS      a,b
#define SUB( x, y)      sub( x, y)
/* Too many arguments error while rescanning after once replaced to:
    sub( a,b, 1);   */
    SUB( TWO_TOKENS, 1);

main( void)
{
    return  0;
}


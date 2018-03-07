/* n_token.t:   special pp-token in C++ */

#define str( a)     xstr( a)
#define xstr( a)    # a
#define paste( a, b)    a ## b

/* token.1: '::' is a token in C++, although illegal token sequence
    in C    */
    /* "::";    */
    str( paste( :, :));

/* token.2: 'and', 'or', 'not', etc. are tokens in C++, although in C
    these are macros defined by <iso646.h>  */
    /* "xor";   */
#if 1 and 2 or not 3
    str( xor);
#endif


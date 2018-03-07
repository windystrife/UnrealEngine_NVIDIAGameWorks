/* n_token.C:   special pp-token in C++ */

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

/* { dg-do preprocess }
   { dg-options "-std=c++98" }
   { dg-final { if ![file exist n_token.i] { return }                   } }
   { dg-final { if \{ [grep n_token.i "\"::\""] != ""           \} \{   } }
   { dg-final { if \{ [grep n_token.i "\"xor\""] != ""          \} \{   } }
   { dg-final { return \} \}                                            } }
   { dg-final { fail "n_token.C: C++ tokens"                            } }
 */


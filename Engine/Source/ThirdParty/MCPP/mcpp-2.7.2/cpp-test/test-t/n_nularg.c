/* n_nularg.c:  Empty argument of macro call.   */

#define ARG( a)         # a
#define EMPTY
#define SHOWN( n)       printf( "%s : %d\n", # n, n)
#define SHOWS( s)       printf( "%s : %s\n", # s, ARG( s))
#define add( a, b)      (a + b)
#define sub( a, b)      (a - b)
#define math( op, a, b)     op( a, b)
#define APPEND( a, b)       a ## b

/*  printf( "%s : %d\n", "math( sub, , y)", ( - y));    */
    SHOWN( math( sub, , y));

/*  printf( "%s : %s\n", "EMPTY", "");  */
    SHOWS( EMPTY);

/*  printf( "%s : %s\n", "APPEND( CON, 1)", "CON1");    */
    SHOWS( APPEND( CON, 1));

/*  printf( "%s : %s\n", "APPEND( , )", "");  */
    SHOWS( APPEND( , ));

/* { dg-do preprocess }
   { dg-options "-std=c99 -w" }
   { dg-final { if ![file exist n_nularg.i] { return }                  } }
   { dg-final { if \{ [grep n_nularg.i "printf\\( \"%s : %d\\\\n\", \"math\\( sub, , y\\)\", *\\( *- *y *\\) *\\)" ] != ""  \} \{   } }
   { dg-final { if \{ [grep n_nularg.i "printf\\( \"%s : %s\\\\n\", \"EMPTY\", *\"\" *\\)" ] != ""  \} \{   } }
   { dg-final { if \{ [grep n_nularg.i "printf\\( \"%s : %s\\\\n\", \"APPEND\\( CON, 1\\)\", *\"CON1\" *\\)" ] != ""    \} \{   } }
   { dg-final { if \{ [grep n_nularg.i "printf\\( \"%s : %s\\\\n\", \"APPEND\\( , \\)\", *\"\" *\\)" ] != ""        \} \{   } }
   { dg-final { return \} \} \} \}                                      } }
   { dg-final { fail "n_nularg.c: empty argument"                       } }
 */


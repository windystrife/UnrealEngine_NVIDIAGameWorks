/* n_24.c:  # operator in macro definition / 1. */

#define str( a)     # a

/* 24.1:    */
/*  "a+b";  */
    str( a+b);

/* 24.2:    White spaces between tokens of operand are converted to one space.
 */
/*  "ab + cd";  */
    str(    ab  /* comment */   +
        cd  );

/* 24.5:    Token separator inserted by macro expansion should be removed.
        (Meanwhile, tokens should not be merged.  See 21.2.)    */
#define xstr( a)    str( a)
#define f(a)        a
/*  "x-y";  */
    xstr( x-f(y));

/* { dg-do preprocess }
   { dg-final { if ![file exist n_24.i] { return }                      } }
   { dg-final { if \{ [grep n_24.i "\"a\\+b\""] != ""           \} \{   } }
   { dg-final { if \{ [grep n_24.i "\"ab \\+ cd\""] != ""       \} \{   } }
   { dg-final { if \{ [grep n_24.i "\"x-y\""] != ""             \} \{   } }
   { dg-final { return \} \} \}                                         } }
   { dg-final { fail "n_24.c: #operator"                                } }
 */


/* n_30.c:  Macro call. */
/*  Note:   Comma separate the arguments of function-like macro call,
        but comma between matching inner parenthesis doesn't.  This feature
        is tested on so many places in this suite especially on *.c samples
        which use assert() macro, that no separete item to test this feature
        is provided.    */

/* 30.1:    A macro call may cross the lines.   */
#define FUNC( a, b, c)      a + b + c
/*  a + b + c;  */
    FUNC
    (
        a,
        b,
        c
    )
    ;

/* { dg-do preprocess }
   { dg-final { if ![file exist n_30.i] { return }                      } }
   { dg-final { if \{ [grep n_30.i "a *\\+ *b *\\+ *c"] != ""   \} \{   } }
   { dg-final { return \}                                               } }
   { dg-final { if \{ [grep n_30.i "a *\\+ *"] != ""            \} \{   } }
   { dg-final { if \{ [grep n_30.i "b *\\+ *"] != ""            \} \{   } }
   { dg-final { if \{ [grep n_30.i "c"] != ""                   \} \{   } }
   { dg-final { return \} \} \}                                         } }
   { dg-final { fail "n_30.c: macro call on multi-line"                 } }
 */


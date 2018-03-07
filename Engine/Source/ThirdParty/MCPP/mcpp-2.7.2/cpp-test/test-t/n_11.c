/* n_11.c:  Operator "defined" in #if or #elif directive.   */

#define MACRO_abc   abc
#define MACRO_xyz   xyz
#define MACRO_0     0
#define ZERO_TOKEN

/* 11.1:    */
/*  abc;    */
/*  xyz;    */
#if     defined a
    a;
#else
    MACRO_abc;
#endif
#if     defined (MACRO_xyz)
    MACRO_xyz;
#else
    0;
#endif

/* 11.2:    "defined" is an unary operator whose result is 1 or 0.  */
#if     defined MACRO_0 * 3 != 3
#error  Bad handling of "defined" operator.
#endif
#if     (!defined ZERO_TOKEN != 0) || (-defined ZERO_TOKEN != -1)
#error  Bad grouping of "defined", !, - operator.
#endif

/* { dg-do preprocess }
   { dg-final { if ![file exist n_11.i] { return }                      } }
   { dg-final { if \{ [grep n_11.i "abc"] != ""                 \} \{   } }
   { dg-final { if \{ [grep n_11.i "xyz"] != ""                 \} \{   } }
   { dg-final { return \} \}                                            } }
   { dg-final { fail "n_11.c: 'defined' operator"                       } }
 */


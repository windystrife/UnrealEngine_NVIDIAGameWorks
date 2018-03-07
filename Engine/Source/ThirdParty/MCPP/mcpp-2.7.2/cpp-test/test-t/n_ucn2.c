/* n_ucn2.c:    Universal-character-name    */

/* UCN in pp-number */

#define mkint( a)   a ## 1\u5B57

    int mkint( abc) = 0;  /* int abc1\u5B57 = 0;    */

/* { dg-do preprocess }
   { dg-options "-std=c99" }
   { dg-final { if ![file exist n_ucn2.i] { return }                    } }
   { dg-final { if \{ [grep n_ucn2.i "int *abc1\\\\u5\[Bb\]57 *= *0"] != "" \} \{    } }
   { dg-final { return \}                                               } }
   { dg-final { fail "n_ucn2.c: UCN in pp-number"                       } }
 */


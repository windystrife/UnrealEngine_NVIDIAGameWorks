/* n_20.c:  Definition of macro lexically identical to keyword. */

/* 20.1:    */
/*  double fl;  */
#define float   double
    float   fl;

/* { dg-do preprocess }
   { dg-final { if ![file exist n_20.i] { return }                      } }
   { dg-final { if \{ [grep n_20.i "double +fl"] != ""          \} \{   } }
   { dg-final { return \}                                               } }
   { dg-final { fail "n_20.c: macro identical to keyword"               } }
 */


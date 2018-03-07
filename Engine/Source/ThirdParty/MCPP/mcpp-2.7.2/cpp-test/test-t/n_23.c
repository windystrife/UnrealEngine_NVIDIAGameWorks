/* n_23.c:  ## operator in macro definition.    */

#define glue( a, b)     a ## b
#define xglue( a, b)    glue( a, b)

/* 23.1:    */
/*  xy; */
    glue( x, y);

/* 23.2:    Generate a preprocessing number.    */
/*  .12e+2; */
#define EXP     2
    xglue( .12e+, EXP);

/* { dg-do preprocess }
   { dg-final { if ![file exist n_23.i] { return }                      } }
   { dg-final { if \{ [grep n_23.i "xy"] != ""                  \} \{   } }
   { dg-final { if \{ [grep n_23.i "\.12e\\+2"] != ""           \} \{   } }
   { dg-final { return \} \}                                            } }
   { dg-final { fail "n_23.c: ## operator"                              } }
 */


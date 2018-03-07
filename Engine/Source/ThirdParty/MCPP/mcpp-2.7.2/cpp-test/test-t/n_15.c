/* n_15.c:  #ifdef, #ifndef directives. */

#define MACRO_1     1

/* 15.1:    #ifdef directive.   */
/*  Valid block */
#ifdef  MACRO_1
    Valid block 1
#else
    Block to be skipped
#endif

/* 15.2:    #ifndef directive.  */
/*  Valid block */
#ifndef MACRO_1
    Block to be skipped
#else
    Valid block 2
#endif

/* { dg-do preprocess }
   { dg-final { if ![file exist n_15.i] { return }                      } }
   { dg-final { if \{ [grep n_15.i "Valid block 1"] != ""       \} \{   } }
   { dg-final { if \{ [grep n_15.i "Valid block 2"] != ""       \} \{   } }
   { dg-final { return \} \}                                            } }
   { dg-final { fail "n_15.c: #ifdef, #ifndef directives"               } }
 */


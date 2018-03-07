/* n_bool.C */
/*
 * On C++:  'true' and 'false' are evaluated 1 and 0 respectively.
 *      and logical AND, logical OR are evaluated boolean.
 */

/*  Valid block;    */

#define MACRO   1
#define MACRO3  3

#if MACRO == true
    Valid block 1
#else
    non-Valid block;
#endif

#if (MACRO && MACRO3) == true
    Valid block 2
#else
    non-Valid block;
#endif

/* { dg-do preprocess }
   { dg-options "-std=c++98" }
   { dg-final { if ![file exist n_bool.i] { return }                    } }
   { dg-final { if \{ [grep n_bool.i "Valid block 1"] != ""     \} \{   } }
   { dg-final { if \{ [grep n_bool.i "Valid block 2"] != ""     \} \{   } }
   { dg-final { return \} \}                                            } }
   { dg-final { fail "n_bool.C: boolean literal"                        } }
 */


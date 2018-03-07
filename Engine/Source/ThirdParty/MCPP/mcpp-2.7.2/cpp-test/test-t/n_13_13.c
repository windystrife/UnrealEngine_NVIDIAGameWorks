/* n_13_13.c:   #if expression with macros. */

#define ZERO_TOKEN
#define MACRO_0         0
#define MACRO_1         1
#define and             &&
#define or              ||
#define not_eq          !=
#define bitor           |

/* 13.13:   With macros expanding to operators. */
/*  Valid block */
#if     (1 bitor 2) == 3 and 4 not_eq 5 or 0
    /* #if (1 | 2) == 3 && 4 != 5 || 0  */
    Valid block 1
#else
    Block to be skipped
#endif

/* 13.14:   With macros expanding to 0 token, nonsence but legal expression.*/
/*  Valid block */
#if     ZERO_TOKEN MACRO_1 ZERO_TOKEN > ZERO_TOKEN MACRO_0 ZERO_TOKEN
    /* #if 1 > 0    */
    Valid block 2
#else
    Block to be skipped
#endif

/* { dg-do preprocess }
   { dg-options "-ansi -w" }
   { dg-final { if ![file exist n_13_13.i] { return }                   } }
   { dg-final { if \{ [grep n_13_13.i "Valid block 1"] != ""    \} \{   } }
   { dg-final { if \{ [grep n_13_13.i "Valid block 2"] != ""    \} \{   } }
   { dg-final { return \} \}                                            } }
   { dg-final { fail "n_13_13.c: #if expression with macros"            } }
 */


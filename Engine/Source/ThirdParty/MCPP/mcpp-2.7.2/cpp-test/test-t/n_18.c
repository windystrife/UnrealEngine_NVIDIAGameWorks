/* n_18.c:  #define directive.  */

/* Excerpts from ISO C 6.8.3 "Examples".    */
#define OBJ_LIKE        (1-1)
#define FTN_LIKE(a)     ( a )

#define ZERO_TOKEN

/* 18.1:    Definition of an object-like macro. */
/*  (1-1);  */
    OBJ_LIKE;
/* Macro defined to no token.   */
/*  ;   */
    ZERO_TOKEN;

/* 18.2:    Definition of a function-like macro.    */
/*  ( c );  */
    FTN_LIKE( c);

/* 18.3:    Spelling in string identical to parameter is not a parameter.   */
/*  "n1:n2";    */
#define STR( n1, n2)    "n1:n2"
    STR( 1, 2);

/* { dg-do preprocess }
   { dg-final { if ![file exist n_18.i] { return }                      } }
   { dg-final { if \{ [grep n_18.i "\\(1-1\\)"] != ""           \} \{   } }
   { dg-final { if \{ [grep n_18.i "^\[ \]*;\[ \]*$"] != ""     \} \{   } }
   { dg-final { if \{ [grep n_18.i "\\( *c *\\)"] != ""         \} \{   } }
   { dg-final { if \{ [grep n_18.i "\"n1:n2\""] != ""           \} \{   } }
   { dg-final { return \} \} \} \}                                      } }
   { dg-final { fail "n_18.c: #define directive"                        } }
 */


/* n_19.c:  Valid re-definitions of macros. */

/* Excerpts from ISO C 6.8.3 "Examples".    */
#define OBJ_LIKE        (1-1)
#define FTN_LIKE(a)     ( a )

/* 19.1:    */
#define OBJ_LIKE    /* white space */  (1-1) /* other */

/* 19.2:    */
#define FTN_LIKE( a     )(  /* note the white space */  \
                        a  /* other stuff on this line
                           */ )
/*  ( c );  */
    FTN_LIKE( c);

/* { dg-do preprocess }
   { dg-final { if ![file exist n_19.i] { return }                      } }
   { dg-final { if \{ [grep n_19.i "\\( *c *\\)"] != ""         \} \{   } }
   { dg-final { return \}                                               } }
   { dg-final { fail "n_19.c: valid re-definition of macros"            } }
 */


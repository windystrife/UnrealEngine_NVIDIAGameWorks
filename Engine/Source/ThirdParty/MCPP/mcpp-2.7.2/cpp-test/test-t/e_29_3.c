/* e_29_3.c:    #undef errors.  */

/* { dg-do preprocess } */

/* 29.3:    Not an identifier.  */
#undef  "string"    /* { dg-error "macro names must be identifiers| invalid macro name| Not an identifier" } */
#undef  123         /* { dg-error "macro names must be identifiers| invalid macro name| Not an identifier" } */

/* 29.4:    Excessive token sequence.   */
#undef  MACRO_0     Junk    /* { dg-error "extra tokens| garbage after| Excessive token sequence" } */

/* 29.5:    No argument.    */
#undef      /* { dg-error "no macro name| invalid macro name| No identifier" } */


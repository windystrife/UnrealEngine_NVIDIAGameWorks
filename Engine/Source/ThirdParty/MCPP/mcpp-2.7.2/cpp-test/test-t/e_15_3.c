/* e_15_3.c:    #ifdef, #ifndef syntax errors.  */

/* { dg-do preprocess } */

/* 15.3:    Not an identifier.  */
#ifdef  "string"    /* { dg-error "macro names must be identifiers|argument starts with punctuation| Not an identifier" } */
#endif
#ifdef  123         /* { dg-error "macro names must be identifiers|argument starts with a digit| Not an identifier" } */
#endif

/* 15.4:    Excessive token sequence.   */
#ifdef  MACRO   Junk    /* { dg-error "extra tokens|garbage at end| Excessive token sequence" } */
#endif

/* 15.5:    No argument.    */
#ifndef             /* { dg-error "no macro name given| (N|n)o argument" } */
#endif


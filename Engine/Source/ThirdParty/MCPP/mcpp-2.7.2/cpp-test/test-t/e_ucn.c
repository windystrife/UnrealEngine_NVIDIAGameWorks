/* e_ucn.c:     Errors of Universal-character-name sequense.    */

/* { dg-do preprocess } */
/* { dg-options "-std=c99 -pedantic-errors" } */

#define macro\U0000001F /* violation of constraint  */
/* { dg-error "universal-character-name| UCN cannot specify the value" "" { target *-*-* } 6 } */
#define macro\uD800     /* violation of constraint (only C, not for C++)    */
/* { dg-error "universal-character-name| UCN cannot specify the value" "" { target *-*-* } 8 } */
#define macro\u123      /* too short sequence (violation of syntax rule)    */
/* { dg-error "incomplete universal-character-name| Illegal UCN sequence" "" { target *-*-* } 10 } */
#define macro\U1234567  /* also too short sequence  */
/* { dg-error "incomplete universal-character-name| Illegal UCN sequence" "" { target *-*-* } 12 } */


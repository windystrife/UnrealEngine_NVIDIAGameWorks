/* e_4_3.c:     Illegal pp-token.   */

/* { dg-do preprocess } */

/* 4.3:     Empty character constant is an error.   */
#if     '' == 0     /* { dg-error "empty character constant| Empty character constant '', skipped the line\n\[^ \]* error:" "" } */
#endif  /* Maybe the second error   */


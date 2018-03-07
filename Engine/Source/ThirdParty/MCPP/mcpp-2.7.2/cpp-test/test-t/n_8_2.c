/* n_8_2.c:     Argument of #error is optional. */
/* { dg-do preprocess } */

/* 8.2: #error should be executed.  */
#error      /* { dg-error "#error" }    */


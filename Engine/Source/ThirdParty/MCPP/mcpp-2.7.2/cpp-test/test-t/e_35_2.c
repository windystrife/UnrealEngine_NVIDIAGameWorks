/* e_35_2.c:    Out of range of character constant. */

/* { dg-do preprocess } */

/* 35.2:    */
/* Perhaps out of range.    */
#if     'abcdefghi' /* { dg-error "character constant too long| out of range" } */
#endif


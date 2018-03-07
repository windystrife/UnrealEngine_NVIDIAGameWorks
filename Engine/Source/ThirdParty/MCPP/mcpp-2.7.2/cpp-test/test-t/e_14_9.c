/* e_14_9.c:    Out of range in #if expression (division by 0). */

/* { dg-do preprocess } */

/* 14.9:    Divided by 0.   */
#if     1 / 0       /* { dg-error "(D|d)ivision by zero" } */
#endif


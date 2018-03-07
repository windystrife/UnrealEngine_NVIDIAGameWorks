/* e_12_8.c:    Out of range of integer pp-token in #if expression. */

/* { dg-do preprocess } */

/* 12.8:    Preprocessing number perhaps out of range of unsigned long. */
#if     123456789012345678901   /* { dg-error "(C|c)onstant (\"\[0-9\]*\" is |)(out of range|is too large for its type)" } */
#endif


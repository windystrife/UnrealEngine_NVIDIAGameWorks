/* warn_2.c */

/*
 *   The following texts are legal but suspicious ones.  Good preprocessor
 * will warn at these texts.
 */

/* { dg-do preprocess } */
/* { dg-options "-ansi -pedantic -Wall" }   */

/* w.2.1:   Negative number converted to positive in #if expression.    */
#if     -1 < 0U     /* { dg-warning "changes sign when promoted| converted to positive" }   */
#endif

/* w.2.2:   Out of range of unsigned type (wraps around and never overflow)
        in #if expression.  */
#if     0U - 1      /* { dg-warning "out of range" }    */
#endif


/* warn_1_1.c   */

/*
 *   The following text is legal but suspicious one.  Good preprocessor
 * will warn at this text.
 */

/* { dg-do preprocess } */
/* { dg-options "-ansi -pedantic -Wall" }   */

/* w.1.1:   "/*" in comment.    */ /* { dg-warning "(\"/\\*\"|`/\\*') within comment" }  */
/*  comment /*  nested comment and no closing   */ /* { dg-warning "(\"/\\*\"|`/\\*') within comment" }  */


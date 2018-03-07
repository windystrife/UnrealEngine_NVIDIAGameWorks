/* warn_1_2.c   */

/*
 *   The following text is legal but suspicious one.  Good preprocessor
 * will warn at this text.
 */

/* { dg-do preprocess } */
/* { dg-options "-ansi -pedantic -Wall" }   */

/* w.1.2:   Rescanning of replacement text involves succeding text. */
#define sub( x, y)      (x - y)
#define head            sub(
    int     a = 1, b = 2, c;
    c = head a,b );             /* { dg-warning "involved subsequent text" "macro call involves subsequent text" }  */

#define OBJECT_LIKE     FUNCTION_LIKE
#define FUNCTION_LIKE( x, y)    (x + y)
    c = OBJECT_LIKE( a, b);     /* { dg-warning "involved subsequent text" "macro call involves subsequent text" }  */


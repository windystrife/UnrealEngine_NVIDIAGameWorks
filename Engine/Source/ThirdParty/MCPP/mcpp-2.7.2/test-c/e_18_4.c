/* e_18_4.c:    #define syntax errors.  */

/* 18.4:    Not an identifier.  */
#define "string"
#define 123

/* 18.5:    No argument.    */
#define

/* 18.6:    Empty parameter list.   */
#define math( op, a, )      op( (a), (b))

/* 18.7:    Duplicate parameter names.  */
#define math( op, a, a)     op( (a), (b))

/* 18.8:    Argument is not an identifier.  */
#define NUMARGS( 1, +, 2)   (1 + 2)

/* 18.9:    No space between macro name and replacement text.   */
/*
    C90 (Corrigendum 1) forbids this if and only the replacement text begins
        with a non-basic-character.
    C99 forbids this even when the replacement text begins with basic-
        character.
*/
/*  From ISO 9899:1990 / Corrigendum 1. */
#define THIS$AND$THAT(a, b)     ((a) + (b))
/* Note: the following definition is legal (object-like macro).
#define THIS $AND$THAT(a, b)    ((a) + (b))
*/

main( void)
{
    return  0;
}


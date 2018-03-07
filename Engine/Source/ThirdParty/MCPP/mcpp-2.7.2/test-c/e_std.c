/*
 *      e_std.c
 *
 * 1998/08      made public                                     kmatsui
 * 2002/08      revised not to conflict with C99 Standard       kmatsui
 * 2003/11      added a few samples                             kmatsui
 *
 * Samples to test Standard C preprocessing.
 * Preprocessor must diagnose all of these samples appropriately.
 */


void    e_19_3( void);
void    e_25_6( void);
void    e_27_6( void);
void    e_31( void);

main( void)
{
    e_19_3();
    e_25_6();
    e_27_6();
    e_31();

    return  0;
}


/*      Illegal pp-token.   */

/* 4.3:     Empty character constant is an error.   */
#if     '' == 0     /* This line is invalid, maybe skipped. */
#endif              /* This line maybe the second error.    */


/*      #line error.    */

/* 7.4:     string literal in #line directive shall be a character string
    literal.    */

#line   123     L"wide"


/*      Out of range of integer pp-token in #if expression. */
/* Note:    Tests of character constant overflow are in 32.5, 33.2, 35.2.   */

/* 12.8:    Preprocessing number perhaps out of range of unsigned long. */
#if     123456789012345678901
#endif


/*      Illegal #if expressions.    */

#define A   1
#define B   1

/* 14.1:    String literal is not allowed in #if expression.    */
#if     "string"
#endif      /* The second error ?   */

/* 14.2:    Operators =, +=, ++, etc. are not allowed in #if expression.    */
#if     A = B
#endif
#if     A++ B
#endif
#if     A --B
#endif
#if     A.B
#endif

/* 14.3:    Unterminated #if expression.    */
#if     0 <
#endif
#if     ( (A == B)
#endif

/* 14.4:    Unbalanced parenthesis in #if defined operator. */
#if     defined ( MACRO
#endif

/* 14.5:    No argument.    */
#if
#endif

/* 14.6:    Macro expanding to 0 token in #if expression.   */
#define ZERO_TOKEN
#if     ZERO_TOKEN
#endif


/*      There is no keyword in #if expression.  */

/* 14.7:    sizeof operator is disallowed.  */
/*  Evaluated as: 0 (0)
    Constant expression syntax error.   */
#if     sizeof (int)
#endif

/* 14.8:    type cast is disallowed.    */
/*  Evaluated as: (0)0x8000
    Also a constant expression error.   */
#if     (int)0x8000 < 0
#endif


/*      Out of range in #if expression (division by 0). */

/* 14.9:    Divided by 0.   */
#if     1 / 0
#endif


/*      Overflow of constant expression in #if directive.   */

/* 14.10:   */
#include    <limits.h>

#if     LONG_MAX - LONG_MIN
#endif
#if     LONG_MAX + 1
#endif
#if     LONG_MIN - 1
#endif
#if     LONG_MAX * 2
#endif


/*      #ifdef, #ifndef syntax errors.  */

/* 15.3:    Not an identifier.  */
#ifdef  "string"
#endif
#ifdef  123
#endif

/* 15.4:    Excessive token sequence.   */
#ifdef  A   Junk
#endif

/* 15.5:    No argument.    */
#ifndef
#endif


/*      Trailing junk of #else, #endif. */

/* 16.1:    Trailing junk of #else. */
#define MACRO_0     0
#if     MACRO_0
#else   MACRO_0

/* 16.2:    Trailing junk of #endif.    */
#endif  MACRO_0


/*      Ill-formed group in a source file.  */

#define MACRO_1     1

/* 17.1:    Error of #endif without #if.    */
#endif

/* 17.2:    Error of #else without #if. */
#else

/* 17.3:    Error of #else after #else. */
#if     MACRO_1
#else                   /* line 168 */
#if     1
#else
#endif
#else
#endif

/* 17.4:    Error of #elif after #else. */
#if     MACRO_1 == 1
#else                   /* line 177 */
#elif   MACRO_1 == 0
#endif

/* 17.5:    Error of #endif without #if in an included file.    */
#if     1
#include    "unbal1.h"

/* 17.6:    Error of unterminated #if section in an included file.  */
#include    "unbal2.h"
#endif

/* 17.7:    Error of unterminated #if section.  */
/* An error would be reported at end of file.   */
#if     MACRO_1 == 0    /* line 191 */
#else


/*      #define syntax errors.  */

/* 18.4:    Not an identifier.  */
#define "string"
#define 123

/* 18.5:    No argument.    */
#define

/* 18.6:    Empty parameter list.   */
#define math( op, a, )      op( (a), (b))

/* 18.7:    Duplicate parameter names.  */
#define math( op, a, a)     op( (a), (b))

/* 18.8:    Parameter is not an identifier. */
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


/*      Redefinitions of macros.    */

#define     str( s)     # s
#define     xstr( s)    str( s)

/* Excerpts from ISO C 6.8.3 "Examples".    */

#define OBJ_LIKE        (1-1)
#define FTN_LIKE(a)     ( a )

void    e_19_3( void)
{
/* The following redefinitions should be diagnosed. */

/* 19.3:    */
#define OBJ_LIKE        (0)     /* different token sequence     */

/* 19.4:    */
#undef  OBJ_LIKE
#define OBJ_LIKE        (1-1)
#define OBJ_LIKE        (1 - 1) /* different white space        */

/* 19.5:    */
#define FTN_LIKE(b)     ( a )   /* different parameter usage    */

/* 19.6:    */
#undef  FTN_LIKE
#define FTN_LIKE(a)     ( a )
#define FTN_LIKE(b)     ( b )   /* different parameter spelling */

/* 19.7:    Not in ISO C "Examples" */
#define FTN_LIKE        OBJ_LIKE
}


/*      ## operator shall not occur at the beginning or at the end of
        replacement list for either form of macro definition.   */

/* 23.3:    In object-like macro.   */
#define con     ## name
#define cat     12 ##

/* 23.4:    In function-like macro. */
#define CON( a, b)  ## a ## b
#define CAT( b, c)  b ## c ##


/*      Operand of # operator in function-like macro definition shall
        be a parameter name.    */

/* 24.6:    */
#define FUNC( a)    # b


/*      Macro arguments are pre-expanded separately.    */

/* 25.6:    */
#define sub( x, y)      (x - y)
#define head            sub(
#define body(x,y)       x,y
#define tail            )
#define head_body_tail( a, b, c)    a b c

void    e_25_6( void)
{
/* "head" is once replaced to "sub(", then rescanning of "sub(" causes an
        uncompleted macro call.  Expansion of an argument should complete
        within the argument.    */
    head_body_tail( head, body(a,b), tail);
}


/*      Error of rescanning.    */

/* 27.7:    */
#define TWO_ARGS        a,b
#define SUB( x, y)      sub( x, y)

void    e_27_7( void)
{
/* Too many arguments error while rescanning after once replaced to:
    sub( a,b, 1);   */
    SUB( TWO_ARGS, 1);
}


/*      #undef errors.  */

/* 29.3:    Not an identifier.  */
#undef  "string"
#undef  123

/* 29.4:    Excessive token sequence.   */
#undef  MACRO_0     Junk

/* 29.5:    No argument.    */
#undef


/*      Illegal macro calls.    */

void    e_31( void)
{
    int     x = 1, y = 2;

/* 31.1:    Too many arguments error.   */
    sub( x, y, z);

/* 31.2:    Too few arguments error.    */
    sub( x);
}


/*      Macro call in control line should complete in the line. */

#define glue( a, b)     a ## b

/* 31.3:    Unterminated macro call.    */
#include    xstr( glue( header,
    .h))


/*      Range error of character constant.  */

/* 32.5:    Value of a numerical escape sequence in character constant should
        be in the range of char.    */
#if     '\x123' == 0x123        /* Out of range.    */
#endif


/*      Out of range of numerical escape sequence in wide-char. */

/* 33.2:    Value of a numerical escape sequence in wide-character constant
        should be in the range of wchar_t.  */
#if     L'\xabcdef012' == 0xbcde012        /* Perhaps out of range.    */
#endif


/*      Out of range of character constant. */

/* In ASCII character set.  */
/* 35.2:    */
#if     'abcdefghi' == '\x61\x62\x63\x64\x65\x66\x67\x68\x69'
        /* Perhaps out of range.    */
#endif


/* Error of "unterminated #if section started at line 191" will be reported
    at end of file. */


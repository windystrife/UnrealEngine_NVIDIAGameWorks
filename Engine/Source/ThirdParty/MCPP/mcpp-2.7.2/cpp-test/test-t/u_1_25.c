/* u_1_25.c:    Miscelaneous undefined macros.  */

/* { dg-do preprocess } */

#define str( a)     # a
#define sub( x, y)      (x - y)
#define SUB             sub

/* u.1.25:  Macro argument otherwise parsed as a directive. */
/*  "#define NAME"; or other undefined behaviour.   */
    str(
#define NAME
/* { dg-error "directives may not be used \[a-z \]*\n\[\^ \]* unterminated| embedding a directive within macro arguments | directive-like line" "" { target *-*-* } 12 } */
    );

#if 0   /* Added by C90: Corrigendum 1 (1994) and deleted by C99    */
/* u.1.26:  Expanded macro replacement list end with name of function-like
        macro.  */
    SUB( a, b);
#endif


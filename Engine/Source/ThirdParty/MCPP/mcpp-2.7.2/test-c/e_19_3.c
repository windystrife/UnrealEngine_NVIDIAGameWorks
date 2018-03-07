/* e_19_3.c:    Redefinitions of macros.    */

#include    "defs.h"
#define     str( s)     # s
#define     xstr( s)    str( s)

/* Excerpts from ISO C 6.8.3 "Examples".    */

#define OBJ_LIKE        (1-1)
#define FTN_LIKE(a)     ( a )

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

main( void)
{
    return  0;
}


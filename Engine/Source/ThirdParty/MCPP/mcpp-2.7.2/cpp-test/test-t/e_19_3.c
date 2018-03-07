/* e_19_3.c:    Redefinitions of macros.    */

/* { dg-do preprocess } */

/* Excerpts from ISO C 3.8.3 "Examples".    */

#define OBJ_LIKE        (1-1)
#define FTN_LIKE(a)     ( a )

/* The following redefinitions should be diagnosed. */

/* 19.3:    */
/* different token sequence     */
#define OBJ_LIKE        (0)     /* { dg-error "redefined\n\[\^ \]*( error:|) this is the location | The macro is redefined" } */

/*  (1-1);  */
    OBJ_LIKE;

/* 19.4:    */
#undef  OBJ_LIKE
#define OBJ_LIKE        (1-1)
/* different white space        */
#define OBJ_LIKE        (1 - 1) /* { dg-error "redefined\n\[\^ \]*( error:|) this is the location | The macro is redefined" } */

/* 19.5:    */
/* different parameter usage    */
#define FTN_LIKE(b)     ( a )   /* { dg-error "redefined\n\[\^ \]*( error:|) this is the location | The macro is redefined" } */

/*  ( x );  */
    FTN_LIKE(x);

/* 19.6:    */
#undef  FTN_LIKE
#define FTN_LIKE(a)     ( a )
/* different parameter spelling */
#define FTN_LIKE(b)     ( b )   /* { dg-error "redefined\n\[\^ \]*( error:|) this is the location | The macro is redefined" } */

/* 19.7:    Not in ISO C "Examples" */
#define FTN_LIKE        OBJ_LIKE    /* { dg-error "redefined\n\[\^ \]*( error:|) this is the location | The macro is redefined" } */


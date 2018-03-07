/* u_1_19.c:    Undefined behaviors on undefined #define and #undef syntax. */

/* { dg-do preprocess } */

/* u.1.19:  A macro expanded to "defined" in #if expression.    */
#define DEFINED     defined
#if     DEFINED DEFINED     /* { dg-warning "this use of \"defined\" may not be portable| is expanded to \"defined\"" } */
#endif

#undef  __linux__
#undef  __arm__
#define __linux__   1
#define HAVE_MREMAP defined(__linux__) && !defined(__arm__)
/* Wrong macro definition.
 * This macro should be defined as follows.
 *  #if defined(__linux__) && !defined(__arm__)
 *  #define HAVE_MREMAP 1
 *  #endif
 */
#if HAVE_MREMAP /* { dg-warning "this use of \"defined\" may not be portable| is expanded to \"defined\"" } */
    mremap();
#endif

/* u.1.20:  Undefining __FILE__, __LINE__, __DATE__, __TIME__, __STDC__ or
        "defined" in #undef directive.  */
#undef  __LINE__    /* { dg-error "undefining| shouldn't be undefined" } */

/* u.1.21:  Defining __FILE__, __LINE__, __DATE__, __TIME__, __STDC__ or
        "defined" in #define directive. */
#define __LINE__    1234    /* { dg-error "defining| shouldn't be redefined" } */
#define defined     defined /* { dg-error "cannot be used as a macro name| invalid macro name | shouldn't be defined" } */
#if     defined defined
#   error   I am not a good preprocessor.
#endif


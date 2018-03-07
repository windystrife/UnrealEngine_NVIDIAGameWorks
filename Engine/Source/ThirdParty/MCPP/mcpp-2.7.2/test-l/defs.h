/* defs.h   */

#include    <stdio.h>

/* assert(): Enable one of these three. */
/* Note: This source doesn't use #elif directive to test preprocessor which
        can't recognize the directive.  */
#if     1   /* For the translator which can process <assert.h> properly.    */
#include    <assert.h>
#else
#if     0   /* Not to abort on error.   */
#define     assert( exp)    (exp) ? (void)0 : (void) fprintf( stderr,   \
        "Assertion failed: %s, from line %d of file %s\n",  \
        # exp, __LINE__, __FILE__)
#else
#if     0   /* For the translator which can't process <assert.h> or '#'
                operator properly.  */
#define     assert( exp)    (exp) ? 0 : fputs( "Assertion failed\n", stderr)
#endif
#endif
#endif

#ifdef  void
/*
 *  For the older compilers which can't handle prototype declarations.
 * You must append these lines in stdio.h.
 *      #undef  void
 *      #define void
 */
extern int      strcmp();
extern size_t   strlen();
extern void     exit();
#else
extern int      strcmp( const char *, const char *);
extern size_t   strlen( const char *);
extern void     exit( int);
#endif


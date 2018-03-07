/* u_2.c:   Undefined behaviors on undefined constant expression.   */

/* { dg-do preprocess } */

/* u.2.1:   Undefined escape sequence.  */
#if     '\x'    /* { dg-error "no following hex digits| Undefined escape sequence '\\\\x'\n\[^ \]* warning:" } */
#endif

/* u.2.2:   Illegal bit shift count.    */
/* dg-warning, not dg-error to avoid a problem of GCC 4.3 testsuite */
#if     1 << -1 /* { dg-warning "Illegal shift count" } */
#endif
#if     1 << 64 /* { dg-error "integer overflow | Illegal shift count" } */
#endif


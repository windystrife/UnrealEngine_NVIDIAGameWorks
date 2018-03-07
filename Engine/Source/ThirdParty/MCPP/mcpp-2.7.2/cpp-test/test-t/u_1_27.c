/* u_1_27.c:   Pseudo-directive-line.   */

/* { dg-do preprocess } */

/* u.1.27:  Unknown preprocessing directive (other than #pragma).   */
#ifdefined MACRO    /* { dg-error "invalid preprocessing directive| Unknown #directive" } */
/* The second error.    */
#endif  /* { dg-error "#endif without #if| unbalanced `#endif'| Not in a #if \\(#ifdef\\) section" } */


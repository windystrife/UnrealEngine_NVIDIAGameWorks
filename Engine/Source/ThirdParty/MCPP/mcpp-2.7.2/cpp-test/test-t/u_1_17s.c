/* u_1_17s.c:   Undefined behavior on out-of-range #line number.   */

/* { dg-do preprocess } */

/* u.1.17s: Line number argument of #line directive should be in range of
        [1,32767].  */
#line   32768   /* { dg-error "line number out of range| Line number \"32768\" is out of range of \[^ \]*\n\[\^ \]* warning: Line number" } */

/*
 * Note: DejaGnu sometimes fails to handle one of the dg-error lines of
 *  #line 0 and #line 32768, when those are tested in a single testcase file.
 *  So, we separated this testcase from u_1_17.c.
 */
 

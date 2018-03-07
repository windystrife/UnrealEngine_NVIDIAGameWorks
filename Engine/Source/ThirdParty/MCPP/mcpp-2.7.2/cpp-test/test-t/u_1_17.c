/* u_1_17.c:    Undefined behaviors on out-of-range #line number.   */

/* { dg-do preprocess } */

/* u.1.17:  Line number argument of #line directive should be in range of
        [1,32767].  */
#line   0   /* { dg-error "line number out of range| Line number \"0\" is out of range" }    */
#line   32767   /* valid here   */
/* line 32767   */
    __LINE__;
/* { dg-warning "line number out of range| got beyond range\n\[^ \]* warning: Line number" "" { target *-*-* } 0 }  */
#line   13  /* Restore to correct line number   */

/* u.1.18:  Line number argument of #line directive should be written in
        decimal digits. */
#line   0x1000  /* { dg-error "not a decimal integer| invalid format | isn't a decimal digits sequence" }    */


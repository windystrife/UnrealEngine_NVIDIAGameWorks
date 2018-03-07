/* u_1_14.c:    Undefined behaviors on undefined #line syntax.  */

/* { dg-do preprocess } */

/* u.1.14:  #line directive without an argument of line number. */
#line   "filename"  /* { dg-error "not a positive integer| invalid format | Not a line number" } */

/* u.1.15:  #line directive with the second argument of other than string
        literal.    */
#line   1234    filename    /* { dg-error "not a valid filename| invalid format | Not a file name" } */

/* u.1.16:  Excessive argument in #line directive.  */
#line   2345    "filename"  Junk    /* { dg-error "extra tokens| garbage | Excessive token sequence" } */


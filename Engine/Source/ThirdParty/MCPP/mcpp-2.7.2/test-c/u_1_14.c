/* u_1_14.c:    Undefined behaviors on undefined #line syntax.  */

main( void)
{

/* u.1.14:  #line directive without an argument of line number. */
#line   "filename"

/* u.1.15:  #line directive with the second argument of other than string
    literal.    */
#line   1234    filename

/* u.1.16:  Excessive argument in #line directive.  */
#line   2345    "filename"  Junk

    return  0;
}


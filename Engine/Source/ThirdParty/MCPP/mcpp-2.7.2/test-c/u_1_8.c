/* u_1_8.c:     Undefined behaviors on unterminated quotations. */

/* u.1.8:   Unterminated character constant.    */
/*  The following "comment" may not interpreted as a comment but swallowed by
        the unterminated character constant.    */
#error  I can't understand. /* Token error prior to execution of #error.    */

main( void)
{
/* u.1.9:   Unterminated string literal.    */
    char *  string =
    "String literal
    across the lines.
"
;
    return  0;
}

/* u.1.10:  Unterminated header-name.   */
#include    <assert.h


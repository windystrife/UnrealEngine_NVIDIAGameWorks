/* u_1_12.c:    Undefined behaviors on undefined #include syntax or header-
        name.   */

/* { dg-do preprocess } */

/* u.1.12:  Argument of #include other than header-name.    */
#include    filename    /* { dg-error "(#include|`#include') expects | Not a header name" } */


/* u_1_13.c:    Undefined behaviors on undefined #include syntax or header-
        name.   */

/* { dg-do preprocess } */

/* u.1.13:  Excessive argument in #include directive.   */
#include    <assert.h>  Junk    /* { dg-error "extra tokens | `#include' expects | Excessive token sequence" } */


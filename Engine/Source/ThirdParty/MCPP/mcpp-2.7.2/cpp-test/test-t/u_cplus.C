/* u_cplus.C:   C++: #undef or #define __cplusplus causes undefined
        behavior.   */

/* { dg-do preprocess } */
/* { dg-options "-std=c++98 -pedantic-errors" } */

#undef  __cplusplus     /* { dg-error "undefining | shouldn't be undefined" } */


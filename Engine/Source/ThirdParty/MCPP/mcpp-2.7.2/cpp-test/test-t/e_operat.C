/* e_operat.C   */
/*
 * In C++98 the 11 identifier-like tokens are operators, not identifiers.
 * Note: in C95 these are defined as macros by <iso646.h>.
 */

/* { dg-do preprocess } */
/* { dg-options "-std=c++98 -pedantic-errors" } */

/* Cannot define operator as a macro.   */
#define and     &&  /* { dg-error "cannot be used as a macro name| is defined as macro" } */
#define xor_eq  ^=  /* { dg-error "cannot be used as a macro name| is defined as macro" } */


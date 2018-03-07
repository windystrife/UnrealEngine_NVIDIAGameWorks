/* e_operat.t   */
/*
 * In C++98 the 11 identifier-like tokens are operators, not identifiers.
 * Note: in C95 these are defined as macros by <iso646.h>.
 */

/* Cannot define operator as a macro.   */
#define and     &&
#define xor_eq  ^=


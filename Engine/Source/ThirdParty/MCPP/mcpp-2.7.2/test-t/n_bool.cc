/* bool.t   */
/*
 * On C++:  'true' and 'false' are evaluated 1 and 0 respectively.
 *      and logical AND, logical OR are evaluated boolean.
 */

/*  Valid block;    */

#define MACRO   1
#define MACRO3  3

#if MACRO == true
    Valid block;
#else
    non-Valid block;
#endif

#if (MACRO && MACRO3) == true
    Valid block;
#else
    non-Valid block;
#endif


/* n_13_5.c:    Arithmetic conversion in #if expressions.   */

/* 13.5:    The usual arithmetic conversion is not performed on bit shift.  */
#if     -1 << 3U > 0
#error  Bad conversion of bit shift operands.
#endif

/* 13.6:    Usual arithmetic conversions.   */
#if     -1 <= 0U        /* -1 is converted to unsigned long.    */
#error  Bad arithmetic conversion.
#endif

#if     -1 * 1U <= 0
#error  Bad arithmetic conversion.
#endif

/* Second and third operands of conditional operator are converted to the
#error      same type, thus -1 is converted to unsigned long.    */
#if     (1 ? -1 : 0U) <= 0
#error  Bad arithmetic conversion.
#endif

/* { dg-do preprocess }
   { dg-options "-ansi -w" }
 */

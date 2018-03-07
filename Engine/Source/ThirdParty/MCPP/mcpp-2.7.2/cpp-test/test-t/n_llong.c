/* n_llong.c:   long long in #if expression */

#if 12345678901234567890U < 13345678901234567890U
    "long long #if expression is implemented."
#else
    "long long #if expression is not implemented."
#endif

#if 12345678901234567890ULL < 13345678901234567890ULL
    Valid block
#else
    Block to be skipped
#endif

#if (0x7FFFFFFFFFFFFFFFULL - 0x6FFFFFFFFFFFFFFFULL) >> 60 == 1
    Valid block
#else
    Block to be skipped
#endif

/* { dg-do preprocess }
   { dg-options "-std=c99" }
 */


/* n_32.c:  Escape sequence in character constant in #if expression.    */

/* 32.1:    Character octal escape sequence.    */
#if     '\123' != 83
#error  Bad evaluation of octal escape sequence.
#endif

/* 32.2:    Character hexadecimal escape sequence.  */
#if     '\x1b' != '\033'
#error  Bad evaluation of hexadecimal escape sequence.
#endif

/* { dg-do preprocess } */


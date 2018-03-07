/* i_35.c:  Multi-character character constant. */

/* In ASCII character set.  */
/* 35.1:    */
#if     ('ab' != '\x61\x62') || ('\aa' != '\7\x61')
#error  Bad handling of multi-character character constant.
#endif

/* { dg-do preprocess }
   { dg-options -w }    */


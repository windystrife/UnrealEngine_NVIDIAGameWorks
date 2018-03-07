/* n_28.c:  __FILE__, __LINE__, __DATE__, __TIME__, __STDC__ and
            __STDC_VERSION__ are predefined.    */

/* 28.1:    */
/*  "n_28.c"; or "path/n_28.c"  */
    __FILE__;

/* 28.2:    */
/*  10; */
    __LINE__;

/* 28.3:    */
/*  "Aug  1 2001";  */
    __DATE__;

/* 28.4:    */
/*  "21:42:22"; */
    __TIME__;

/* 28.5:    */
/*  1;  */
    __STDC__;

/* 28.6:    */
/*  199409L;    */
/* In C99, the value of this macro is 199901L   */
    __STDC_VERSION__;

/* 28.7:    __LINE__, __FILE__ in an included file. */
/*  3; "line.h"; or "path/line.h";  */
#include    "line.h"

/* { dg-do preprocess }
   { dg-options "-std=iso9899:199409" }
   { dg-final { if ![file exist n_28.i] { return }                      } }
   { dg-final { if \{ [grep n_28.i "n_28\.c"] != ""             \} \{   } }
   { dg-final { if \{ [grep n_28.i "^\[ \]*10\[ \]*;"] != ""    \} \{   } }
   { dg-final { if \{ [grep n_28.i "\"\[ADFJMNOS\]\[aceopu\]\[bcglnprtvy\] \[ 123\]\[0-9\] 20\[0-9\]\[0-9\]\""] != ""   \} \{   } }
   { dg-final { if \{ [grep n_28.i "\"\[0-2\]\[0-9\]:\[0-5\]\[0-9\]:\[0-6\]\[0-9\]\""] != ""    \} \{   } }
   { dg-final { if \{ [grep n_28.i "^\[ \]*1\[ \]*;"] != ""     \} \{   } }
   { dg-final { if \{ [grep n_28.i "199409L"] != ""             \} \{   } }
   { dg-final { if \{ [grep n_28.i "^\[ \]*3\[ \]*;"] != ""     \} \{   } }
   { dg-final { if \{ [grep n_28.i "line\.h"] != ""             \} \{   } }
   { dg-final { return \} \} \} \} \} \} \} \}                          } }
   { dg-final { fail "n_28.c: standard predefined macros"               } }
 */


/* n_6.c:   #include directive. */

/* 6.1: Header-name quoted by " and " as well as by < and > can include
        standard headers.   */
/* Note: Standard headers can be included any times.    */
#include    "ctype.h"
#include    <ctype.h>

/* 6.2: Macro is allowed in #include line.  */
#define HEADER  "header.h"
/* Before file inclusion:   #include "header.h" */
#include    HEADER
/*  xyz */
    MACRO_xyz

/* 6.3: With macro nonsence but legal.  */
#undef  MACRO_zyx
#define ZERO_TOKEN
#include    ZERO_TOKEN HEADER ZERO_TOKEN
/*  zyx */
    MACRO_zyx

/* { dg-do preprocess }
   { dg-options "-std=c89 -w" }
   { dg-final { if ![file exist n_6.i] { return }                       } }
   { dg-final { if \{ [grep n_6.i "xyz"] != ""                  \} \{   } }
   { dg-final { if \{ [grep n_6.i "zyx"] != ""                  \} \{   } }
   { dg-final { return \} \}                                            } }
   { dg-final { fail "n_6.c: #includes"                                 } }
 */


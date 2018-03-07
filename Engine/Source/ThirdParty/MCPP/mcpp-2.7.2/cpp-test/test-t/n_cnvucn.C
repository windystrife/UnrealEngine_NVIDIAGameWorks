/* n_cnvucn.C:  C++ Conversion from multi-byte character to UCN.    */
/* This conversion takes place in the translation phase 1.  */

#define str( a)     # a

    /* "\"\u6F22\u5B57\"" or "\"\\u6F22\\u5B57\""   */
    str( "´Á»ú")

/* { dg-do preprocess }
   { dg-options "-std=c++98 -finput-charset=EUC-JP" }
   { dg-final { if ![file exist n_cnvucn.i] { return }                  } }
   { dg-final { if \{ [grep n_cnvucn.i "\"\\\"\\\\u6F22\\\\u5B57\\\"\"|\"\\\"\\\\\\\\u6F22\\\\\\\\u5B57\\\"\""] != ""   \} \{   } }
   { dg-final { return \}                                               } }
   { dg-final { fail "n_cnvucn.C: UCN conversion"                       } }
 */


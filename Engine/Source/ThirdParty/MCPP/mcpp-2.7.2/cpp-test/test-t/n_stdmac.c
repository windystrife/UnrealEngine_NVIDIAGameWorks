/* n_stdmac.c:  C99 Standard pre-defined macros.    */

/*  199901L     */
    __STDC_VERSION__

/*  1  or 0     */
    __STDC_HOSTED__

/* { dg-do preprocess }
   { dg-options "-std=c99" }
   { dg-final { if ![file exist n_stdmac.i] { return }                  } }
   { dg-final { if \{ [grep n_stdmac.i "199901L"] != ""         \} \{   } }
   { dg-final { if \{ [grep n_stdmac.i "1|0"] != ""             \} \{   } }
   { dg-final { if \{ [grep n_stdmac.i "__STDC_HOSTED__"] == "" \} \{   } }
   { dg-final { return \} \} \}                                         } }
   { dg-final { fail "n_stdmac.c: standard pre-defined macro in C99"    } }
 */


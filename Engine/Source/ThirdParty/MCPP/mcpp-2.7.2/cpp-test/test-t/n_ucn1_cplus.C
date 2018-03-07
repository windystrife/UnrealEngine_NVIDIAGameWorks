/* n_ucn1_cplus.C:  Universal-character-name    */ 

/* UCN in character constant    */

#if '\u5B57'
    UCN-16bits is implemented
#endif

#if '\U00006F22'
    UCN-32bits is implemented
#endif

/* UCN in string literal    */

    "abc\u6F22\u5B57xyz";   /* i.e. "abc´Á»úxyx";   */

/* UCN in identifier    */

#define macro\u5B57         9
#define macro\U00006F22     99

    macro\u5B57         /* 9    */
    macro\U00006F22 = macro\U00006f22   /* 99 = 99  */
        /* '6F22' and '6f22' have the same value    */

/* { dg-do preprocess }
   { dg-options "-std=c++98 -w" }
   { dg-final { if ![file exist n_ucn1_cplus.i] { return }              } }
   { dg-final { if \{ [grep n_ucn1_cplus.i "UCN-16bits is implemented"] != ""   \} \{   } }
   { dg-final { if \{ [grep n_ucn1_cplus.i "UCN-32bits is implemented"] != ""   \} \{   } }
   { dg-final { if \{ [grep n_ucn1_cplus.i "\"abc\\\\u6\[Ff\]22\\\\u5\[Bb\]57xyz\""] != ""  \} \{   } }
   { dg-final { if \{ [grep n_ucn1_cplus.i "9"] != ""           \} \{   } }
   { dg-final { if \{ [grep n_ucn1_cplus.i "99 *= *99"] != ""   \} \{   } }
   { dg-final { return \} \} \} \} \}                                   } }
   { dg-final { fail "n_ucn1_cplus.C: UCN in tokens"                    } }
 */


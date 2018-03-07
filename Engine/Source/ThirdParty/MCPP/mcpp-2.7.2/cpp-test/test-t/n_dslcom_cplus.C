/* n_dslcom_cplus.C:    // is a comment of C++ and C99. */
/*  a;  */ 
    a;  // is a comment of C++ and C99

/* { dg-do preprocess }
   { dg-options "-std=c++98" }
   { dg-final { if ![file exist n_dslcom_cplus.i] { return }            } }
   { dg-final { if \{ [grep n_dslcom_cplus.i "^\[ \]*a;\[ \]*$"] != ""  \} \{   } }
   { dg-final { return \}                                               } }
   { dg-final { fail "n_dslcom_cplus.C: double slash comment"           } }
 */


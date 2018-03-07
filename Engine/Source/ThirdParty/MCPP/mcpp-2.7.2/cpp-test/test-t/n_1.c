/* n_1.c:   Conversion of trigraph sequences.   */

/* 1.1: The following 9 sequences are valid trigraph sequences. */
/*  [ ] \ ^ { } | ~ #;  */
    ??( ??) ??/ ??' ??< ??> ??! ??- ??=;

/* 1.2: In directive line.  */
/*  ab | cd;    */
??= define  OR( a, b)   a ??! b
    OR( ab, cd);

/* { dg-do preprocess }
   { dg-final { if ![file exist n_1.i] { return }                       } }
   { dg-final { if \{ [grep n_1.i "\[ \] \\ \^ \{ \} \| ~ #"] != "" \} \{ } }
   { dg-final { if \{ [grep n_1.i "ab *\| *cd"] != ""           \} \{   } }
   { dg-final { return \} \} \}                                         } }
   { dg-final { fail "n_1.c: trigraphs conversion"                      } }
 */


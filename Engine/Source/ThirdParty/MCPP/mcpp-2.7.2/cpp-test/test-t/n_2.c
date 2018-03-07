/* n_2.c:   Line splicing by <backslash><newline> sequence. */

/* 2.1: In a #define directive line, between the parameter list and the
        replacement text.   */
/*  ab + cd + ef;   */
#define FUNC( a, b, c)  \
        a + b + c
    FUNC( ab, cd, ef);

/* 2.2: In a #define directive line, among the parameter list and among the
        replacement text.   */
/*  gh + ij + kl;   */
#undef  FUNC
#define FUNC( a, b  \
        , c)        \
        a + b       \
        + c
    FUNC( gh, ij, kl);

/* 2.3: In a string literal.    */
/*  "abcde" */
    "abc\
de"

/* 2.4: <backslash><newline> in midst of an identifier. */
/*  abcde   */
    abc\
de

/* 2.5: <backslash><newline> by trigraph.   */
/*  ghijk   */
    ghi??/
jk

/* { dg-do preprocess }
   { dg-final { if ![file exist n_2.i] { return }                       } }
   { dg-final { if \{ [grep n_2.i "ab *\\+ *cd *\\+ *ef"] != "" \} \{   } }
   { dg-final { if \{ [grep n_2.i "gh *\\+ *ij *\\+ *kl"] != "" \} \{   } }
   { dg-final { if \{ [grep n_2.i "\"abcde\""] != ""            \} \{   } }
   { dg-final { if \{ [grep n_2.i "abcde"] != ""                \} \{   } }
   { dg-final { if \{ [grep n_2.i "ghijk"] != ""                \} \{   } }
   { dg-final { return   \} \} \} \} \}                                 } }
   { dg-final { fail "n_2.c: <backslash><newline> deletion"             } }
 */

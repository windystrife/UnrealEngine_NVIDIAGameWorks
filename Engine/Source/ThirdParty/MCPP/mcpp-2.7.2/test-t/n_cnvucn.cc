/* n_cnvucn.t:  C++ Conversion from multi-byte character to UCN.    */
/* This conversion takes place in the translation phase 1.  */

#define str( a)     # a

    /* "\"\u6F22\u5B57\"" or "\"\\u6F22\\u5B57\""   */
    str( "漢字")

/* Multi-byte characters in identifier. */
#define マクロ  漢字
#define 関数様マクロ(引数1, 引数2)  引数1 ## 引数2
/*  漢字;   */
    マクロ;
/*  漢字の名前; */
    関数様マクロ(漢字の, 名前);

/* Multi-byte character in pp-number.   */
#define mkname( a)  a ## 1型
#define mkstr( a)   xmkstr( a)
#define xmkstr( a)  # a
/*  "abc1型"    */
    char *  mkstr( mkname( abc));


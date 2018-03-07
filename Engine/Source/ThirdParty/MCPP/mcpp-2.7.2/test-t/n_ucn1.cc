/* n_ucn1.t:    Universal-character-name    */ 

/* UCN in character constant    */

#if '\u5B57'
    UCN-16bits is implemented.
#endif

#if '\U00006F22'
    UCN-32bits is implemented.
#endif

/* UCN in string literal    */

    "abc\u6F22\u5B57xyz";   /* i.e. "abc´Á»úxyx";   */

/* UCN in identifier    */

#define macro\u5B57         9
#define macro\U00006F22     99

    macro\u5B57         /* 9    */
    macro\U00006F22     /* 99   */


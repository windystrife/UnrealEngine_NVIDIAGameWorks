/* u_1_5.c:     Undefined behaviors on illegal characters.  */

/* { dg-do preprocess } */

/* u.1.5:   Illegal characters (in other than string literal, character
        constant, header-name or comment).  */
#if     1 ||2
/*     0x1e ^ ^ 0x1f    */
/* { dg-error "invalid character| Invalid token| is not valid | Illegal control character 0x1e, \[a-z \]*\n\[\^ \]* error: Illegal" "" { target *-*-* } 7 } */
#endif  /* Maybe the second error.  */

/* u.1.6:   [VT], [FF] in directive line.   */
#if     1 ||2
/*     [VT] ^ ^ [FF]    */
/* { dg-error "vertical tab in preprocessing directive| Converted 0x0b to a space" "" { target *-*-* } 13 } */
/* { dg-error "form feed in preprocessing directive| Converted 0x0c to a space" "" { target *-*-* } 13 } */
#endif  /* Maybe the second error.  */


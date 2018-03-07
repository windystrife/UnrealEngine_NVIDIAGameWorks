/* n_37.t:  Translation limits. */

/* 37.1:    Number of parameters in macro: at least 31. */
#define glue31(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z,A,B,C,D,E)   \
    a##b##c##d##e##f##g##h##i##j##k##l##m##n##o##p##q##r##s##t##u##v##w##x##y##z##A##B##C##D##E

/* 37.2:    Number of arguments of macro call: at least 31. */
/*  ABCDEFGHIJKLMNOPQRSTUVWXYZabcde;    */
    glue31( A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R
            , S, T, U, V, W, X, Y, Z, a, b, c, d, e);

/* 37.3:    Significant initial characters in an internal identifier or a
        macro name: at least 31.  */
/*  ABCDEFGHIJKLMNOPQRSTUVWXYZabcd_;    */
    ABCDEFGHIJKLMNOPQRSTUVWXYZabcd_;

/* 37.4:    Nested conditional inclusion: at least 8 levels.    */
/*  ifdef_nest = 8;   */
#ifdef  A
#else
#   ifdef   B
#   else
#       ifdef   C
#       else
#           ifdef   D
#           else
#               ifdef   E
#               else
#                   ifdef   F
#                   else
#                       ifdef   G
#                       else
#                           ifdef   H
#                           else
                                ifdef_nest = 8;
#                           endif
#                       endif
#                   endif
#               endif
#           endif
#       endif
#   endif
#endif

/* 37.5:    Nested source file inclusion: at least 8 levels.    */
#define X8
/*  nest = 1;  nest = 2;  nest = 3;  nest = 4;
    nest = 5;  nest = 6;  nest = 7;  nest = 8;  */
#include    "nest1.h"

/* 37.6:    Parenthesized expression: at least 32 levels.   */
/*  nest = 32;  */
#if     0 + (1 - (2 + (3 - (4 + (5 - (6 + (7 - (8 + (9 - (10 + (11 - (12 +  \
        (13 - (14 + (15 - (16 + (17 - (18 + (19 - (20 + (21 - (22 + (23 -   \
        (24 + (25 - (26 + (27 - (28 + (29 - (30 + (31 - (32 + 0))))))))))   \
        )))))))))))))))))))))) == 0
    nest = 32;
#endif

/* 37.7:    Characters in a string (after concatenation): at least 509. */
"123456789012345678901234567890123456789012345678901234567890123456789\
0123456789012345678901234567890123456789012345678901234567890123456789\
0123456789012345678901234567890123456789012345678901234567890123456789\
0123456789012345678901234567890123456789012345678901234567890123456789\
0123456789012345678901234567890123456789012345678901234567890123456789\
0123456789012345678901234567890123456789012345678901234567890123456789\
0123456789012345678901234567890123456789012345678901234567890123456789\
012345678901234567"
        ;

/* 37.8:    Characters in a logical source line: at least 509.  */
    int a123456789012345678901234567890 = 123450;   \
    int b123456789012345678901234567890 = 123451;   \
    int c123456789012345678901234567890 = 123452;   \
    int d123456789012345678901234567890 = 123453;   \
    int e123456789012345678901234567890 = 123454;   \
    int f123456789012345678901234567890 = 123455;   \
    int A123456789012345678901234567890 = 123456;   \
    int B123456789012345678901234567890 = 123457;   \
    int C123456789012345678901234567890 = 123458;   \
    int D1234567890123456789012 = 123459;

/* 37.9:    Macro definitions: at least 1024.   */
#define X0400
#include    "m4095.h"
/*  0x0400; */
    BNJ;

/* { dg-do preprocess }
   { dg-options "-ansi -w" }
   { dg-final { if ![file exist n_37.i] { return }                      } }
   { dg-final { if \{ [grep n_37.i "ABCDEFGHIJKLMNOPQRSTUVWXYZabcde"] != "" \} \{   } }
   { dg-final { if \{ [grep n_37.i "ABCDEFGHIJKLMNOPQRSTUVWXYZabcd_"] != "" \} \{   } }
   { dg-final { if \{ [grep n_37.i "ifdef_nest = 8"] != ""      \} \{   } }
   { dg-final { if \{ [grep n_37.i "nest = 8"] != ""            \} \{   } }
   { dg-final { if \{ [grep n_37.i "nest = 32"] != ""           \} \{   } }
   { dg-final { if \{ [grep n_37.i "0x0400"] != ""              \} \{   } }
   { dg-final { return \} \} \} \} \} \}                                } }
   { dg-final { fail "n_37.c: translation limits"                       } }
 */


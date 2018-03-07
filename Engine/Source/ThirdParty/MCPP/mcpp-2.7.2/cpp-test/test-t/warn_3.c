/* warn_3.c */

/* { dg-do preprocess } */
/* { dg-options "-ansi -pedantic -Wall" }   */
/* { dg-excess-errors "" }  */

/*
 *   The following texts are legal but non-portable ones, since these requires
 * translation limits greater than the minima quaranteed by C90.  Good
 * preprocessor will warn at these texts (at least when user wants), unless
 * it diagnose these as errors.
 */

/* w.3.1:   Number of parameters in macro: more than 31.    */
#define glue63(    \
    a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, \
    a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, \
    a2, b2, c2, d2, e2, f2, g2, h2, i2, j2, k2, l2, m2, n2, o2, p2, \
    a3, b3, c3, d3, e3, f3, g3, h3, i3, j3, k3, l3, m3, n3, o3)     \
    a0 ## b0 ## c0 ## d0 ## e0 ## f0 ## g0 ## h0 ## \
    o0 ## o1 ## o2 ## o3 ## p0 ## p1 ## p2
 
/* w.3.2:   Number of arguments in macro call: more than 31.    */
/*  A0B0C0D0E0F0G0H0O0O1O2O3P0P1P2; */
    glue63(
    A0, B0, C0, D0, E0, F0, G0, H0, I0, J0, K0, L0, M0, N0, O0, P0,
    A1, B1, C1, D1, E1, F1, G1, H1, I1, J1, K1, L1, M1, N1, O1, P1,
    A2, B2, C2, D2, E2, F2, G2, H2, I2, J2, K2, L2, M2, N2, O2, P2,
    A3, B3, C3, D3, E3, F3, G3, H3, I3, J3, K3, L3, M3, N3, O3);
/* { dg-warning "More than 31 (parameters|arguments)" "" { target *-*-* } 0 }   */

/* w.3.3:   Initial significant characters in an identifier: more than 31.  */
    int
A23456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef = 63;
/* { dg-warning "Identifier longer than 31 characters" "" { target *-*-* } 34 }  */


/* w.3.4:   Nested conditional inclusion: more than 8 levels.   */
#define X0F
/*  nest = 0x0f;    */
#include    "ifdef15.h"
/* { dg-warning "More than 8 nesting of #if" "" { target *-*-* } 0 }    */

/* w.3.5:   Nested source file inclusion: more than 8 levels.   */
#define X0F
/*  nest = 0x0f;    */
#include    "nest1.h"
/* { dg-warning "More than 8 nesting of #include" "" { target *-*-* } 0 }   */

/* w.3.6:   Parenthesized expression: more than 32 levels.  */
/*  nest = 63;  */
#if \
        (0x00 + (0x01 - (0x02 + (0x03 - (0x04 + (0x05 - (0x06 + (0x07 - \
        (0x08 + (0x09 - (0x0A + (0x0B - (0x0C + (0x0D - (0x0E + (0x0F - \
        (0x10 + (0x11 - (0x12 + (0x13 - (0x14 + (0x15 - (0x16 + (0x17 - \
        (0x18 + (0x19 - (0x1A + (0x1B - (0x1C + (0x1D - (0x1E + (0x1F - \
        (0x20 + (0x21 - (0x22 + (0x23 - (0x24 + (0x25 - (0x26 + (0x27 - \
        (0x28 + (0x29 - (0x2A + (0x2B - (0x2C + (0x2D - (0x2E + (0x2F - \
        (0x30 + (0x31 - (0x32 + (0x33 - (0x34 + (0x35 - (0x36 + (0x37 - \
        (0x38 + (0x39 - (0x3A + (0x3B - (0x3C + (0x3D - 0x3E)           \
        )))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))   \
        == -1
    nest = 63;
#endif
/* { dg-warning "More than 32 nesting of parens" "" { target *-*-* } 0  }   */

/* w.3.7:   Characters in a string (after concatenation): more than 509.    */
    char    *string1023 =
"123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
1123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
2123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
3123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
4123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
5123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
6123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
7123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
8123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
9123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
a123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
b123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
c123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
d123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
e123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\
f123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
;
/* { dg-warning "string length `\[0-9\]*' is greater than the length `509'| Quotation longer than 509 bytes" "" { target *-*-* } 0 } */

/* w.3.8:   Characters in a logical source line: more than 509. */
    int a123456789012345678901234567890 = 123450;   \
    int b123456789012345678901234567890 = 123451;   \
    int c123456789012345678901234567890 = 123452;   \
    int d123456789012345678901234567890 = 123453;   \
    int e123456789012345678901234567890 = 123454;   \
    int f123456789012345678901234567890 = 123455;   \
    int g123456789012345678901234567890 = 123456;   \
    int h123456789012345678901234567890 = 123457;   \
    int i123456789012345678901234567890 = 123458;   \
    int j123456789012345678901234567890 = 123459;   \
    int k123456789012345678901234567890 = 123460;   \
    int l123456789012345678901234567890 = 123461;   \
    int m123456789012345678901234567890 = 123462;   \
    int n123456789012345678901234567890 = 123463;   \
    int o123456789012345678901234567890 = 123464;   \
    int p123456789012345678901234567890 = 123465;   \
    int q123456789012345678901234567890 = 123466;   \
    int r123456789012345678901234567890 = 123467;   \
    int s123456789012345678901234567890 = 123468;   \
    int t1234567890123456 = 123469;
/* { dg-warning "Logical source line longer than 509 bytes" "" { target *-*-* } 0 } */

/* w.3.9:   Macro definitions: more than 1024 (including predefined ones).  */
#define X0400
#include    "m4095.h"
/*  0x0400; */
    BNJ;
/* { dg-warning "More than 1024 macros defined" "" { target *-*-* } 0 } */


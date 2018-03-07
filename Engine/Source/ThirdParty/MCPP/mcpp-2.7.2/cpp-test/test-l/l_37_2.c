/* l_37_2.c:    Translation limits larger than C99 / 2. */

/* 37.2L:   Number of arguments in macro call.  */
#include    "l_37_1.h"

/*  A0B0C0D0E0F0G0H0P0P1P2P3P4P5P6P7P8P9PAPBPCPDPEOF;   */
    glue255(
    A0, B0, C0, D0, E0, F0, G0, H0, I0, J0, K0, L0, M0, N0, O0, P0,
    A1, B1, C1, D1, E1, F1, G1, H1, I1, J1, K1, L1, M1, N1, O1, P1,
    A2, B2, C2, D2, E2, F2, G2, H2, I2, J2, K2, L2, M2, N2, O2, P2,
    A3, B3, C3, D3, E3, F3, G3, H3, I3, J3, K3, L3, M3, N3, O3, P3,
    A4, B4, C4, D4, E4, F4, G4, H4, I4, J4, K4, L4, M4, N4, O4, P4,
    A5, B5, C5, D5, E5, F5, G5, H5, I5, J5, K5, L5, M5, N5, O5, P5,
    A6, B6, C6, D6, E6, F6, G6, H6, I6, J6, K6, L6, M6, N6, O6, P6,
    A7, B7, C7, D7, E7, F7, G7, H7, I7, J7, K7, L7, M7, N7, O7, P7,
    A8, B8, C8, D8, E8, F8, G8, H8, I8, J8, K8, L8, M8, N8, O8, P8,
    A9, B9, C9, D9, E9, F9, G9, H9, I9, J9, K9, L9, M9, N9, O9, P9,
    AA, BA, CA, DA, EA, FA, GA, HA, IA, JA, KA, LA, MA, NA, OA, PA,
    AB, BB, CB, DB, EB, FB, GB, HB, IB, JB, KB, LB, MB, NB, OB, PB,
    AC, BC, CC, DC, EC, FC, GC, HC, IC, JC, KC, LC, MC, NC, OC, PC,
    AD, BD, CD, DD, ED, FD, GD, HD, ID, JD, KD, LD, MD, ND, OD, PD,
    AE, BE, CE, DE, EE, FE, GE, HE, IE, JE, KE, LE, ME, NE, OE, PE,
    AF, BF, CF, DF, EF, FF, GF, HF, IF, JF, KF, LF, MF, NF, OF);

/* { dg-do preprocess }
   { dg-options "-std=c99 -pedantic" }
   { dg-warning "| More than 127 (parameters|arguments)" "translation limit" { target *-*-* } 0 }
   { dg-final { if ![file exist l_37_2.i] { return }        } }
   { dg-final { if \{ [grep l_37_2.i "A0B0C0D0E0F0G0H0P0P1P2P3P4P5P6P7P8P9PAPBPCPDPEOF"] != "" \} \{ } }
   { dg-final { return \}                                   } }
   { dg-final { fail "more than 127 parameters"             } }
 */


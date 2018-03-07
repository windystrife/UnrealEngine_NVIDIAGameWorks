/* u_1_11.c:    Undefined behaviors on undefined #include syntax or header-
        name.   */

/* { dg-do preprocess } */

/* u.1.11:  Header-name containing ', ", \ or / followed by *.  */
/*  Probably illegal filename and fails to open.    */
#include    "../*line.h"    /* { dg-error "No such file or directory| Can't open include file" } */
/*  \ is a legal path-delimiter in DOS/Windows or some other OS's.  */
#include    "..\test-t\line.h"  /* { dg-error "No such file or directory| Can't open include file" }    */


/* u_1_1.c:     Undefined behaviors on unterminated line, comment or macro. */

/* { dg-do preprocess } */

/* u.1.1:   End of a source file without <newline>. */
#include    "unbal3.h"
/* { dg-error "u_1_1\.c:6:\n\[^ \]*( error:|) (no newline at end of file|file does not end in newline)| End of file with no newline" "" { target *-*-* } 0 } */
int e_1;

/* u.1.2:   End of a source file with <backslash><newline>. */
#include    "unbal4.h"
/* { dg-error "u_1_1\.c:11:\n\[^ \]*( error:|) backslash-newline at end of file| End of file with \\\\" "" { target *-*-* } 0 } */
;

/* u.1.3:   End of a source file with an unterminated comment.  */
#include    "unbal5.h"
/* { dg-error "u_1_1\.c:16:\n\[^ \]*( error:|) unterminated comment| End of file with unterminated comment" "" { target *-*-* } 0 } */
*/

/* u.1.4:   End of a source file with an uncompleted macro call.    */
#include    "unbal6.h"
/* { dg-error "u_1_1\.c:21:\n\[^ \]*( error:|) unterminated (argument list|macro call)| End of file within macro call" "" { target *-*-* } 0 } */
    y);


/* u_1_1.c:     Undefined behaviors on unterminated line, comment or macro. */

main( void)
{

/* u.1.1:   End of a source file without <newline>. */
#include    "unbal3.h"
int e_1;

/* u.1.2:   End of a source file with <backslash><newline>. */
#include    "unbal4.h"
;

/* u.1.3:   End of a source file with an unterminated comment.  */
#include    "unbal5.h"
*/

/* u.1.4:   End of a source file with an uncompleted macro call.    */
#include    "unbal6.h"
    y);

    return  0;
}


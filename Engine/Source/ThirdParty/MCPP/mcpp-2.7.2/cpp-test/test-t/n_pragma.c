/* n_pragma.c:  _Pragma() operator  */
/* based on the proposal to C99 by Bill Homer   */

#define Machine_B

#include "pragmas.h"

/* #pragma vfunction    */
Fast_call
void f(int n, double * a, double * b) {
/* #pragma ivdep    */
    Independent
    while(n-- > 0) {
        *a++ += *b++;
    }
}

#define f(N, A, B)  \
{   int n = (N), double * a = (A), double * b = (B); \
    Independent while(n-- > 0) { *a++ += *b++; } \
}

#define EXPRAG(x) _Pragma(#x)
#define PRAGMA(x) EXPRAG(x)
#define LIBFUNC xyz

int libfunc() { ... }

/* Direct the linker to define alternate entry points for libfunc. */
/* #pragma duplicate libfunc as (lib_func,xyz)  */
PRAGMA( duplicate libfunc as (lib_func,LIBFUNC) )

/* { dg-do preprocess }
   { dg-options "-std=c99" }
   { dg-final { if ![file exist n_pragma.i] { return }                  } }
   { dg-final { if \{ [grep n_pragma.i "#pragma ivdep"] != ""   \} \{   } }
   { dg-final { if \{ [grep n_pragma.i "#pragma vfunction"] != ""   \} \{   } }
   { dg-final { if \{ [grep n_pragma.i "#pragma duplicate libfunc as \\(lib_func,xyz\\)"] != "" \} \{   } }
   { dg-final { return \} \} \}                                         } }
   { dg-final { fail "n_pragma.c: _Pragma operator"                     } }
 */


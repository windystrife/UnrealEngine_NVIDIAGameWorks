/* n_1_3_run.c: Trigraph-like sequences.    */

#include    <string.h>
#include    <assert.h>

int main( void)
{
/* 1.3: Any sequence other than the specified 9 sequences is not a trigraph.*/
    char    quasi_trigraph[] = { '?', '?', ' ', '?', '?', '?', ' ', '?',
                '?', '%', ' ', '?', '?', '^', ' ', '?', '#', '\0' };

    assert( strcmp( "?? ??? ??% ??^ ???=", quasi_trigraph) == 0);
    return  0;
}

/* { dg-do run }
 * { dg-options "-ansi -no-integrated-cpp" }
 */

/*
 * Note: It is troublesome and unreadable to write preprocessor output of
 *  many '?' sequence for DajaGnu.  So, we test this testcase by strcmp().
 * Note: -no-integrated-cpp is necessary for MCPP on GCC 3.  GCC 2, however,
 *  does not recognize this option.  Remove this option on GCC 2.
 */


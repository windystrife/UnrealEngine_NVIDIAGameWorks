/* n_24_3_run.c:    # operator in macro definition. */

#include    <string.h>
#include    <stdlib.h>
#include    <assert.h>

#define str( a)     # a

int main( void)
{
/* 24.3:    \ is inserted before \ and " in or surrounding literals and no
        other character is inserted to anywhere.    */
    assert( strcmp( str( '"' + "' \""), "'\"' + \"' \\\"\"") == 0);

/* 24.4:    Line splicing by <backslash><newline> is done prior to token
        parsing.   */
    assert( strcmp( str( "ab\
c"), "\"abc\"") == 0);

    return  0;
}

/* { dg-do run }
 * { dg-options "-ansi -no-integrated-cpp -w" }
 */

/*
 * Note: DejaGnu cannot handle the preprocessor output which contains a
 *  doubly stringized string literal.
 *  So, we test this testcase with strcmp().
 * Note: -no-integrated-cpp is necessary for MCPP on GCC 3.  GCC 2, however,
 *  does not recognize this option.  Remove this option on GCC 2.
 */

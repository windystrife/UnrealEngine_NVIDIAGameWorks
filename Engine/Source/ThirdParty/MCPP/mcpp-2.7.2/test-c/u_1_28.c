/* u_1_28.c:    Macro expanding to name identical to directive. */

#define D   define
/* u.1.28:  There are following two ways of preprocessing.
    1:  "D" isn't expanded, because # is the first token of the line.
        Preprocessor reports that "D" is an unknown directive.
    2:  "D" is expanded, because that is not a directive.
        Compiler-phase will diagnose the output of preprocess.
    Anyway, preprocessor should not interprete this line as a preprocessing
    directive.  */
#D  A   B

main( void)
{
    return  0;
}


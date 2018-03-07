/* n_vargs.c:   Macro of variable arguments */
/* from C99 Standard 6.10.3 Examples    */
    #define debug( ...) fprintf( stderr, __VA_ARGS__)
    #define showlist( ...)  puts( #__VA_ARGS__)
    #define report( test, ...)  ((test) ? puts( #test)  \
            : printf( __VA_ARGS__))
    {
        /* fprintf( stderr, "Flag");    */
    debug( "Flag");
        /* fprintf( stderr, "X = %d\n", x);     */
    debug( "X = %d\n", x);
        /* puts( "The first, second, and third items.");   */
    showlist( The first, second, and third items.);
        /* ((x>y) ? puts( "x>y") : printf( "x is %d but y is %d", x, y));   */
    report( x>y, "x is %d but y is %d", x, y);
    }

/* { dg-do preprocess }
   { dg-options "-std=c99" }
   { dg-final { if ![file exist n_vargs.i] { return }                   } }
   { dg-final { if \{ [grep n_vargs.i "fprintf\\( stderr, *\"Flag\" *\\)"] != ""    \} \{   } }
   { dg-final { if \{ [grep n_vargs.i "fprintf\\( stderr, *\"X = %d\\\\n\", x *\\)"] != ""  \} \{   } }
   { dg-final { if \{ [grep n_vargs.i "puts\\( \"The first, second, and third items\.\" *\\)"] != ""  \} \{   } }
   { dg-final { if \{ [grep n_vargs.i "\\(\\( *x>y *\\) \[?\] puts\\( \"x>y\" *\\) : printf\\( *\"x is %d but y is %d\", x, y *\\)\\)"] != "" \} \{   } }
   { dg-final { return \} \} \} \}                                      } }
   { dg-final { fail "n_vargs.c: macro of variable arguments"           } }
 */


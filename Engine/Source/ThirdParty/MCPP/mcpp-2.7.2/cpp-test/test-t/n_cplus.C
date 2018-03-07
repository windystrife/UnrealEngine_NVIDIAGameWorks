/* n_cplus.C:   C++98 pre-defined macro __cplusplus.    */

/*  199711L     */
#if __cplusplus < 199711L
#error  __cplusplus not conforms to C++98
#endif

/* { dg-do preprocess }
   { dg-options "-std=c++98" }
 */


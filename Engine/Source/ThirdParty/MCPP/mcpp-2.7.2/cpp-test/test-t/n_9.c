/* n_9.t:   #pragma directive.  */

/* 9.1: Any #pragma directive should be processed or ignored, should not
    be diagnosed as an error.   */
#pragma __once
#pragma who knows ?

/* { dg-do preprocess } */


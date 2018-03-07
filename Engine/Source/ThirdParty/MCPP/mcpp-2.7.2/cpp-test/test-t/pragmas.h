/* pragmas.h    based on the proposal to C99 by Bill Homer  */

#if defined(Machine_A)
  /* Request fastest calling sequence for machine A */
# define Fast_call \
          _Pragma("fast_call")
#elif defined(Machine_B)
  /* Request fastest calling sequence for machine B */
# define Fast_call \
          _Pragma("vfunction")
#else
# define Fast_call
#endif

#if defined(Machine_B)
  /* Vectorization hint (ignore vector dependencies) */
# define Independent _Pragma("ivdep")
#elif defined(Machine_C)
  /* Parallelization hint (iterations are independent) */
# define Independent _Pragma("independent")
#else
# define Independent
#endif


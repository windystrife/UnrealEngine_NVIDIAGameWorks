/*
    Copyright 2005-2012 Intel Corporation.  All Rights Reserved.

    The source code contained or described herein and all documents related
    to the source code ("Material") are owned by Intel Corporation or its
    suppliers or licensors.  Title to the Material remains with Intel
    Corporation or its suppliers and licensors.  The Material is protected
    by worldwide copyright laws and treaty provisions.  No part of the
    Material may be used, copied, reproduced, modified, published, uploaded,
    posted, transmitted, distributed, or disclosed in any way without
    Intel's prior express written permission.

    No license under any patent, copyright, trade secret or other
    intellectual property right is granted to or conferred upon you by
    disclosure or delivery of the Materials, either expressly, by
    implication, inducement, estoppel or otherwise.  Any license under such
    intellectual property rights must be express and approved by Intel in
    writing.
*/

#include <cstdio>
#include <cstdlib>

#include "tbb/task_scheduler_init.h"
#include "tbb/task.h"
#include "tbb/tick_count.h"

long CutOff = 1;

long SerialFib( const long n );

long ParallelFib( const long n ); 

inline void dump_title() {
    printf("Serial/Parallel, P, N, cutoff, repetitions, time, fib, speedup\n");
}

inline void output(int P, long n, long c, int T, double serial_elapsed, double elapsed, long result) {
    printf("%s, %d, %ld, %ld, %d, %g, %ld, %g\n", ( (P == 0) ? "Serial" : "Parallel" ), P, n, c, T, elapsed, result, serial_elapsed / elapsed);
}

#define MOVE_BY_FOURTHS 1
inline long calculate_new_cutoff(const long lo, const long hi) {
#if MOVE_BY_FOURTHS    
    return lo + (3 + hi - lo ) / 4;
#else
    return (hi + lo)/2;
#endif
}

void find_cutoff(const int P, const long n, const int T, const double serial_elapsed) {
    long lo = 1, hi = n;
    double elapsed = 0, lo_elapsed = 0, hi_elapsed = 0;
    long final_cutoff = -1;

    tbb::task_scheduler_init init(P);

    while(true) {
       CutOff = calculate_new_cutoff(lo, hi);
       long result = 0;
       tbb::tick_count t0;
       for (int t = -1; t < T; ++t) {
           if (t == 0) t0 = tbb::tick_count::now();
           result += ParallelFib(n);
       }
       elapsed = (tbb::tick_count::now() - t0).seconds();
       output(P,n,CutOff,T,serial_elapsed,elapsed,result);

       if (serial_elapsed / elapsed >= P/2.0) {
           final_cutoff = CutOff;
           if (hi == CutOff) {
               if (hi == lo) {
                  // we have had this value at both above and below 50%
                  lo = 1; lo_elapsed = 0;
               } else  {
                  break;
               }
           }
           hi = CutOff;
           hi_elapsed = elapsed;
       } else {
           if (lo == CutOff) break;
           lo = CutOff;
           lo_elapsed = elapsed;
       }
    } 

    double interpolated_cutoff = lo + ( P/2.0 - serial_elapsed/lo_elapsed ) * ( (hi - lo) / ( serial_elapsed/hi_elapsed - serial_elapsed/lo_elapsed ));

    if (final_cutoff != -1) {
        printf("50%% efficiency cutoff is %ld ( linearly interpolated cutoff is %g )\n", final_cutoff, interpolated_cutoff);
    } else {
        printf("Cannot achieve 50%% efficiency\n");
    }

    return;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s threads n repetitions\n",argv[0]); 
        return 1;
    }

    dump_title();

    int P = atoi(argv[1]);
    long n = atol(argv[2]);
    int T = atoi(argv[3]);

    // collect serial time
    long serial_result = 0;
    tbb::tick_count t0; 
    for (int t = -1; t < T; ++t) {
        if (t == 0) t0 = tbb::tick_count::now();        
        serial_result += SerialFib(n);
    }
    double serial_elapsed = (tbb::tick_count::now() - t0).seconds();
    output(0,n,0,T,serial_elapsed,serial_elapsed,serial_result);

    // perform search
    find_cutoff(P,n,T,serial_elapsed);

    return 0;
}


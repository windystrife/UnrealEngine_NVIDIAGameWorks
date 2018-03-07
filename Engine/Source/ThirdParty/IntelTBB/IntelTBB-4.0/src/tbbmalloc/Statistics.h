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

#define MAX_THREADS 1024
#define NUM_OF_BINS 30
#define ThreadCommonCounters NUM_OF_BINS

enum counter_type {
    allocBlockNew = 0,
    allocBlockPublic,
    allocBumpPtrUsed,
    allocFreeListUsed,
    allocPrivatized,
    examineEmptyEnough,
    examineNotEmpty,
    freeRestoreBumpPtr,
    freeByOtherThread,
    freeToActiveBlock,
    freeToInactiveBlock,
    freeBlockPublic,
    freeBlockBack,
    MaxCounters
};
enum common_counter_type {
    allocNewLargeObj = 0,
    allocCachedLargeObj,
    cacheLargeObj,
    freeLargeObj,
    lockPublicFreeList,
    freeToOtherThread
};

#if COLLECT_STATISTICS
/* Statistics reporting callback registred via a static object dtor 
   on Posix or DLL_PROCESS_DETACH on Windows.
 */

static bool reportAllocationStatistics;

struct bin_counters {
    int counter[MaxCounters];
};

static bin_counters statistic[MAX_THREADS][NUM_OF_BINS+1]; //zero-initialized;

static inline int STAT_increment(int thread, int bin, int ctr)
{
    return reportAllocationStatistics && thread < MAX_THREADS ? ++(statistic[thread][bin].counter[ctr]) : 0;
}

static inline void initStatisticsCollection() {
#if defined(MALLOCENV_COLLECT_STATISTICS)
    if (NULL != getenv(MALLOCENV_COLLECT_STATISTICS))
        reportAllocationStatistics = true;
#endif
}

#else
#define STAT_increment(a,b,c) ((void)0)
#endif /* COLLECT_STATISTICS */

#if COLLECT_STATISTICS
static inline void STAT_print(int thread)
{
    if (!reportAllocationStatistics)
        return;

    char filename[100];
#if USE_PTHREAD
    sprintf(filename, "stat_ScalableMalloc_proc%04d_thr%04d.log", getpid(), thread);
#else
    sprintf(filename, "stat_ScalableMalloc_thr%04d.log", thread);
#endif
    FILE* outfile = fopen(filename, "w");
    for(int i=0; i<NUM_OF_BINS; ++i)
    {
        bin_counters& ctrs = statistic[thread][i];
        fprintf(outfile, "Thr%04d Bin%02d", thread, i);
        fprintf(outfile, ": allocNewBlocks %5d", ctrs.counter[allocBlockNew]);
        fprintf(outfile, ", allocPublicBlocks %5d", ctrs.counter[allocBlockPublic]);
        fprintf(outfile, ", restoreBumpPtr %5d", ctrs.counter[freeRestoreBumpPtr]);
        fprintf(outfile, ", privatizeCalled %10d", ctrs.counter[allocPrivatized]);
        fprintf(outfile, ", emptyEnough %10d", ctrs.counter[examineEmptyEnough]);
        fprintf(outfile, ", notEmptyEnough %10d", ctrs.counter[examineNotEmpty]);
        fprintf(outfile, ", freeBlocksPublic %5d", ctrs.counter[freeBlockPublic]);
        fprintf(outfile, ", freeBlocksBack %5d", ctrs.counter[freeBlockBack]);
        fprintf(outfile, "\n");
    }
    for(int i=0; i<NUM_OF_BINS; ++i)
    {
        bin_counters& ctrs = statistic[thread][i];
        fprintf(outfile, "Thr%04d Bin%02d", thread, i);
        fprintf(outfile, ": allocBumpPtr %10d", ctrs.counter[allocBumpPtrUsed]);
        fprintf(outfile, ", allocFreeList %10d", ctrs.counter[allocFreeListUsed]);
        fprintf(outfile, ", freeToActiveBlk %10d", ctrs.counter[freeToActiveBlock]);
        fprintf(outfile, ", freeToInactive  %10d", ctrs.counter[freeToInactiveBlock]);
        fprintf(outfile, ", freedByOther %10d", ctrs.counter[freeByOtherThread]);
        fprintf(outfile, "\n");
    }
    bin_counters& ctrs = statistic[thread][ThreadCommonCounters];
    fprintf(outfile, "Thr%04d common counters", thread);
    fprintf(outfile, ": allocNewLargeObject %5d", ctrs.counter[allocNewLargeObj]);
    fprintf(outfile, ": allocCachedLargeObject %5d", ctrs.counter[allocCachedLargeObj]);
    fprintf(outfile, ", cacheLargeObject %5d", ctrs.counter[cacheLargeObj]);
    fprintf(outfile, ", freeLargeObject %5d", ctrs.counter[freeLargeObj]);
    fprintf(outfile, ", lockPublicFreeList %5d", ctrs.counter[lockPublicFreeList]);
    fprintf(outfile, ", freeToOtherThread %10d", ctrs.counter[freeToOtherThread]);
    fprintf(outfile, "\n");

    fclose(outfile);
}
#endif

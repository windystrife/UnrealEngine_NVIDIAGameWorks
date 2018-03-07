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

#include "tbbmalloc_internal.h"

/********* Allocation of large objects ************/


namespace rml {
namespace internal {

static struct LargeBlockCacheStat {
    uintptr_t age;
} loCacheStat;

#if __TBB_MALLOC_LOCACHE_STAT
intptr_t mallocCalls, cacheHits;
intptr_t memAllocKB, memHitKB;
#endif

bool LargeObjectCache::CacheBin::put(ExtMemoryPool *extMemPool,
                                     LargeMemoryBlock *ptr, int idx)
{
    bool blockCached;
    BinStatus binStatus = NOT_CHANGED;
    size_t size = ptr->unalignedSize;
    ptr->prev = NULL;
    BinBitMask *bitMask = &extMemPool->loc.bitMask;
    uintptr_t currTime = ptr->age =
        extMemPool->loc.cleanupCacheIfNeeded(extMemPool);

    {
        MallocMutex::scoped_lock scoped_cs(lock);

        forgetOutdatedState(currTime);
        usedSize -= size;
        if (!usedSize)
            binStatus = SET_EMPTY;

        if (lastCleanedAge) {
            ptr->next = first;
            first = ptr;
            if (ptr->next) ptr->next->prev = ptr;
            if (last) {
                binStatus = NOT_CHANGED;
            } else {
                MALLOC_ASSERT(0 == oldest, ASSERT_TEXT);
                oldest = ptr->age;
                last = ptr;
                binStatus = SET_NON_EMPTY;
            }
            cachedSize += size;
            blockCached = true;
        } else {
            // 1st object of such size was released.
            // Not cache it, and remeber when this occurs
            // to take into account during cache miss.
            lastCleanedAge = ptr->age;
            blockCached = false;
        }
    }
    // Bitmask is modified out of lock. As bitmask is used only for cleanup,
    // this is not violating correctness.
    if (binStatus != NOT_CHANGED)
        bitMask->set(idx, binStatus == SET_NON_EMPTY);
    return blockCached;
}

LargeMemoryBlock *
    LargeObjectCache::CacheBin::get(ExtMemoryPool *extMemPool, size_t size,
                                    int idx)
{
    BinStatus binStatus = NOT_CHANGED;
    BinBitMask *bitMask = &extMemPool->loc.bitMask;
    uintptr_t currTime = extMemPool->loc.cleanupCacheIfNeeded(extMemPool);
    LargeMemoryBlock *result=NULL;
    {
        MallocMutex::scoped_lock scoped_cs(lock);
        forgetOutdatedState(currTime);

        if (first) {
            result = first;
            first = result->next;
            if (first)
                first->prev = NULL;
            else {
                last = NULL;
                oldest = 0;
            }
            // use moving average with current hit interval
            intptr_t hitR = currTime - result->age;
            lastHit = lastHit? (lastHit + hitR)/2 : hitR;

            cachedSize -= size;
        } else {
            /* If cache miss occured and cache was cleaned,
               set ageThreshold to twice the difference
               between current time and last time cache was cleaned. */
            if (lastCleanedAge)
                ageThreshold = 2*(currTime - lastCleanedAge);
        }
        if (!usedSize) // inform that there are used blocks in the bin
            binStatus = SET_NON_EMPTY;
        // subject to later correction, if got cache miss and later allocation failed
        usedSize += size;
        lastGet = currTime;
    }
    // Bitmask is modified out of lock. As bitmask is used only for cleanup,
    // this is not violating correctness.
    if (binStatus == SET_NON_EMPTY)
        bitMask->set(idx, true);
    return result;
}

// forget the history for the bin if it was unused for long time
void LargeObjectCache::CacheBin::forgetOutdatedState(uintptr_t currTime)
{
    // If time since last get tooLongWait times more ageThreshold for the bin,
    // treat the bin as rarely-used and forget everything we know about it.
    // 16 is balanced between too small value, that forgets too early and
    // so prevents good caching, and too high, that leads to caching blocks
    // with unrelated usage pattern.
    const int tooLongWait = 16;
    const uintptr_t sinceLastGet = currTime - lastGet;
    bool doCleanup = false;

    if (!last) // clean only empty bins
        if (ageThreshold)
            doCleanup = sinceLastGet > tooLongWait*ageThreshold;
        else if (lastCleanedAge)
            doCleanup = sinceLastGet > tooLongWait*(lastCleanedAge - lastGet);
    if (doCleanup) {
        lastCleanedAge = 0;
        ageThreshold = 0;
    }
}

bool LargeObjectCache::CacheBin::cleanToThreshold(ExtMemoryPool *extMemPool,
                                                  uintptr_t currTime, int idx)
{
    LargeMemoryBlock *toRelease = NULL;
    bool released = false;
    BinBitMask *bitMask = &extMemPool->loc.bitMask;

    /* oldest may be more recent then age, that's why cast to signed type
       was used. age overflow is also processed correctly. */
    if (last && (intptr_t)(currTime - oldest) > ageThreshold) {
        MallocMutex::scoped_lock scoped_cs(lock);
        // double check
        if (last && (intptr_t)(currTime - last->age) > ageThreshold) {
            do {
                cachedSize -= last->unalignedSize;
                last = last->prev;
            } while (last && (intptr_t)(currTime - last->age) > ageThreshold);
            if (last) {
                toRelease = last->next;
                oldest = last->age;
                last->next = NULL;
            } else {
                toRelease = first;
                first = NULL;
                oldest = 0;
                if (!usedSize)
                    bitMask->set(idx, false);
            }
            MALLOC_ASSERT( toRelease, ASSERT_TEXT );
            lastCleanedAge = toRelease->age;
        }
        else
            return false;
    }
    released = toRelease;

    while ( toRelease ) {
        LargeMemoryBlock *helper = toRelease->next;
        removeBackRef(toRelease->backRefIdx);
        extMemPool->backend.putLargeBlock(toRelease);
        toRelease = helper;
    }
    return released;
}

bool LargeObjectCache::CacheBin::cleanAll(ExtMemoryPool *extMemPool, BinBitMask *bitMask, int idx)
{
    LargeMemoryBlock *toRelease = NULL;
    bool released = false;

    if (last) {
        MallocMutex::scoped_lock scoped_cs(lock);
        // double check
        if (last) {
            toRelease = first;
            last = NULL;
            first = NULL;
            oldest = 0;
            cachedSize = 0;
            if (!usedSize)
                bitMask->set(idx, false);
        }
        else
            return false;
    }
    released = toRelease;

    while ( toRelease ) {
        LargeMemoryBlock *helper = toRelease->next;
        removeBackRef(toRelease->backRefIdx);
        extMemPool->backend.putLargeBlock(toRelease);
        toRelease = helper;
    }
    return released;
}

size_t LargeObjectCache::CacheBin::reportStat(int num, FILE *f)
{
#if __TBB_MALLOC_LOCACHE_STAT
    if (first)
        printf("%d(%d): total %lu KB thr %ld lastCln %lu lastHit %lu oldest %lu\n",
               num, num*largeBlockCacheStep+minLargeObjectSize,
               cachedSize/1024, ageThreshold, lastCleanedAge, lastHit, oldest);
#endif
    return cachedSize;
}
// release from cache blocks that are older than ageThreshold
bool LargeObjectCache::regularCleanup(ExtMemoryPool *extMemPool, uintptr_t currTime)
{
    bool released = false, doThreshDecr = false;
    BinsSummary binsSummary;

    for (int i = bitMask.getMaxTrue(numLargeBlockBins-1); i >= 0;
         i = bitMask.getMaxTrue(i-1)) {
        bin[i].updateBinsSummary(&binsSummary);
        if (!doThreshDecr && tooLargeLOC>2 && binsSummary.isLOCTooLarge()) {
            // if LOC is too large for quite long time, decrease the threshold
            // based on bin hit statistics.
            // For this, redo cleanup from the beginnig.
            // Note: on this iteration total usedSz can be not too large
            // in comparison to total cachedSz, as we calculated it only
            // partially. We are ok this it.
            i = bitMask.getMaxTrue(numLargeBlockBins-1);
            doThreshDecr = true;
            binsSummary.reset();
            continue;
        }
        if (doThreshDecr)
            bin[i].decreaseThreshold();
        if (bin[i].cleanToThreshold(extMemPool, currTime, i))
            released = true;
    }

    // We want to find if LOC was too large for some time continuously,
    // so OK with races between incrementing and zeroing, but incrementing
    // must be atomic.
    if (binsSummary.isLOCTooLarge())
        AtomicIncrement(tooLargeLOC);
    else
        tooLargeLOC = 0;
    return released;
}

#if __TBB_MALLOC_WHITEBOX_TEST
size_t LargeObjectCache::getLOCSize() const
{
    size_t size = 0;
    for (int i = numLargeBlockBins-1; i >= 0; i--)
        size += bin[i].getSize();
    return size;
}

size_t LargeObjectCache::getUsedSize() const
{
    size_t size = 0;
    for (int i = numLargeBlockBins-1; i >= 0; i--)
        size += bin[i].getUsedSize();
    return size;
}
#endif // __TBB_MALLOC_WHITEBOX_TEST

bool ExtMemoryPool::softCachesCleanup()
{
    // TODO: cleanup small objects as well
    return loc.regularCleanup(this, FencedLoad((intptr_t&)loCacheStat.age));
}

uintptr_t LargeObjectCache::cleanupCacheIfNeeded(ExtMemoryPool *extMemPool)
{
    /* loCacheStat.age overflow is OK, as we only want difference between
     * its current value and some recent.
     *
     * Both malloc and free should increment loCacheStat.age, as in
     * a different case multiple cached blocks would have same age,
     * and accuracy of predictors suffers.
     */
    uintptr_t currTime = (uintptr_t)AtomicIncrement((intptr_t&)loCacheStat.age);

    if ( 0 == currTime % cacheCleanupFreq )
        regularCleanup(extMemPool, currTime);

    return currTime;
}

LargeMemoryBlock *LargeObjectCache::get(ExtMemoryPool *extMemPool, size_t size)
{
    MALLOC_ASSERT( size%largeBlockCacheStep==0, ASSERT_TEXT );
    LargeMemoryBlock *lmb = NULL;
    size_t idx = sizeToIdx(size);
    if (idx<numLargeBlockBins) {
        lmb = bin[idx].get(extMemPool, size, idx);
        if (lmb) {
            MALLOC_ITT_SYNC_ACQUIRED(bin+idx);
            STAT_increment(getThreadId(), ThreadCommonCounters, allocCachedLargeBlk);
        }
    }
    return lmb;
}

void LargeObjectCache::rollbackCacheState(size_t size)
{
    size_t idx = sizeToIdx(size);
    if (idx<numLargeBlockBins)
        bin[idx].decrUsedSize(size, &bitMask, idx);
}

void *ExtMemoryPool::mallocLargeObject(size_t size, size_t alignment)
{
    size_t headersSize = sizeof(LargeMemoryBlock)+sizeof(LargeObjectHdr);
    // TODO: take into account that they are already largeObjectAlignment-aligned
    size_t allocationSize = alignUp(size+headersSize+alignment, largeBlockCacheStep);

    if (allocationSize < size) // allocationSize is wrapped around after alignUp
        return NULL;

#if __TBB_MALLOC_LOCACHE_STAT
    AtomicIncrement(mallocCalls);
    AtomicAdd(memAllocKB, allocationSize/1024);
#endif
    LargeMemoryBlock* lmb = loc.get(this, allocationSize);
    if (!lmb) {
        BackRefIdx backRefIdx = BackRefIdx::newBackRef(/*largeObj=*/true);
        if (backRefIdx.isInvalid())
            return NULL;

        // unalignedSize is set in getLargeBlock
        lmb = backend.getLargeBlock(allocationSize);
        if (!lmb) {
            removeBackRef(backRefIdx);
            loc.rollbackCacheState(allocationSize);
            return NULL;
        }
        lmb->backRefIdx = backRefIdx;
        STAT_increment(getThreadId(), ThreadCommonCounters, allocNewLargeObj);
    } else {
#if __TBB_MALLOC_LOCACHE_STAT
        AtomicIncrement(cacheHits);
        AtomicAdd(memHitKB, allocationSize/1024);
#endif
    }

    void *alignedArea = (void*)alignUp((uintptr_t)lmb+headersSize, alignment);
    LargeObjectHdr *header = (LargeObjectHdr*)alignedArea-1;
    header->memoryBlock = lmb;
    header->backRefIdx = lmb->backRefIdx;
    setBackRef(header->backRefIdx, header);

    lmb->objectSize = size;

    MALLOC_ASSERT( isLargeObject(alignedArea), ASSERT_TEXT );
    return alignedArea;
}

bool LargeObjectCache::put(ExtMemoryPool *extMemPool, LargeMemoryBlock *largeBlock)
{
    size_t idx = sizeToIdx(largeBlock->unalignedSize);
    if (idx<numLargeBlockBins) {
        MALLOC_ITT_SYNC_RELEASING(bin+idx);
        if (bin[idx].put(extMemPool, largeBlock, idx)) {
            STAT_increment(getThreadId(), ThreadCommonCounters, cacheLargeBlk);
            return true;
        } else
            return false;
    }
    return false;
}

#if __TBB_MALLOC_LOCACHE_STAT
void LargeObjectCache::reportStat(FILE *f)
{
    size_t cachedSize = 0;
    for (int i=0; i<numLargeBlockBins; i++)
        cachedSize += bin[i].reportStat(i, f);
    fprintf(f, "total LOC size %lu MB\nnow %lu\n", cachedSize/1024/1024,
            loCacheStat.age);
}
#endif

void ExtMemoryPool::freeLargeObject(void *object)
{
    LargeObjectHdr *header = (LargeObjectHdr*)object - 1;

    // overwrite backRefIdx to simplify double free detection
    header->backRefIdx = BackRefIdx();
    if (!loc.put(this, header->memoryBlock)) {
        removeBackRef(header->memoryBlock->backRefIdx);
        backend.putLargeBlock(header->memoryBlock);
        STAT_increment(getThreadId(), ThreadCommonCounters, freeLargeObj);
    }
}

/*********** End allocation of large objects **********/

} // namespace internal
} // namespace rml


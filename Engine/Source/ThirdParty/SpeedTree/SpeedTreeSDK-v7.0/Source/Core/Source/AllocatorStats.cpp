///////////////////////////////////////////////////////////////////////
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) CONFIDENTIAL AND PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization, Inc. and
//  may not be copied, disclosed, or exploited except in accordance with 
//  the terms of that agreement.
//
//      Copyright (c) 2003-2014 IDV, Inc.
//      All rights reserved in all media.
//
//      IDV, Inc.
//      http://www.idvinc.com


///////////////////////////////////////////////////////////////////////  
//  Preprocessor

#include "Core/Allocator.h"
#include "Core/Core.h"
using namespace SpeedTree;

#ifdef SPEEDTREE_MEMORY_STATS

#ifdef __GNUC__
	#include <stdio.h>
#endif
#include "Core/String.h"
#include "Core/Map.h"
#include "Core/Mutex.h"


///////////////////////////////////////////////////////////////////////  
//  Constants

#define VERBOSE_MEMORY_REPORT_FILENAME "speedtree_sdk_memory_verbose_report.csv"


///////////////////////////////////////////////////////////////////////  
//  Structure SAllocStats

struct SAllocStats
{
    SAllocStats( ) :
        m_siNumAllocates(0),
        m_siAmountAllocated(0),
        m_siNumFrees(0),
        m_siAmountFreed(0)
    {
    }

    bool operator<(const SAllocStats& sRight) const
    {
        return sRight.m_siNumAllocates < m_siNumAllocates;
    }
	bool operator==(const SAllocStats& sRight) const
	{
		return sRight.m_siNumAllocates == m_siNumAllocates;
	}

    SAllocStats& operator+=(const SAllocStats& sRight)
    {
        m_siNumAllocates += sRight.m_siNumAllocates;
        m_siAmountAllocated += sRight.m_siAmountAllocated;
        m_siNumFrees += sRight.m_siNumFrees;
        m_siAmountFreed += sRight.m_siAmountFreed;

        return *this;
    }

    CFixedString	m_strDesc;
    size_t			m_siNumAllocates;
    size_t			m_siAmountAllocated;
    size_t			m_siNumFrees;
    size_t			m_siAmountFreed;
};


///////////////////////////////////////////////////////////////////////  
//  Structure SLeakStats

struct SLeakStats
{
		SLeakStats( ) :
			m_nCount(0),
			m_nAmount(0)
		{
		}
		
		bool			operator<(const SLeakStats& sRight) const
		{
			return m_nAmount > sRight.m_nAmount; // backwards on purpose
		}
		bool			operator==(const SLeakStats& sRight) const
		{
			return m_nAmount == sRight.m_nAmount;
		}

		CFixedString	m_strDesc;
		st_int32		m_nCount;
		st_int32		m_nAmount;
};


///////////////////////////////////////////////////////////////////////  
//  Type definitions

typedef CMap<CFixedString, SAllocStats, CLess<CFixedString>, false> stats_map;
typedef CMap<void*, SLeakStats, CLess<void*>, false> leak_map;


///////////////////////////////////////////////////////////////////////  
//  Variables shared by all CAllocator instances

stats_map g_mStatsMap;
SAllocStats g_sGlobalStats;
leak_map g_mLeakMap;
CMutex g_cMutex;


///////////////////////////////////////////////////////////////////////  
//  CAllocator::TrackAlloc

void CAllocator::TrackAlloc(const char* pDescription, void* pBlock, size_t siAmount)
{
	g_cMutex.Lock( );

	// local statistics
	g_mStatsMap[pDescription].m_siNumAllocates++;
	g_mStatsMap[pDescription].m_siAmountAllocated += siAmount;

	// global statistics
	g_sGlobalStats.m_siNumAllocates++;
	g_sGlobalStats.m_siAmountAllocated += siAmount;

	// leak statistics
	g_mLeakMap[pBlock].m_nCount++;
	g_mLeakMap[pBlock].m_nAmount += st_int32(siAmount);
	g_mLeakMap[pBlock].m_strDesc = pDescription;

	#ifdef SPEEDTREE_MEMORY_STATS_VERBOSE
		// if first allocation, create a new file
		if (g_sGlobalStats.m_siNumAllocates == 1)
		{
			FILE* pFile = fopen(VERBOSE_MEMORY_REPORT_FILENAME, "w");
			if (pFile)
			{
				fprintf(pFile, "Ordinal,Action,Amount (KB),Total (KB),Description\n");
				fclose(pFile);
			}
		}

		// write next row of data
		FILE* pFile = fopen(VERBOSE_MEMORY_REPORT_FILENAME, "a");
		if (pFile)
		{
			fprintf(pFile, "%d,allocation,%.2f,%.2f,\"%s\"\n", 
				g_sGlobalStats.m_siNumAllocates,siAmount / 1024.0f, CHeapSystem::CurrentUse( ) / 1024.0f, pDescription);
			fclose(pFile);
		}
	#endif

	g_cMutex.Unlock( );
}


///////////////////////////////////////////////////////////////////////  
//  CAllocator::TrackFree

void CAllocator::TrackFree(void* pBlock, size_t siAmount)
{
	g_cMutex.Lock( );

	if (pBlock)
	{
		// leak statistics
		leak_map::iterator iFind = g_mLeakMap.find(pBlock);
		CFixedString strDescription;
		if (iFind != g_mLeakMap.end( ))
		{
			strDescription = iFind->second.m_strDesc.c_str( );
			iFind->second.m_nCount--;
			iFind->second.m_nAmount -= st_int8(siAmount);
			if (iFind->second.m_nCount == 0)
				(void) g_mLeakMap.erase(iFind);
		}
		else
			fprintf(stderr, "CAllocator::TrackFree cannot find block %p, size: %d bytes\n", pBlock, st_int8(siAmount));

		// local statistics
		g_mStatsMap[strDescription].m_siNumFrees++;
		g_mStatsMap[strDescription].m_siAmountFreed += siAmount;

		// global statistics
		g_sGlobalStats.m_siNumFrees++;
		g_sGlobalStats.m_siAmountFreed += siAmount;

		#ifdef SPEEDTREE_MEMORY_STATS_VERBOSE
			// write next row of data
			FILE* pFile = fopen(VERBOSE_MEMORY_REPORT_FILENAME, "a");
			if (pFile)
			{
				fprintf(pFile, "%d,deletion,%.2f,%.2f,\"%s\"\n", 
					g_sGlobalStats.m_siNumAllocates,siAmount / 1024.0f, CHeapSystem::CurrentUse( ) / 1024.0f, strDescription.c_str( ));
				fclose(pFile);
			}
		#endif
	}

	g_cMutex.Unlock( );
}


///////////////////////////////////////////////////////////////////////  
//  CAllocator::Report

bool CAllocator::Report(const char* pFilename, bool bFreeTrackingData)
{
	bool bSuccess = false;

	g_cMutex.Lock( );

	CArray<SAllocStats, false> vOrdered(0, SAllocStats( ));
	for (stats_map::const_iterator i = g_mStatsMap.begin( ); i != g_mStatsMap.end( ); ++i)
	{
		vOrdered.push_back(i->second);
		vOrdered.back( ).m_strDesc = i->first;
	}
	vOrdered.sort( );

	FILE* pFile = stdout;
	if (pFilename)
	{
		pFile = fopen(pFilename, "w");
		if (!pFile)
			pFile = stdout;
	}
	if (pFile)
	{
		fprintf(pFile, "\nallocator,alloc/free delta (#),alloc/free delta (KB),# allocs,# frees,alloced (KB),freed (KB),%% of all allocs,%% of all frees\n");

		for (st_int32 i = 0; i < st_int32(vOrdered.size( )); ++i)
		{
			fprintf(pFile, "\"%s\",%d,%0.2f,%d,%d,%0.2f,%0.2f,%0.1f,%0.1f\n",
				vOrdered[i].m_strDesc.c_str( ),													// allocator
				st_int32(vOrdered[i].m_siNumAllocates - vOrdered[i].m_siNumFrees),				// alloc/free delta (#)
				(vOrdered[i].m_siAmountAllocated - vOrdered[i].m_siAmountFreed) / 1024.0f,		// alloc/free delta (KB)
				st_int32(vOrdered[i].m_siNumAllocates),											// # allocs
				st_int32(vOrdered[i].m_siNumFrees),												// # frees
				vOrdered[i].m_siAmountAllocated / 1024.0f,										// alloced (KB)
				vOrdered[i].m_siAmountFreed / 1024.0f,											// freed (KB)
				100.0f * vOrdered[i].m_siNumAllocates / float(g_sGlobalStats.m_siNumAllocates),	// % of all allocs
				100.0f * vOrdered[i].m_siNumFrees / float(g_sGlobalStats.m_siNumFrees));		// % of all frees
		}

		fprintf(pFile, "\nGlobal statistics:\n");
		fprintf(pFile, "\t[%d] total allocations,[%1.f KB] allocated\n", st_int32(g_sGlobalStats.m_siNumAllocates), g_sGlobalStats.m_siAmountAllocated / 1024.0f);
		fprintf(pFile, "\t[%d] total free calls,[%1.f KB] freed\n", st_int32(g_sGlobalStats.m_siNumFrees), g_sGlobalStats.m_siAmountFreed / 1024.0f);
		fprintf(pFile, "\n");

		// fill vector with values and sort
		st_int32 nIndex = 0;
		st_int32 nTotalLeaked = 0;
		CArray<SLeakStats, false> vLeaks(0, SLeakStats( ));
		vLeaks.resize(g_mLeakMap.size( ));
		for (leak_map::iterator j = g_mLeakMap.begin( ); j != g_mLeakMap.end( ); ++j)
		{
			vLeaks[nIndex++] = j->second;
			nTotalLeaked += j->second.m_nAmount;
		}
		//sort(vLeaks.begin( ), vLeaks.end( ));
		vLeaks.sort( );

		if (nTotalLeaked != 0)
		{
			fprintf(pFile, "\tTotal leaked using SpeedTree allocator: %.2fKB\n", nTotalLeaked / 1024.0f);
			for (nIndex = 0; nIndex < int(vLeaks.size( )); ++nIndex)
				fprintf(pFile, "\t\t%d. \"%s\",total amount: %.2fKB\n", 
				nIndex + 1, vLeaks[nIndex].m_strDesc.c_str( ), vLeaks[nIndex].m_nAmount / 1024.0f);
		}
		else 
			fprintf(pFile, "\t[No leaks detected]\n");

		fclose(pFile);
		bSuccess = true;
	}

	if (bFreeTrackingData)
		g_mStatsMap.clear( );

	g_cMutex.Unlock( );
	
	return bSuccess;
}

#endif // SPEEDTREE_MEMORY_STATS


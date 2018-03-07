///////////////////////////////////////////////////////////////////////  
//  Allocator.h
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

#pragma once
#include <cstdlib>
#include "Core/ExportBegin.h"

#if defined(__GNUC__) || defined(__psp2__)
	#include <string.h>
#endif


///////////////////////////////////////////////////////////////////////  
//  Packing

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(push, 4)
#endif


// enable to get a complete report of the heap allocations used by the SpeedTree
// SDK; the reference application writes this file next to the executable (e.g. 
// st_memory_report_windows_opengl.csv, st_memory_report_windows_directx9.csv, etc)
//#define SPEEDTREE_MEMORY_STATS
//#define SPEEDTREE_MEMORY_STATS_VERBOSE


///////////////////////////////////////////////////////////////////////  
//  All SpeedTree SDK classes and variables are under the namespace "SpeedTree"

namespace SpeedTree
{

	///////////////////////////////////////////////////////////////////////  
	//  Enumeration EAllocationType

	enum EAllocationType
	{
		ALLOC_TYPE_TEMPORARY,
		ALLOC_TYPE_LONG_TERM
	};


	///////////////////////////////////////////////////////////////////////  
	//  Class CAllocator

	class ST_DLL_LINK CAllocator
	{
	public:
	virtual             		~CAllocator( )		{ }

	virtual void*				Alloc(size_t BlockSize, EAllocationType eType) = 0;
	virtual void				Free(void* pBlock) = 0;

	static  void   ST_CALL_CONV	TrackAlloc(const char* pDescription, void* pBlock, size_t sAmount);
	static  void   ST_CALL_CONV	TrackFree(void* pBlock, size_t sAmount);
	static  bool   ST_CALL_CONV	Report(const char* pFilename = 0, bool bFreeTrackingData = true);
	};


} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif

#include "Core/ExportEnd.h"

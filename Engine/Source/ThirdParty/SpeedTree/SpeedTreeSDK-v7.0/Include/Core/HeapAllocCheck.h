///////////////////////////////////////////////////////////////////////  
//  HeapAllocCheck.h
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


///////////////////////////////////////////////////////////////////////  
//  Packing

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(push, 4)
#endif


///////////////////////////////////////////////////////////////////////  
//  All SpeedTree SDK classes and variables are under the namespace "SpeedTree"

namespace SpeedTree
{

	///////////////////////////////////////////////////////////////////////  
	//  Enumeration ESdkLimit

	enum ESdkLimit
	{
		SDK_LIMIT_MAX_BASE_TREES,							// SHeapReserves::m_nMaxBaseTrees
		SDK_LIMIT_MAX_VISIBLE_TREE_CELLS,					// SHeapReserves::m_nMaxVisibleTreeCells
		SDK_LIMIT_MAX_VISIBLE_GRASS_CELLS,					// SHeapReserves::m_nMaxVisibleGrassCells
		SDK_LIMIT_MAX_VISIBLE_TERRAIN_CELLS,				// SHeapReserves::m_nMaxVisibleTerrainCells
		SDK_LIMIT_MAX_TREE_INSTANCES_IN_ANY_CELL,			// SHeapReserves::m_nMaxTreeInstancesInAnyCell
		SDK_LIMIT_MAX_PER_BASE_GRASS_INSTANCES_IN_ANY_CELL,	// SHeapReserves::m_nMaxPerBaseGrassInstancesInAnyCell
		SDK_LIMIT_COUNT
	};


	///////////////////////////////////////////////////////////////////////  
	//  Macro SPEEDTREE_HEAP_ALLOC_CHECK

	#define SPEEDTREE_HEAP_ALLOC_CHECK(type, variable, limit_enum) \
		CHeapAllocCheck<type > __cSdkHeapCheck(variable, limit_enum, __FILE__, __LINE__)


	///////////////////////////////////////////////////////////////////////  
	//  Class CHeapAllocCheck

	template <class T>
	class ST_DLL_LINK CHeapAllocCheck
	{
	#ifdef SPEEDTREE_RUNTIME_HEAP_CHECK
		public:
								CHeapAllocCheck(const T& tContainer, ESdkLimit eLimit, const char* pSourceFilename, st_int32 nSourceLineNum);
								~CHeapAllocCheck( );

		private:
				// constructor params
				const T*		m_pContainer;
				ESdkLimit		m_eLimit;
				const char*		m_pSourceFilename;
				st_int32		m_nSourceLineNum;

				size_t			m_siInitCapacity;
	#else
		public:
								CHeapAllocCheck(const T&, ESdkLimit, const char*, st_int32) { }
	#endif
	};

	// include inline functions
	#include "HeapAllocCheck_inl.h"

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif

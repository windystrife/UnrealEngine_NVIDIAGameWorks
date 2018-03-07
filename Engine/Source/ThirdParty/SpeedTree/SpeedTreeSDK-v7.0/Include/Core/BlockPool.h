///////////////////////////////////////////////////////////////////////  
//	ObjectPool.h
//
//	This file is part of IDV's lightweight STL-like container classes.
//	Like STL, these containers are templated and portable, but are thin
//	enough not to have some of the portability issues of STL nor will
//	it conflict with an application's similar libraries.  These containers
//	also contain some mechanisms for mitigating the inordinate number of
//	of heap allocations associated with the standard STL.
//
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization and may
//	not be copied or disclosed except in accordance with the terms of
//	that agreement
//
//      Copyright (c) 2003-2014 IDV, Inc.
//      All Rights Reserved.
//
//		IDV, Inc.
//		http://www.idvinc.com


/////////////////////////////////////////////////////////////////////
// Preprocessor

#pragma once
#include "Core/ExportBegin.h"
#include <cassert>


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
	// class CBlockPool

	template <bool bUseCustomAllocator = true>
	class ST_DLL_LINK CBlockPool
	{
	public:
								CBlockPool(size_t uiBlockSize, size_t uiNum = 10);
	virtual						~CBlockPool(void);
	
			size_t				size(void) const;
			void				resize(size_t uiSize);
			void				clear(bool bForce = false);

			typedef void* CReference;

			CReference			GrabBlock(void);
			void				ReleaseBlock(CReference& cRef);
			void*				ResolveBlock(const CReference& cRef) const;

	private:
			CBlockPool&			operator = (const CBlockPool& cR) { }

	protected:
			char*				m_pData;
			size_t*				m_pFreeLocations;
			size_t				m_uiSize;
			size_t				m_uiCurrent;
			const size_t		m_uiBlockSize;
	};

	// include inline functions
	#include "Core/BlockPool_inl.h"

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif

#include "Core/ExportEnd.h"



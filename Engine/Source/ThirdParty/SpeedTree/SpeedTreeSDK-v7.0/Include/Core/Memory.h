///////////////////////////////////////////////////////////////////////  
//  Memory.h
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

#include "Allocator.h"
#include "Core/Types.h"
#include <cassert>
#include <new>
#ifdef _XBOX
	#include <malloc.h>
#endif
#if defined(__GNUC__) || defined(__psp2__)
	#include <stdlib.h>
#endif


///////////////////////////////////////////////////////////////////////  
//  Packing

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(push, 4)
#endif


///////////////////////////////////////////////////////////////////////
//  namespace SpeedTree

namespace SpeedTree
{

	///////////////////////////////////////////////////////////////////////  
	//  class CHeapSystem

	class ST_DLL_LINK CHeapSystem
	{
	public:
	static	CAllocator*&	Allocator(void);
	static	size_t&			CurrentUse(void);
	static	size_t&			PeakUse(void);
	static	size_t&			NumAllocs(void);

	private:
	static	CAllocator*		m_pAllocator;
	static	size_t			m_siCurrentUsage;
	static	size_t			m_siPeakUsage;
	static	size_t			m_siNumAllocs;
	};


	///////////////////////////////////////////////////////////////////////
	//  struct SHeapHandle
	//
	//  Necessary overhead for overriding array allocation.

	struct SHeapHandle
	{
		size_t  m_siNumElements;
	};


	///////////////////////////////////////////////////////////////////////
	//  st_new
	//
	//	Alternate path for C++ operator::new; uses placement new to request
	//	allocation through SpeedTree's allocator interface.
	//
	//	Example usage:
	//	  CObject* pObj = st_new(CObject, "my object")(constr_param1, constr_param2, etc)

	#define st_new(TYPE, DESCRIPTION) new (st_allocate<TYPE>(DESCRIPTION, ALLOC_TYPE_LONG_TERM)) TYPE


	///////////////////////////////////////////////////////////////////////
	//  st_allocate

	template <typename TYPE> inline TYPE* st_allocate(const char* pDescription, EAllocationType eType = ALLOC_TYPE_LONG_TERM)
	{
		size_t siTotalSize = sizeof(TYPE);

		char* pRawBlock = (char*) (CHeapSystem::Allocator( ) ? CHeapSystem::Allocator( )->Alloc(siTotalSize, eType) : malloc(siTotalSize));
		if (pRawBlock)
		{
			TYPE* pBlock = (TYPE*) pRawBlock;

			// track all SpeedTree memory allocation
			CHeapSystem::CurrentUse( ) += siTotalSize;
			CHeapSystem::PeakUse( ) = st_max(CHeapSystem::PeakUse( ), CHeapSystem::CurrentUse( ));
			CHeapSystem::NumAllocs( )++;

			#ifdef SPEEDTREE_MEMORY_STATS
				SpeedTree::CAllocator::TrackAlloc(pDescription ? pDescription : "Unknown", pRawBlock, siTotalSize);
			#else
				ST_UNREF_PARAM(pDescription);
			#endif

			return pBlock;
		}

		return NULL;
	}


	///////////////////////////////////////////////////////////////////////
	//  st_new_array (equivalent of C++ operator::new[])

	template <typename TYPE> inline TYPE* st_new_array(size_t siNumElements, const char* pDescription, EAllocationType eType = ALLOC_TYPE_LONG_TERM)
	{
		size_t siTotalSize = sizeof(SpeedTree::SHeapHandle) + siNumElements * sizeof(TYPE);

		char* pRawBlock = (char*) (CHeapSystem::Allocator( ) ? CHeapSystem::Allocator( )->Alloc(siTotalSize, eType) : malloc(siTotalSize));
		if (pRawBlock)
		{
			// get the main part of the data, after the heap handle
			TYPE* pBlock = (TYPE*) (pRawBlock + sizeof(SpeedTree::SHeapHandle));

			// record the number of elements
			*((size_t*) pRawBlock) = siNumElements;

			// manually call the constructors using placement new
			TYPE* pElements = pBlock;
			for (size_t i = 0; i < siNumElements; ++i)
				(void) new (static_cast<void*>(pElements + i)) TYPE;

			// track all SpeedTree memory allocation
			CHeapSystem::CurrentUse( ) += siTotalSize;
			CHeapSystem::PeakUse( ) = st_max(CHeapSystem::PeakUse( ), CHeapSystem::CurrentUse( ));
			CHeapSystem::NumAllocs( )++;

			#ifdef SPEEDTREE_MEMORY_STATS
				SpeedTree::CAllocator::TrackAlloc(pDescription ? pDescription : "Unknown", pRawBlock, siTotalSize);
			#else
				ST_UNREF_PARAM(pDescription);
			#endif

			return pBlock;
		}
		
		return NULL;
	}


	///////////////////////////////////////////////////////////////////////
	//  st_delete (equivalent of C++ delete)

	template <typename TYPE> inline void st_delete(TYPE*& pBlock)
	{
		#ifdef SPEEDTREE_MEMORY_STATS
			if (pBlock)
				SpeedTree::CAllocator::TrackFree(pBlock, pBlock ? sizeof(TYPE) : 0);
		#endif

		if (pBlock)
		{
			pBlock->~TYPE( );
			
			if (CHeapSystem::Allocator( ))
				CHeapSystem::Allocator( )->Free(pBlock);
			else
				free(pBlock);
			pBlock = NULL;

			if (CHeapSystem::NumAllocs( ) == 905)

			// track all SpeedTree memory allocation
			CHeapSystem::CurrentUse( ) -= sizeof(TYPE);
		}
	}


	///////////////////////////////////////////////////////////////////////
	//  st_delete_array (equivalent of C++ delete[])

	template <typename TYPE> inline void st_delete_array(TYPE*& pRawBlock)
	{
		if (pRawBlock)
		{
			// extract the array size
			SpeedTree::SHeapHandle* pHeapHandle = (SpeedTree::SHeapHandle*) ( ((char*) pRawBlock) - sizeof(SpeedTree::SHeapHandle) );

			if (pHeapHandle)
			{
				// point to the elements
				TYPE* pElements = pRawBlock; 

				if (pElements)
				{
					// track all SpeedTree memory allocation
					CHeapSystem::CurrentUse( ) -= sizeof(SpeedTree::SHeapHandle) + pHeapHandle->m_siNumElements * sizeof(TYPE);

					for (size_t i = 0; i < pHeapHandle->m_siNumElements; ++i)
						(pElements + i)->~TYPE( );

					#ifdef SPEEDTREE_MEMORY_STATS
						SpeedTree::CAllocator::TrackFree(pHeapHandle, pHeapHandle->m_siNumElements * sizeof(TYPE) + sizeof(SpeedTree::SHeapHandle));
					#endif

					if (CHeapSystem::Allocator( ))
						CHeapSystem::Allocator( )->Free(pHeapHandle);
					else
						free(pHeapHandle);
					pRawBlock = NULL;
				}
			}
		}
	}

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif


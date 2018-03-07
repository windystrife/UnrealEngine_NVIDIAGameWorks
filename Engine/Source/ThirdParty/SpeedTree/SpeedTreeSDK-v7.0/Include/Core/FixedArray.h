///////////////////////////////////////////////////////////////////////  
//	FixedArray.h
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
#include "Core/Array.h"


///////////////////////////////////////////////////////////////////////  
//  Packing

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(push, 4)
#endif


///////////////////////////////////////////////////////////////////////  
//  All SpeedTree SDK classes and variables are under the namespace "SpeedTree"

namespace SpeedTree
{

	/////////////////////////////////////////////////////////////////////
	// class CFixedArray

	template <class T, size_t uiDataSize = 256>
	class ST_DLL_LINK CFixedArray
	{
	public:
								CFixedArray(void);
								CFixedArray(size_t uiSize, const T& tDefault = T( ));
								CFixedArray(const CFixedArray& cCopy);
	virtual						~CFixedArray(void);											

			// standard functions
			void 				clear(void);											
			bool 				empty(void) const;
			size_t				size(void) const;										
			size_t				capacity(void) const;
			bool				resize(size_t uiSize);
			bool				resize(size_t uiSize, const T& tDefault);							
			bool				reserve(size_t uiSize);							
			bool				push_back(const T& tNew);								
			void 				pop_back(void);
			void				erase_all(const T& tErase);
			T&					front(void);
			T&					back(void);
			T&					at(size_t uiIndex);
			const T&			at(size_t uiIndex) const;
			T&					operator [] (size_t uiIndex);	
			const T&			operator [] (size_t uiIndex) const;
			CFixedArray&		operator = (const CFixedArray& cRight);						
			
			// sorting			
			template <class CompareFunctor>
			void				sort(const CompareFunctor& Compare, bool bMemorySwap = false);
			void				sort(bool bMemorySwap = false);

			// iterator access
			typedef T*			iterator;	
			typedef T const*	const_iterator;	
			iterator			begin(void);											
			iterator			end(void);
			const_iterator		begin(void) const;								
			const_iterator		end(void) const;
			iterator 			erase(iterator iterWhere);
			iterator 			insert(iterator iterWhere, const T& tData);
		
			// these only work correctly on a sorted array
			iterator			lower(const T& tData);
			iterator			higher(const T& tData);
			void				lower_and_higher(const T& tData, iterator& iterLower, iterator& iterHigher);
			iterator			insert_sorted(const T& tData);
			iterator			insert_sorted_unique(const T& tData);

	protected:
			T					m_aData[uiDataSize];											
			size_t				m_uiSize;
	};

	// include inline functions
	#include "Core/FixedArray_inl.h"

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif

#include "Core/ExportEnd.h"




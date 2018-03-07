///////////////////////////////////////////////////////////////////////  
//	List.h
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
#include "Array.h"
#include <cassert>

#ifndef SPEEDTREE_NO_ALLOCATORS
	#include "Core/Memory.h"
#endif


///////////////////////////////////////////////////////////////////////  
//  All SpeedTree SDK classes and variables are under the namespace "SpeedTree"

namespace SpeedTree
{

	/////////////////////////////////////////////////////////////////////
	// class CList

	template <class T, bool bUseCustomAllocator = true>
	class ST_DLL_LINK CList
	{
	protected:
		class CNode;
	
	public:
								CList(void);
								CList(const CList& cRight);
	virtual						~CList( );

			// standard functions
			void				clear(void);					
			bool				empty(void) const;					
			size_t				size(void) const;						
			void				push_back(const T& tData);		
			void				push_front(const T& tData);		
			void				pop_back(void);					
			void				pop_front(void);
			T&					front(void);
			T&					back(void);
			CList&				operator = (const CList& cRight);
	
			// sorting
			void				sort(void);

			// iterator access
			class iterator_base
			{
			friend class CList;
			public:
				bool			operator == (const iterator_base& cRight) const;
				bool			operator != (const iterator_base& cRight) const;
				iterator_base&	operator ++ (void);
				iterator_base&	operator -- (void);

			protected:
								iterator_base(CNode* pNode = NULL);  // prevent base class instantiation
				CNode*			m_pNode;
			};

			class iterator : public iterator_base
			{
			public:
								iterator(CNode* pNode = NULL) : iterator_base(pNode) { }
								operator T* (void);
				T*				operator -> (void);
			};

			class const_iterator : public iterator_base
			{
			public:
								const_iterator(CNode* pNode = NULL) : iterator_base(pNode) { }
								const_iterator(const iterator& cRight) : iterator_base(cRight.m_pNode) { }
								operator T const* (void);
				T const*		operator -> (void);
			};
			
			iterator 			begin(void);
			iterator 			end(void);
			const_iterator		begin(void) const;
			const_iterator		end(void) const;
			iterator 			erase(iterator iterWhere);
			iterator 			insert(iterator iterWhere, const T& tData);
	
			// only works correctly on a sorted list
			iterator			insert_sorted(const T& tData);

	protected:
			CNode*				Allocate(const T& tData, CNode* pNext, CNode* pPrevious);
			void				Deallocate(CNode* pData);

			class CNode
			{
			public:
								CNode(const T& tData, CNode* pNext, CNode* pPrevious);
				T				m_tData;
				CNode*			m_pNext;
				CNode*			m_pPrevious;
			};

			CNode*				m_pStart;
			CNode*				m_pEnd;	
			size_t				m_uiSize;
	};

	// include inline functions
	#include "Core/List_inl.h"

} // end namespace SpeedTree

#include "Core/ExportEnd.h"

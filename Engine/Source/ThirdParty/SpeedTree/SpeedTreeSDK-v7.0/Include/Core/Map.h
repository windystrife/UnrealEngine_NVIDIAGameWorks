///////////////////////////////////////////////////////////////////////  
//	Map.h
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


///////////////////////////////////////////////////////////////////////
// Preprocessor

#pragma once
#include "ExportBegin.h"
#include <cstdlib>
#include <cassert>
#include "Array.h"
#include "BlockPool.h"
#include "Comparators.h"


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
	// class CMap

	template <class TKey, class TValue, class TCompare = CLess<TKey>, bool bUseCustomAllocator = true>
	class ST_DLL_LINK CMap
	{
	protected:
		class CNode;
		typedef CBlockPool<bUseCustomAllocator> CPool;
		typedef typename CBlockPool<bUseCustomAllocator>::CReference CNodeReference;
	
	public:
										CMap(size_t uiStartingPoolSize = 10);
										CMap(const CMap& cRight);
	virtual								~CMap(void);

			// standard functions
			void						clear(void);
			bool						empty(void) const;
			size_t						size(void) const;
			size_t						capacity(void) const;
			TValue&						operator [] (const TKey& tKey);
			CMap&						operator = (const CMap& cRight);

			// iterator access
			class ST_DLL_LINK iterator_base
			{
			friend class CMap;
			public:
					bool				operator == (const iterator_base& cRight) const;
					bool				operator != (const iterator_base& cRight) const;
					iterator_base&		operator ++ (void);
					iterator_base&		operator -- (void);

			protected:
										iterator_base(CNodeReference pNode = NULL, const CPool* pPool = NULL);
					CNode*				Ptr(void) const;

					CNodeReference		m_pNode;
					const CPool*		m_pPool;
			};

			class ST_DLL_LINK iterator : public iterator_base
			{
			public:			
										iterator(CNodeReference pNode = NULL, const CPool* pPool = NULL);
										iterator(const iterator& cRight);
					CNode*				operator -> (void) const;
			};

			class ST_DLL_LINK const_iterator : public iterator_base
			{
			public:				
										const_iterator(CNodeReference pNode = NULL, const CPool* pPool = NULL);
										const_iterator(const iterator& cRight);
										const_iterator(const const_iterator& cRight);
					CNode const*		operator -> (void) const;
			};

			iterator					begin(void);
			iterator					rbegin(void);
			iterator					end(void);
			const_iterator				begin(void) const;
			const_iterator				rbegin(void) const;
			const_iterator				end(void) const;
			iterator					find(const TKey& tKey) const;
			iterator 					erase(iterator iterWhere);
			iterator					lower(const TKey& tKey);
			iterator					higher(const TKey& tKey);
			void						lower_and_higher(const TKey& tKey, iterator& iterLower, iterator& iterHigher);
			const_iterator				lower(const TKey& tKey) const;
			const_iterator				higher(const TKey& tKey) const;
			void						lower_and_higher(const TKey& tKey, const_iterator& iterLower, const_iterator& iterHigher) const;

			// memory pool access
			void						ResizePool(size_t uiSize);

			// internal
			void						SetHeapDescription(const char* pDesc);

	protected:
			void						Rebalance(CNodeReference pCurrent);
			CNodeReference				Allocate(const TKey& tKey, CNodeReference pParent = NULL);
			void						Deallocate(CNodeReference& pData);
			CNode*						Ptr(CNodeReference pNode) const;

			class CNode
			{
			friend class CMap;
			friend class iterator_base;
			public:
					const TKey			first;
					TValue				second;

			private:
										CNode(const TKey& tKey, CNodeReference pParent = NULL);
					void				DeleteChildren(CMap* pMap);
					CNode&				operator = (const CNode& cR) { }

					CNodeReference		m_pLeft;
					CNodeReference		m_pRight;
					CNodeReference		m_pParent;
					size_t				m_uiLevel;
			};

			CNodeReference				m_pRoot;
			size_t						m_uiSize;
			CPool						m_cPool;
			const char*					m_pHeapDesc;
	};

	// include inline functions
	#include "Map_inl.h"

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif

#include "ExportEnd.h"



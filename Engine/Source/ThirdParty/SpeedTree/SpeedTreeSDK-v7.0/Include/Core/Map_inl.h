///////////////////////////////////////////////////////////////////////  
//	Map.inl
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
// CMap::CMap

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE CMap<TKey, TValue, TCompare, bUseCustomAllocator>::CMap(size_t uiStartingPoolSize) :
	m_pRoot(NULL),
	m_uiSize(0),
	m_cPool(sizeof(CNode), uiStartingPoolSize)
{
}
	

/////////////////////////////////////////////////////////////////////
// CMap::CMap

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE CMap<TKey, TValue, TCompare, bUseCustomAllocator>::CMap(const CMap& cRight) :
	m_pRoot(NULL),
	m_uiSize(0),
	m_cPool(sizeof(CNode), cRight.m_cPool.size( ))
{
	*this = cRight;
}


/////////////////////////////////////////////////////////////////////
// CMap::CMap

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE CMap<TKey, TValue, TCompare, bUseCustomAllocator>::~CMap(void)
{
	clear( );
}


/////////////////////////////////////////////////////////////////////
// CMap::clear

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE void CMap<TKey, TValue, TCompare, bUseCustomAllocator>::clear(void)
{
	if (m_pRoot != NULL)
	{
		Ptr(m_pRoot)->DeleteChildren(this);
		Deallocate(m_pRoot);
		m_pRoot = NULL;
	}

	m_uiSize = 0;
}


/////////////////////////////////////////////////////////////////////
// CMap::size

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE size_t CMap<TKey, TValue, TCompare, bUseCustomAllocator>::size(void) const
{
	return m_uiSize;
}


/////////////////////////////////////////////////////////////////////
// CMap::capacity

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE size_t CMap<TKey, TValue, TCompare, bUseCustomAllocator>::capacity(void) const
{
	return m_cPool.size( );
}


/////////////////////////////////////////////////////////////////////
// CMap::empty

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE bool CMap<TKey, TValue, TCompare, bUseCustomAllocator>::empty(void) const
{
	return (m_uiSize == 0);
}


/////////////////////////////////////////////////////////////////////
// CMap::operator[]

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE TValue& CMap<TKey, TValue, TCompare, bUseCustomAllocator>::operator[](const TKey& tKey)
{
	TCompare tCompare;

	CNodeReference pCurrent = m_pRoot;
	CNodeReference pParent = NULL;
	while (pCurrent != NULL && tKey != Ptr(pCurrent)->first)
	{
		pParent = pCurrent;
		if (tCompare(tKey, Ptr(pCurrent)->first))
			pCurrent = Ptr(pCurrent)->m_pLeft;
		else
			pCurrent = Ptr(pCurrent)->m_pRight;
	}

	if (pCurrent == NULL)
	{
		pCurrent = Allocate(tKey, pParent);

		if (pParent == NULL)
			m_pRoot = pCurrent;
		else if (tCompare(tKey, Ptr(pParent)->first))
			Ptr(pParent)->m_pLeft = pCurrent;
		else
			Ptr(pParent)->m_pRight = pCurrent;

		Rebalance(pParent);
		++m_uiSize;
	}

	return Ptr(pCurrent)->second;
}


/////////////////////////////////////////////////////////////////////
// CMap::operator =

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE CMap<TKey, TValue, TCompare, bUseCustomAllocator>& CMap<TKey, TValue, TCompare, bUseCustomAllocator>::operator = (const CMap<TKey, TValue, TCompare, bUseCustomAllocator>& cRight)
{
	clear( );

	if (cRight.m_pRoot != NULL)
	{
		m_pRoot = Allocate(cRight.Ptr(cRight.m_pRoot)->first, NULL);
		Ptr(m_pRoot)->second = cRight.Ptr(cRight.m_pRoot)->second;
		Ptr(m_pRoot)->m_uiLevel = cRight.Ptr(cRight.m_pRoot)->m_uiLevel;
		++m_uiSize;

		CArray<CNodeReference> aThis;
		CArray<CNodeReference> aRight;
		aThis.push_back(m_pRoot);
		aRight.push_back(cRight.m_pRoot);

		while (!aThis.empty( ))
		{
			CNodeReference pThis = aThis.back( );
			CNodeReference pRight = aRight.back( );
			aThis.pop_back( );
			aRight.pop_back( );

			if (cRight.Ptr(pRight)->m_pLeft != NULL)
			{
				Ptr(pThis)->m_pLeft = Allocate(cRight.Ptr(cRight.Ptr(pRight)->m_pLeft)->first, pThis);
				Ptr(Ptr(pThis)->m_pLeft)->second = cRight.Ptr(cRight.Ptr(pRight)->m_pLeft)->second;
				Ptr(Ptr(pThis)->m_pLeft)->m_uiLevel = cRight.Ptr(cRight.Ptr(pRight)->m_pLeft)->m_uiLevel;
				aThis.push_back(Ptr(pThis)->m_pLeft);
				aRight.push_back(cRight.Ptr(pRight)->m_pLeft);
				++m_uiSize;
			}

			if (cRight.Ptr(pRight)->m_pRight != NULL)
			{
				Ptr(pThis)->m_pRight = Allocate(cRight.Ptr(cRight.Ptr(pRight)->m_pRight)->first, pThis);
				Ptr(Ptr(pThis)->m_pRight)->second = cRight.Ptr(cRight.Ptr(pRight)->m_pRight)->second;
				Ptr(Ptr(pThis)->m_pRight)->m_uiLevel = cRight.Ptr(cRight.Ptr(pRight)->m_pRight)->m_uiLevel;
				aThis.push_back(Ptr(pThis)->m_pRight);
				aRight.push_back(cRight.Ptr(pRight)->m_pRight);
				++m_uiSize;
			}
		}
	}

	return (*this);
}


/////////////////////////////////////////////////////////////////////
// CMap::begin

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CMap<TKey, TValue, TCompare, bUseCustomAllocator>::iterator CMap<TKey, TValue, TCompare, bUseCustomAllocator>::begin(void) 
{ 
	if (m_pRoot == NULL)
		return iterator( );
	
	CNodeReference pCurrent = m_pRoot;
	while (Ptr(pCurrent)->m_pLeft != NULL)
		pCurrent = Ptr(pCurrent)->m_pLeft;

	return iterator(pCurrent, &m_cPool); 
}


/////////////////////////////////////////////////////////////////////
// CMap::rbegin

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CMap<TKey, TValue, TCompare, bUseCustomAllocator>::iterator CMap<TKey, TValue, TCompare, bUseCustomAllocator>::rbegin(void) 
{ 
	if (m_pRoot == NULL)
		return iterator( );
	
	CNodeReference pCurrent = m_pRoot;
	while (Ptr(pCurrent)->m_pRight != NULL)
		pCurrent = Ptr(pCurrent)->m_pRight;

	return iterator(pCurrent, &m_cPool); 
}


/////////////////////////////////////////////////////////////////////
// CMap::end

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CMap<TKey, TValue, TCompare, bUseCustomAllocator>::iterator CMap<TKey, TValue, TCompare, bUseCustomAllocator>::end(void) 
{ 
	return iterator( ); 
}


/////////////////////////////////////////////////////////////////////
// CMap::begin

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CMap<TKey, TValue, TCompare, bUseCustomAllocator>::const_iterator CMap<TKey, TValue, TCompare, bUseCustomAllocator>::begin(void) const
{ 
	return ((CMap*)this)->begin( );
}


/////////////////////////////////////////////////////////////////////
// CMap::rbegin

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CMap<TKey, TValue, TCompare, bUseCustomAllocator>::const_iterator CMap<TKey, TValue, TCompare, bUseCustomAllocator>::rbegin(void) const
{ 
	return ((CMap*)this)->rbegin( );
}


/////////////////////////////////////////////////////////////////////
// CMap::end

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CMap<TKey, TValue, TCompare, bUseCustomAllocator>::const_iterator CMap<TKey, TValue, TCompare, bUseCustomAllocator>::end(void) const
{ 
	return const_iterator( ); 
}


/////////////////////////////////////////////////////////////////////
// CMap::find

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CMap<TKey, TValue, TCompare, bUseCustomAllocator>::iterator CMap<TKey, TValue, TCompare, bUseCustomAllocator>::find(const TKey& tKey) const
{
	TCompare tCompare;
	CNodeReference pCurrent = m_pRoot;

	while (pCurrent != NULL && Ptr(pCurrent)->first != tKey)
	{
		if (tCompare(tKey, Ptr(pCurrent)->first))
			pCurrent = Ptr(pCurrent)->m_pLeft;
		else
			pCurrent = Ptr(pCurrent)->m_pRight;
	}

	return iterator(pCurrent, &m_cPool);
}


/////////////////////////////////////////////////////////////////////
// CMap::erase

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CMap<TKey, TValue, TCompare, bUseCustomAllocator>::iterator CMap<TKey, TValue, TCompare, bUseCustomAllocator>::erase(iterator iterWhere)
{
	#ifdef SPEEDTREE_ITERATOR_DEBUGGING
		// make sure iterator is part of this map
		assert(iterWhere.m_pNode != NULL);
		CNodeReference pRoot = iterWhere.m_pNode;
		while (Ptr(pRoot)->m_pParent != NULL)
			pRoot = Ptr(pRoot)->m_pParent;
		assert(pRoot == m_pRoot);
	#endif

	CNodeReference pRemove = iterWhere.m_pNode;
	++iterWhere;

	// delete like a normal binary search tree
	bool bRootDelete = (pRemove == m_pRoot);
	CNodeReference pBalance = NULL;
	if (Ptr(pRemove)->m_pLeft == NULL && Ptr(pRemove)->m_pRight == NULL)
	{
		// has no children
		pBalance = Ptr(pRemove)->m_pParent;
		if (Ptr(pRemove)->m_pParent != NULL)
		{
			if (Ptr(Ptr(pRemove)->m_pParent)->m_pLeft == pRemove)
				Ptr(Ptr(pRemove)->m_pParent)->m_pLeft = NULL;
			else
				Ptr(Ptr(pRemove)->m_pParent)->m_pRight = NULL;
		}
		if (bRootDelete)
			m_pRoot = NULL;
	}
	else if (Ptr(pRemove)->m_pLeft == NULL || Ptr(pRemove)->m_pRight == NULL)
	{
		// has only one child
		CNodeReference pChild = Ptr(pRemove)->m_pLeft;
		if (pChild == NULL)
			pChild = Ptr(pRemove)->m_pRight;
		pBalance = pChild;
		if (Ptr(pRemove)->m_pParent != NULL)
		{
			if (Ptr(Ptr(pRemove)->m_pParent)->m_pLeft == pRemove)
				Ptr(Ptr(pRemove)->m_pParent)->m_pLeft = pChild;
			else
				Ptr(Ptr(pRemove)->m_pParent)->m_pRight = pChild;
		}
		Ptr(pChild)->m_pParent = Ptr(pRemove)->m_pParent;
		if (bRootDelete)
			m_pRoot = pChild;
	}
	else
	{
		// has both children
		CNodeReference pSwitch = Ptr(pRemove)->m_pLeft;
		while (Ptr(pSwitch)->m_pRight != NULL)
			pSwitch = Ptr(pSwitch)->m_pRight;

		if (pSwitch == Ptr(pRemove)->m_pLeft)
		{
			Ptr(pSwitch)->m_pRight = Ptr(pRemove)->m_pRight;
			if (Ptr(pSwitch)->m_pRight != NULL)
				Ptr(Ptr(pSwitch)->m_pRight)->m_pParent = pSwitch;
			pBalance = pSwitch;
		}
		else
		{
			CNodeReference pParent = Ptr(pSwitch)->m_pParent;
			Ptr(pParent)->m_pRight = Ptr(pSwitch)->m_pLeft;
			if (Ptr(pParent)->m_pRight != NULL)
				Ptr(Ptr(pParent)->m_pRight)->m_pParent = pParent;

			Ptr(pSwitch)->m_pLeft = Ptr(pRemove)->m_pLeft;
			Ptr(pSwitch)->m_pRight = Ptr(pRemove)->m_pRight;
			Ptr(Ptr(pSwitch)->m_pLeft)->m_pParent = pSwitch;
			if (Ptr(pSwitch)->m_pRight != NULL)
				Ptr(Ptr(pSwitch)->m_pRight)->m_pParent = pSwitch;
			pBalance = pParent;
		}

		if (Ptr(pRemove)->m_pParent != NULL)
		{
			if (Ptr(Ptr(pRemove)->m_pParent)->m_pLeft == pRemove)
				Ptr(Ptr(pRemove)->m_pParent)->m_pLeft = pSwitch;
			else
				Ptr(Ptr(pRemove)->m_pParent)->m_pRight = pSwitch;
		}
		Ptr(pSwitch)->m_pParent = Ptr(pRemove)->m_pParent;
		Ptr(pSwitch)->m_uiLevel = Ptr(pRemove)->m_uiLevel;
		if (bRootDelete)
			m_pRoot = pSwitch;
	}

	Deallocate(pRemove);

	// re-level and balance
	if (pBalance != NULL)
	{
		CNodeReference pLower = pBalance;
		while (pLower != NULL)
		{
			if ((Ptr(pLower)->m_pLeft != NULL && Ptr(Ptr(pLower)->m_pLeft)->m_uiLevel < Ptr(pLower)->m_uiLevel - 1) ||
				(Ptr(pLower)->m_pRight != NULL && Ptr(Ptr(pLower)->m_pRight)->m_uiLevel < Ptr(pLower)->m_uiLevel - 1))
			{
				--Ptr(pLower)->m_uiLevel;
				pLower = Ptr(pLower)->m_pParent;
			}
			else
				pLower = NULL;
		}

		Rebalance(pBalance);
	}	
	--m_uiSize;

	return iterWhere;
}


/////////////////////////////////////////////////////////////////////
// CMap::lower

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CMap<TKey, TValue, TCompare, bUseCustomAllocator>::iterator CMap<TKey, TValue, TCompare, bUseCustomAllocator>::lower(const TKey& tKey)
{
	TCompare tCompare;

	CNodeReference pCurrent = m_pRoot;
	CNodeReference pParent = NULL;
	while (pCurrent != NULL && tKey != Ptr(pCurrent)->first)
	{
		pParent = pCurrent;
		if (tCompare(tKey, Ptr(pCurrent)->first))
			pCurrent = Ptr(pCurrent)->m_pLeft;
		else
			pCurrent = Ptr(pCurrent)->m_pRight;
	}

	if (pCurrent == NULL)
	{
		while (pParent != NULL && tCompare(tKey, Ptr(pParent)->first))
			pParent = Ptr(pParent)->m_pParent;
		return iterator(pParent, &m_cPool);
	}
	return iterator(pCurrent, &m_cPool);
}


/////////////////////////////////////////////////////////////////////
// CMap::higher

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CMap<TKey, TValue, TCompare, bUseCustomAllocator>::iterator CMap<TKey, TValue, TCompare, bUseCustomAllocator>::higher(const TKey& tKey)
{
	TCompare tCompare;

	iterator iterHigher = lower(tKey);
	if (iterHigher == end( ))
	{
		if (m_uiSize > 0)
		{
			iterator iterBegin = begin( );
			if (tCompare(tKey, iterBegin->first))
				iterHigher = iterBegin;
		}
	}
	else if (tCompare(iterHigher->first, tKey))
		++iterHigher;
	return iterHigher;
}


/////////////////////////////////////////////////////////////////////
// CMap::lower_and_higher

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
void CMap<TKey, TValue, TCompare, bUseCustomAllocator>::lower_and_higher(const TKey& tKey, iterator& iterLower, iterator& iterHigher)
{
	TCompare tCompare;

	iterLower = lower(tKey);
	iterHigher = iterLower;
	if (iterHigher == end( ))
	{
		if (m_uiSize > 0)
		{
			iterator iterBegin = begin( );
			if (tCompare(tKey, iterBegin->first))
				iterHigher = iterBegin;
		}
	}
	else if (tCompare(iterHigher->first, tKey))
		++iterHigher;
}


/////////////////////////////////////////////////////////////////////
// CMap::lower

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CMap<TKey, TValue, TCompare, bUseCustomAllocator>::const_iterator CMap<TKey, TValue, TCompare, bUseCustomAllocator>::lower(const TKey& tKey) const
{
	return const_cast<CMap*>(this)->lower(tKey);
}


/////////////////////////////////////////////////////////////////////
// CMap::higher

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CMap<TKey, TValue, TCompare, bUseCustomAllocator>::const_iterator CMap<TKey, TValue, TCompare, bUseCustomAllocator>::higher(const TKey& tKey) const
{
	return const_cast<CMap*>(this)->higher(tKey);
}


/////////////////////////////////////////////////////////////////////
// CMap::lower_and_higher

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
void CMap<TKey, TValue, TCompare, bUseCustomAllocator>::lower_and_higher(const TKey& tKey, const_iterator& iterLower, const_iterator& iterHigher) const
{
	return const_cast<CMap*>(this)->lower_and_higher(tKey, iterLower, iterHigher);
}


/////////////////////////////////////////////////////////////////////
// CMap::ResizePool

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE void CMap<TKey, TValue, TCompare, bUseCustomAllocator>::ResizePool(size_t uiSize)
{
	m_cPool.resize(uiSize);
}


/////////////////////////////////////////////////////////////////////
// CMap::SetHeapDescription

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE void CMap<TKey, TValue, TCompare, bUseCustomAllocator>::SetHeapDescription(const char* pHeapDesc)
{
	m_pHeapDesc = pHeapDesc;
}


/////////////////////////////////////////////////////////////////////
// CMap::Allocate

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CMap<TKey, TValue, TCompare, bUseCustomAllocator>::CNodeReference CMap<TKey, TValue, TCompare, bUseCustomAllocator>::Allocate(const TKey& tKey, CNodeReference pParent)
{
	CNodeReference cReturn = m_cPool.GrabBlock( );
	
	new (m_cPool.ResolveBlock(cReturn)) CNode(tKey, pParent);
	return cReturn;
}


/////////////////////////////////////////////////////////////////////
// CMap::Deallocate

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE void CMap<TKey, TValue, TCompare, bUseCustomAllocator>::Deallocate(CNodeReference& pData)
{
	Ptr(pData)->~CNode( );
	m_cPool.ReleaseBlock(pData);
}


/////////////////////////////////////////////////////////////////////
// CMap::Ptr

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CMap<TKey, TValue, TCompare, bUseCustomAllocator>::CNode* CMap<TKey, TValue, TCompare, bUseCustomAllocator>::Ptr(CNodeReference pNode) const 
{ 
	return (CNode*)m_cPool.ResolveBlock(pNode); 
}


/////////////////////////////////////////////////////////////////////
// CMap::Rebalance

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE void CMap<TKey, TValue, TCompare, bUseCustomAllocator>::Rebalance(CNodeReference pCurrent)
{
	const unsigned char c_ucTestSteps = 5;
	unsigned char ucSteps = c_ucTestSteps;

	while (pCurrent != NULL)
	{
		if (Ptr(pCurrent)->m_pLeft != NULL && Ptr(Ptr(pCurrent)->m_pLeft)->m_uiLevel == Ptr(pCurrent)->m_uiLevel) 
		{
			// skew
			if (Ptr(pCurrent)->m_pRight != NULL && Ptr(Ptr(pCurrent)->m_pRight)->m_uiLevel == Ptr(pCurrent)->m_uiLevel) 
			{
				// combo skew/split
				++Ptr(pCurrent)->m_uiLevel;
			}
			else
			{
				CNodeReference pSave = Ptr(pCurrent)->m_pLeft;
				Ptr(pCurrent)->m_pLeft = Ptr(pSave)->m_pRight;
				if (Ptr(pCurrent)->m_pLeft != NULL)
					Ptr(Ptr(pCurrent)->m_pLeft)->m_pParent = pCurrent;
				Ptr(pSave)->m_pRight = pCurrent;
				Ptr(pSave)->m_pParent = Ptr(pCurrent)->m_pParent;
				if (Ptr(pSave)->m_pParent == NULL)
					m_pRoot = pSave;
				else if (Ptr(Ptr(pSave)->m_pParent)->m_pRight == pCurrent)
					Ptr(Ptr(pSave)->m_pParent)->m_pRight = pSave;
				else
					Ptr(Ptr(pSave)->m_pParent)->m_pLeft = pSave;		
				Ptr(pCurrent)->m_pParent = pSave;
				pCurrent = pSave;
			}
			
			ucSteps = c_ucTestSteps;
		}
		else if (Ptr(pCurrent)->m_pRight != NULL && Ptr(Ptr(pCurrent)->m_pRight)->m_pRight != NULL && Ptr(Ptr(Ptr(pCurrent)->m_pRight)->m_pRight)->m_uiLevel == Ptr(pCurrent)->m_uiLevel) 
		{
			// split
			CNodeReference pSave = Ptr(pCurrent)->m_pRight;
			Ptr(pCurrent)->m_pRight = Ptr(pSave)->m_pLeft;
			if (Ptr(pCurrent)->m_pRight != NULL)
				Ptr(Ptr(pCurrent)->m_pRight)->m_pParent = pCurrent;
			Ptr(pSave)->m_pLeft = pCurrent;
			Ptr(pSave)->m_pParent = Ptr(pCurrent)->m_pParent;
			if (Ptr(pSave)->m_pParent == NULL)
				m_pRoot = pSave;
			else if (Ptr(Ptr(pSave)->m_pParent)->m_pRight == pCurrent)
				Ptr(Ptr(pSave)->m_pParent)->m_pRight = pSave;
			else
				Ptr(Ptr(pSave)->m_pParent)->m_pLeft = pSave;
			Ptr(pCurrent)->m_pParent = pSave;
			pCurrent = pSave;
			++Ptr(pCurrent)->m_uiLevel;
			ucSteps = c_ucTestSteps;
		}

		// early bail out if we've gone sufficiently far to check for rebalancing
		if (--ucSteps == 0)
			return;

		pCurrent = Ptr(pCurrent)->m_pParent;
	}
}


/////////////////////////////////////////////////////////////////////
// CMap::iterator_base::iterator_base

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE CMap<TKey, TValue, TCompare, bUseCustomAllocator>::iterator_base::iterator_base(CNodeReference pNode, const CPool* pPool) : 
	m_pNode(pNode),
	m_pPool(pPool)
{
}


/////////////////////////////////////////////////////////////////////
// CMap::iterator_base::operator ++

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CMap<TKey, TValue, TCompare, bUseCustomAllocator>::iterator_base& CMap<TKey, TValue, TCompare, bUseCustomAllocator>::iterator_base::operator ++ (void) 
{ 
	assert(m_pNode != NULL); 

	if (Ptr( )->m_pRight == NULL)
	{
		CNodeReference pLastNode = NULL;
		while (m_pNode != NULL && pLastNode == Ptr( )->m_pRight)
		{	
			pLastNode = m_pNode;
			m_pNode = Ptr( )->m_pParent;
		}
	}
	else
	{
		m_pNode = Ptr( )->m_pRight;
		while (Ptr( )->m_pLeft != NULL)
			m_pNode = Ptr( )->m_pLeft;
	}

	return *(this);
}


/////////////////////////////////////////////////////////////////////
// CMap::iterator_base::operator --

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CMap<TKey, TValue, TCompare, bUseCustomAllocator>::iterator_base& CMap<TKey, TValue, TCompare, bUseCustomAllocator>::iterator_base::operator -- (void) 
{ 
	assert(m_pNode != NULL); 

	if (Ptr( )->m_pLeft == NULL)
	{
		CNodeReference pLastNode = NULL;
		while (m_pNode != NULL && pLastNode == Ptr( )->m_pLeft)
		{	
			pLastNode = m_pNode;
			m_pNode = Ptr( )->m_pParent;
		}
	}
	else
	{
		m_pNode = Ptr( )->m_pLeft;
		while (Ptr( )->m_pRight != NULL)
			m_pNode = Ptr( )->m_pRight;
	}

	return *(this);
}


/////////////////////////////////////////////////////////////////////
// CMap::iterator_base::operator ==

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE bool CMap<TKey, TValue, TCompare, bUseCustomAllocator>::iterator_base::operator == (const iterator_base& cRight) const 
{ 
	return (m_pNode == cRight.m_pNode); 
}


/////////////////////////////////////////////////////////////////////
// CMap::iterator_base::operator !=

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE bool CMap<TKey, TValue, TCompare, bUseCustomAllocator>::iterator_base::operator != (const iterator_base& cRight) const 
{ 
	return (m_pNode != cRight.m_pNode); 
}


/////////////////////////////////////////////////////////////////////
// CMap::iterator_base::Ptr

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CMap<TKey, TValue, TCompare, bUseCustomAllocator>::CNode* CMap<TKey, TValue, TCompare, bUseCustomAllocator>::iterator_base::Ptr(void) const 
{ 
	return (m_pPool ? (CNode*)m_pPool->ResolveBlock(m_pNode) : NULL);
}


/////////////////////////////////////////////////////////////////////
// CMap::iterator::iterator

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE CMap<TKey, TValue, TCompare, bUseCustomAllocator>::iterator::iterator(CNodeReference pNode, const CPool* pPool) : 
	iterator_base(pNode, pPool) 
{ 
}


/////////////////////////////////////////////////////////////////////
// CMap::iterator::iterator

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE CMap<TKey, TValue, TCompare, bUseCustomAllocator>::iterator::iterator(const iterator& cRight) : 
	iterator_base(cRight.m_pNode, cRight.m_pPool) 
{ 
}


/////////////////////////////////////////////////////////////////////
// CMap::iterator::operator ->

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CMap<TKey, TValue, TCompare, bUseCustomAllocator>::CNode* CMap<TKey, TValue, TCompare, bUseCustomAllocator>::iterator::operator -> (void) const
{
	return this->Ptr( );
}


/////////////////////////////////////////////////////////////////////
// CMap::const_iterator::const_iterator

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE CMap<TKey, TValue, TCompare, bUseCustomAllocator>::const_iterator::const_iterator(CNodeReference pNode, const CPool* pPool) : 
	iterator_base(pNode, pPool) 
{ 
}


/////////////////////////////////////////////////////////////////////
// CMap::const_iterator::const_iterator

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE CMap<TKey, TValue, TCompare, bUseCustomAllocator>::const_iterator::const_iterator(const iterator& cRight) : 
	iterator_base(((const_iterator*)&cRight)->m_pNode, ((const_iterator*)&cRight)->m_pPool) 
{ 
}


/////////////////////////////////////////////////////////////////////
// CMap::const_iterator::const_iterator

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE CMap<TKey, TValue, TCompare, bUseCustomAllocator>::const_iterator::const_iterator(const const_iterator& cRight) : 
	iterator_base(cRight.m_pNode, cRight.m_pPool) 
{ 
}


/////////////////////////////////////////////////////////////////////
// CMap::iterator::operator ->

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CMap<TKey, TValue, TCompare, bUseCustomAllocator>::CNode const* CMap<TKey, TValue, TCompare, bUseCustomAllocator>::const_iterator::operator -> (void) const
{
	return this->Ptr( );
}


/////////////////////////////////////////////////////////////////////
// CMap::CNode::CNode

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
ST_INLINE CMap<TKey, TValue, TCompare, bUseCustomAllocator>::CNode::CNode(const TKey& tKey, CNodeReference pParent) :
	first(tKey),
	m_pLeft(NULL),
	m_pRight(NULL),
	m_pParent(pParent),
	m_uiLevel(0)
{
}


/////////////////////////////////////////////////////////////////////
// CMap::CNode::DeleteChildren

template <class TKey, class TValue, class TCompare, bool bUseCustomAllocator>
inline void CMap<TKey, TValue, TCompare, bUseCustomAllocator>::CNode::DeleteChildren(CMap* pMap)
{	
	if (m_pLeft != NULL)
	{
		((CNode*)pMap->m_cPool.ResolveBlock(m_pLeft))->DeleteChildren(pMap);
		pMap->Deallocate(m_pLeft);
	}

	if (m_pRight != NULL)
	{
		((CNode*)pMap->m_cPool.ResolveBlock(m_pRight))->DeleteChildren(pMap);
		pMap->Deallocate(m_pRight);
	}
}




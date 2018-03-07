///////////////////////////////////////////////////////////////////////  
//	Set.inl
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
// CSet::CSet

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE CSet<T, TCompare, bUseCustomAllocator>::CSet(size_t uiStartingPoolSize) :
	m_pRoot(NULL),
	m_uiSize(0),
	m_cPool(sizeof(CNode), uiStartingPoolSize)
{
}
	

/////////////////////////////////////////////////////////////////////
// CSet::CSet

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE CSet<T, TCompare, bUseCustomAllocator>::CSet(const CSet& cRight) :
	m_pRoot(NULL),
	m_uiSize(0),
	m_cPool(sizeof(CNode), cRight.m_cPool.size( ))
{
	*this = cRight;
}


/////////////////////////////////////////////////////////////////////
// CSet::CSet

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE CSet<T, TCompare, bUseCustomAllocator>::~CSet(void)
{
	clear( );
}


/////////////////////////////////////////////////////////////////////
// CSet::clear

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE void CSet<T, TCompare, bUseCustomAllocator>::clear(void)
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
// CSet::size

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE size_t CSet<T, TCompare, bUseCustomAllocator>::size(void) const
{
	return m_uiSize;
}


/////////////////////////////////////////////////////////////////////
// CSet::capacity

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE size_t CSet<T, TCompare, bUseCustomAllocator>::capacity(void) const
{
	return m_cPool.size( );
}


/////////////////////////////////////////////////////////////////////
// CSet::empty

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE bool CSet<T, TCompare, bUseCustomAllocator>::empty(void) const
{
	return (m_uiSize == 0);
}


/////////////////////////////////////////////////////////////////////
// CSet::operator =

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE CSet<T, TCompare, bUseCustomAllocator>& CSet<T, TCompare, bUseCustomAllocator>::operator = (const CSet<T, TCompare, bUseCustomAllocator>& cRight)
{
	clear( );

	if (cRight.m_pRoot != NULL)
	{
		m_pRoot = Allocate(cRight.Ptr(cRight.m_pRoot)->m_tData, NULL);
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
				Ptr(pThis)->m_pLeft = Allocate(cRight.Ptr(cRight.Ptr(pRight)->m_pLeft)->m_tData, pThis);
				Ptr(Ptr(pThis)->m_pLeft)->m_uiLevel = cRight.Ptr(cRight.Ptr(pRight)->m_pLeft)->m_uiLevel;
				aThis.push_back(Ptr(pThis)->m_pLeft);
				aRight.push_back(cRight.Ptr(pRight)->m_pLeft);
				++m_uiSize;
			}

			if (cRight.Ptr(pRight)->m_pRight != NULL)
			{
				Ptr(pThis)->m_pRight = Allocate(cRight.Ptr(cRight.Ptr(pRight)->m_pRight)->m_tData, pThis);
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
// CSet::begin

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CSet<T, TCompare, bUseCustomAllocator>::iterator CSet<T, TCompare, bUseCustomAllocator>::begin(void) const
{ 
	if (m_pRoot == NULL)
		return iterator( );
	
	CNodeReference pCurrent = m_pRoot;
	while (Ptr(pCurrent)->m_pLeft != NULL)
		pCurrent = Ptr(pCurrent)->m_pLeft;

	return iterator(pCurrent, &m_cPool); 
}


/////////////////////////////////////////////////////////////////////
// CSet::rbegin

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CSet<T, TCompare, bUseCustomAllocator>::iterator CSet<T, TCompare, bUseCustomAllocator>::rbegin(void) const
{ 
	if (m_pRoot == NULL)
		return iterator( );
	
	CNodeReference pCurrent = m_pRoot;
	while (Ptr(pCurrent)->m_pRight != NULL)
		pCurrent = Ptr(pCurrent)->m_pRight;

	return iterator(pCurrent, &m_cPool); 
}


/////////////////////////////////////////////////////////////////////
// CSet::end

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CSet<T, TCompare, bUseCustomAllocator>::iterator CSet<T, TCompare, bUseCustomAllocator>::end(void) const
{ 
	return iterator( ); 
}


/////////////////////////////////////////////////////////////////////
// CSet::find

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CSet<T, TCompare, bUseCustomAllocator>::iterator CSet<T, TCompare, bUseCustomAllocator>::find(const T& tData) const
{
	TCompare tCompare;

	CNodeReference pCurrent = m_pRoot;
	while (pCurrent != NULL && Ptr(pCurrent)->m_tData != tData)
	{
		if (tCompare(tData, Ptr(pCurrent)->m_tData))
			pCurrent = Ptr(pCurrent)->m_pLeft;
		else
			pCurrent = Ptr(pCurrent)->m_pRight;
	}

	return iterator(pCurrent, &m_cPool);
}


/////////////////////////////////////////////////////////////////////
// CSet::insert

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CSet<T, TCompare, bUseCustomAllocator>::iterator CSet<T, TCompare, bUseCustomAllocator>::insert(const T& tData)
{
	TCompare tCompare;

	CNodeReference pCurrent = m_pRoot;
	CNodeReference pParent = NULL;
	while (pCurrent != NULL && tData != Ptr(pCurrent)->m_tData)
	{
		pParent = pCurrent;
		if (tCompare(tData, Ptr(pCurrent)->m_tData))
			pCurrent = Ptr(pCurrent)->m_pLeft;
		else
			pCurrent = Ptr(pCurrent)->m_pRight;
	}

	if (pCurrent == NULL)
	{
		pCurrent = Allocate(tData, pParent);

		if (pParent == NULL)
			m_pRoot = pCurrent;
		else if (tCompare(tData, Ptr(pParent)->m_tData))
			Ptr(pParent)->m_pLeft = pCurrent;
		else
			Ptr(pParent)->m_pRight = pCurrent;

		Rebalance(pParent);
		++m_uiSize;
	}

	return iterator(pCurrent, &m_cPool);
}


/////////////////////////////////////////////////////////////////////
// CSet::erase

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CSet<T, TCompare, bUseCustomAllocator>::iterator CSet<T, TCompare, bUseCustomAllocator>::erase(iterator iterWhere)
{
	#ifdef SPEEDTREE_ITERATOR_DEBUGGING
		// make sure iterator is part of this Set
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
				--pLower->m_uiLevel;
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
// CSet::Rebalance

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE void CSet<T, TCompare, bUseCustomAllocator>::Rebalance(CNodeReference pCurrent)
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
// CSet::lower

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CSet<T, TCompare, bUseCustomAllocator>::iterator CSet<T, TCompare, bUseCustomAllocator>::lower(const T& tData) const
{
	TCompare tCompare;

	CNodeReference pCurrent = m_pRoot;
	CNodeReference pParent = NULL;
	while (pCurrent != NULL && tData != Ptr(pCurrent)->m_tData)
	{
		pParent = pCurrent;
		if (tCompare(tData, Ptr(pCurrent)->m_tData))
			pCurrent = Ptr(pCurrent)->m_pLeft;
		else
			pCurrent = Ptr(pCurrent)->m_pRight;
	}

	if (pCurrent == NULL)
	{
		while (pParent != NULL && tCompare(tData, Ptr(pParent)->m_tData))
			pParent = Ptr(pParent)->m_pParent;
		return iterator(pParent, &m_cPool);
	}
	return iterator(pCurrent, &m_cPool);
}


/////////////////////////////////////////////////////////////////////
// CSet::higher

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CSet<T, TCompare, bUseCustomAllocator>::iterator CSet<T, TCompare, bUseCustomAllocator>::higher(const T& tData) const
{
	TCompare tCompare;

	iterator iterHigher = lower(tData);
	if (iterHigher == end( ))
	{
		if (m_uiSize > 0)
		{

			iterator iterBegin = begin( );
			if (tCompare(tData, *iterBegin))
				iterHigher = iterBegin;
		}
	}
	else if (tCompare(*iterHigher, tData))
		++iterHigher;
	return iterHigher;
}


/////////////////////////////////////////////////////////////////////
// CSet::lower_and_higher

template <class T, class TCompare, bool bUseCustomAllocator>
void CSet<T, TCompare, bUseCustomAllocator>::lower_and_higher(const T& tData, iterator& iterLower, iterator& iterHigher) const
{
	TCompare tCompare;

	iterLower = lower(tData);
	iterHigher = iterLower;
	if (iterHigher == end( ))
	{
		if (m_uiSize > 0)
		{
			iterator iterBegin = begin( );
			if (tCompare(tData, *iterBegin))
				iterHigher = iterBegin;
		}
	}
	else if (tCompare(*iterHigher, tData))
		++iterHigher;
}


/////////////////////////////////////////////////////////////////////
// CSet::Allocate

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CSet<T, TCompare, bUseCustomAllocator>::CNodeReference CSet<T, TCompare, bUseCustomAllocator>::Allocate(const T& tData, CNodeReference pParent)
{
	CNodeReference cReturn = m_cPool.GrabBlock( );
	new (m_cPool.ResolveBlock(cReturn)) CNode(tData, pParent);
	return cReturn;
}


/////////////////////////////////////////////////////////////////////
// CSet::Deallocate

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE void CSet<T, TCompare, bUseCustomAllocator>::Deallocate(CNodeReference pData)
{
	Ptr(pData)->~CNode( );
	m_cPool.ReleaseBlock(pData);
}


/////////////////////////////////////////////////////////////////////
// CSet::Ptr

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CSet<T, TCompare, bUseCustomAllocator>::CNode* CSet<T, TCompare, bUseCustomAllocator>::Ptr(CNodeReference pNode) const 
{ 
	return (CNode*)m_cPool.ResolveBlock(pNode); 
}


/////////////////////////////////////////////////////////////////////
// CSet::iterator::iterator

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE CSet<T, TCompare, bUseCustomAllocator>::iterator::iterator(CNodeReference pNode, const CPool* pPool) : 
	m_pNode(pNode),
	m_pPool(pPool)
{
}


/////////////////////////////////////////////////////////////////////
// CSet::iterator::operator ==

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE bool CSet<T, TCompare, bUseCustomAllocator>::iterator::operator == (const iterator& cRight) const 
{ 
	return (Ptr( ) == cRight.Ptr( )); 
}


/////////////////////////////////////////////////////////////////////
// CSet::iterator::operator !=

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE bool CSet<T, TCompare, bUseCustomAllocator>::iterator::operator != (const iterator& cRight) const 
{ 
	return (Ptr( ) != cRight.Ptr( ));
}


/////////////////////////////////////////////////////////////////////
// CSet::iterator::operator ++

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CSet<T, TCompare, bUseCustomAllocator>::iterator& CSet<T, TCompare, bUseCustomAllocator>::iterator::operator ++ (void) 
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
// CSet::iterator::operator --

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CSet<T, TCompare, bUseCustomAllocator>::iterator& CSet<T, TCompare, bUseCustomAllocator>::iterator::operator -- (void) 
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
// CSet::iterator::operator *

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE const T& CSet<T, TCompare, bUseCustomAllocator>::iterator::operator * (void) const
{
	return Ptr( )->m_tData;
}


/////////////////////////////////////////////////////////////////////
// CSet::iterator::operator ->

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE T* CSet<T, TCompare, bUseCustomAllocator>::iterator::operator -> (void) const
{
	return &(Ptr( )->m_tData);
}


/////////////////////////////////////////////////////////////////////
// CSet::iterator::Ptr

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE typename CSet<T, TCompare, bUseCustomAllocator>::CNode* CSet<T, TCompare, bUseCustomAllocator>::iterator::Ptr(void) const 
{ 
	return (m_pPool ? (CNode*)m_pPool->ResolveBlock(m_pNode) : NULL);
}



/////////////////////////////////////////////////////////////////////
// CSet::CNode::CNode

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE CSet<T, TCompare, bUseCustomAllocator>::CNode::CNode(const T& tData, CNodeReference pParent) :
	m_tData(tData),
	m_pLeft(NULL),
	m_pRight(NULL),
	m_pParent(pParent),
	m_uiLevel(0)
{
}


/////////////////////////////////////////////////////////////////////
// CSet::CNode::DeleteChildren

template <class T, class TCompare, bool bUseCustomAllocator>
ST_INLINE void CSet<T, TCompare, bUseCustomAllocator>::CNode::DeleteChildren(CSet* pSet)
{	
	if (m_pLeft != NULL)
	{
		((CNode*)pSet->m_cPool.ResolveBlock(m_pLeft))->DeleteChildren(pSet);
		pSet->Deallocate(m_pLeft);
	}
	m_pLeft = NULL;

	if (m_pRight != NULL)
	{
		((CNode*)pSet->m_cPool.ResolveBlock(m_pRight))->DeleteChildren(pSet);
		pSet->Deallocate(m_pRight);
	}
	m_pRight = NULL;
}


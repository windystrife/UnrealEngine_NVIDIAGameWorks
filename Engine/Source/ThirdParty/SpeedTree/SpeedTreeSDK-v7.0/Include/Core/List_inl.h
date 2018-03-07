///////////////////////////////////////////////////////////////////////  
//	List.inl
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
// CList::CList

template <class T, bool bUseCustomAllocator>
inline CList<T, bUseCustomAllocator>::CList(void) :
	m_pStart(NULL),
	m_pEnd(NULL),
	m_uiSize(0)
{
}


/////////////////////////////////////////////////////////////////////
// CList::CList

template <class T, bool bUseCustomAllocator>
inline CList<T, bUseCustomAllocator>::CList(const CList& cRight) :
	m_pStart(NULL),
	m_pEnd(NULL),
	m_uiSize(0)
{
	*this = cRight;
}


/////////////////////////////////////////////////////////////////////
// CList::~CList

template <class T, bool bUseCustomAllocator>
inline CList<T, bUseCustomAllocator>::~CList(void)
{
	clear( );
}


/////////////////////////////////////////////////////////////////////
// CList::clear

template <class T, bool bUseCustomAllocator>
inline void CList<T, bUseCustomAllocator>::clear(void)
{
	CNode* pCurrent = m_pStart;
	while (pCurrent != NULL)
	{
		CNode* pToDelete = pCurrent;
		pCurrent = pCurrent->m_pNext;
		Deallocate(pToDelete);
	}

	m_pStart = m_pEnd = NULL;
	m_uiSize = 0;
}


/////////////////////////////////////////////////////////////////////
// CList::empty

template <class T, bool bUseCustomAllocator>
inline bool CList<T, bUseCustomAllocator>::empty(void) const
{
	return !m_uiSize;
}


/////////////////////////////////////////////////////////////////////
// CList::size

template <class T, bool bUseCustomAllocator>
inline size_t CList<T, bUseCustomAllocator>::size(void) const
{
	return m_uiSize;
}


/////////////////////////////////////////////////////////////////////
// CList::push_back

template <class T, bool bUseCustomAllocator>
inline void CList<T, bUseCustomAllocator>::push_back(const T& tData)
{
	if (m_pStart == NULL)
	{
		m_pStart = Allocate(tData, NULL, NULL);
		m_pEnd = m_pStart;
	}
	else
	{
		m_pEnd->m_pNext = Allocate(tData, NULL, m_pEnd);
		m_pEnd = m_pEnd->m_pNext;
	}
	++m_uiSize;
}


/////////////////////////////////////////////////////////////////////
// CList::push_front

template <class T, bool bUseCustomAllocator>
inline void CList<T, bUseCustomAllocator>::push_front(const T& tData)
{
	if (m_pStart == NULL)
	{
		m_pStart = Allocate(tData, NULL, NULL);
		m_pEnd = m_pStart;
	}
	else
	{
		m_pStart = Allocate(tData, m_pStart, NULL);
		m_pStart->m_pNext->m_pPrevious = m_pStart;
	}
	++m_uiSize;
}


/////////////////////////////////////////////////////////////////////
// CList::pop_back

template <class T, bool bUseCustomAllocator>
inline void CList<T, bUseCustomAllocator>::pop_back(void)
{
	assert(m_pEnd != NULL);

	if (m_pEnd->m_pPrevious == NULL)
	{
		Deallocate(m_pEnd);
		m_pStart = m_pEnd = NULL;
	}
	else
	{
		m_pEnd = m_pEnd->m_pPrevious;
		Deallocate(m_pEnd->m_pNext);
		m_pEnd->m_pNext = NULL;
	}
	--m_uiSize;
}


/////////////////////////////////////////////////////////////////////
// CList::pop_front

template <class T, bool bUseCustomAllocator>
inline void CList<T, bUseCustomAllocator>::pop_front(void)
{
	assert(m_pStart != NULL);

	if (m_pStart->m_pNext == NULL)
	{
		Deallocate(m_pStart);
		m_pStart = m_pEnd = NULL;
	}
	else
	{
		m_pStart = m_pStart->m_pNext;
		Deallocate(m_pStart->m_pPrevious);
		m_pStart->m_pPrevious = NULL;
	}
	--m_uiSize;
}


/////////////////////////////////////////////////////////////////////
// CList::front

template <class T, bool bUseCustomAllocator>
inline T& CList<T, bUseCustomAllocator>::front(void)
{
	assert(m_pStart != NULL);
	return m_pStart->m_tData;
}


/////////////////////////////////////////////////////////////////////
// CList::back

template <class T, bool bUseCustomAllocator>
inline T& CList<T, bUseCustomAllocator>::back(void)
{
	assert(m_pEnd != NULL);
	return m_pEnd->m_tData;
}


/////////////////////////////////////////////////////////////////////
// CList::operator =

template <class T, bool bUseCustomAllocator>
inline CList<T, bUseCustomAllocator>& CList<T, bUseCustomAllocator>::operator = (const CList<T, bUseCustomAllocator>& cRight)
{
	CNode* pCurrent = m_pStart;
	for (const_iterator iter = cRight.begin( ); iter != cRight.end( ); ++iter)
	{
		if (pCurrent == NULL)
			push_back(*iter);
		else
		{
			pCurrent->m_tData = *iter;
			pCurrent = pCurrent->m_pNext;
		}
	}

	return (*this);
}


/////////////////////////////////////////////////////////////////////
// CList::begin

template <class T, bool bUseCustomAllocator>
inline typename CList<T, bUseCustomAllocator>::iterator CList<T, bUseCustomAllocator>::begin(void) 
{ 
	return iterator(m_pStart); 
}


/////////////////////////////////////////////////////////////////////
// CList::end

template <class T, bool bUseCustomAllocator>
inline typename CList<T, bUseCustomAllocator>::iterator CList<T, bUseCustomAllocator>::end(void) 
{ 
	return iterator( ); 
}


/////////////////////////////////////////////////////////////////////
// CList::begin

template <class T, bool bUseCustomAllocator>
inline typename CList<T, bUseCustomAllocator>::const_iterator CList<T, bUseCustomAllocator>::begin(void) const
{ 
	return const_iterator(m_pStart); 
}


/////////////////////////////////////////////////////////////////////
// CList::end

template <class T, bool bUseCustomAllocator>
inline typename CList<T, bUseCustomAllocator>::const_iterator CList<T, bUseCustomAllocator>::end(void) const
{ 
	return const_iterator( ); 
}


/////////////////////////////////////////////////////////////////////
// CList::erase

template <class T, bool bUseCustomAllocator>
inline typename CList<T, bUseCustomAllocator>::iterator CList<T, bUseCustomAllocator>::erase(iterator iterWhere)
{
	#ifdef SPEEDTREE_ITERATOR_DEBUGGING
		// make sure iterator is part of this list
		assert(iterWhere.m_pNode != NULL);
		CNode* pRoot = iterWhere.m_pNode;
		while (pRoot->m_pPrevious != NULL)
			pRoot = pRoot->m_pPrevious;
		assert(pRoot == m_pStart);
	#endif

	if (iterWhere.m_pNode->m_pPrevious != NULL)
		iterWhere.m_pNode->m_pPrevious->m_pNext = iterWhere.m_pNode->m_pNext;
	if (iterWhere.m_pNode->m_pNext != NULL)
		iterWhere.m_pNode->m_pNext->m_pPrevious = iterWhere.m_pNode->m_pPrevious;

	iterator iterReturn(iterWhere.m_pNode->m_pNext);
	Deallocate(iterWhere.m_pNode);

	return iterReturn;
}


/////////////////////////////////////////////////////////////////////
// CList::insert

template <class T, bool bUseCustomAllocator>
inline typename CList<T, bUseCustomAllocator>::iterator CList<T, bUseCustomAllocator>::insert(iterator iterWhere, const T& tData)
{
	#ifdef SPEEDTREE_ITERATOR_DEBUGGING
		// make sure iterator is part of this list
		if (iterWhere.m_pNode != NULL)
		{
			CNode* pRoot = iterWhere.m_pNode;
			while (pRoot->m_pPrevious != NULL)
				pRoot = pRoot->m_pPrevious;
			assert(pRoot == m_pStart);
		}
	#endif

	if (m_pStart == NULL)
	{
		m_pStart = m_pEnd = Allocate(tData, NULL, NULL);
		return iterator(m_pStart);
	}
	
	if (iterWhere.m_pNode == NULL)
	{
		m_pEnd = m_pEnd->m_pNext = Allocate(tData, NULL, m_pEnd);
		return iterator(m_pEnd);
	}
	
	CNode* pNewNode = Allocate(tData, iterWhere.m_pNode, iterWhere.m_pNode->m_pPrevious);
	if (iterWhere.m_pNode->m_pPrevious == NULL)
		m_pStart = pNewNode;
	else
		iterWhere.m_pNode->m_pPrevious->m_pNext = pNewNode;
	iterWhere.m_pNode->m_pPrevious = pNewNode;
	++m_uiSize;

	return iterator(pNewNode);
}


/////////////////////////////////////////////////////////////////////
// CList::CNode::CNode

template <class T, bool bUseCustomAllocator>
inline CList<T, bUseCustomAllocator>::CNode::CNode(const T& tData, CNode* pNext, CNode* pPrevious) :
	m_tData(tData),
	m_pNext(pNext),
	m_pPrevious(pPrevious)
{
}


/////////////////////////////////////////////////////////////////////
// CList::iterator_base::iterator_base

template <class T, bool bUseCustomAllocator>
inline CList<T, bUseCustomAllocator>::iterator_base::iterator_base(CNode* pNode) : 
	m_pNode(pNode) 
{ 
}


/////////////////////////////////////////////////////////////////////
// CList::iterator::operator T*

template <class T, bool bUseCustomAllocator>
inline CList<T, bUseCustomAllocator>::iterator::operator T* (void) 
{ 
	assert(iterator_base::m_pNode != NULL);

	return &iterator_base::m_pNode->m_tData; 
}


/////////////////////////////////////////////////////////////////////
// CList::const_iterator::operator T*

template <class T, bool bUseCustomAllocator>
inline CList<T, bUseCustomAllocator>::const_iterator::operator T const* (void) 
{ 
	assert(iterator_base::m_pNode != NULL);

	return &iterator_base::m_pNode->m_tData; 
}


/////////////////////////////////////////////////////////////////////
// CList::iterator::operator ->

template <class T, bool bUseCustomAllocator>
T* CList<T, bUseCustomAllocator>::iterator::operator -> (void) 
{ 
	return &iterator_base::m_pNode->m_tData; 
}


/////////////////////////////////////////////////////////////////////
// CList::const_iterator::operator ->

template <class T, bool bUseCustomAllocator>
T const* CList<T, bUseCustomAllocator>::const_iterator::operator -> (void) 
{ 
	return &iterator_base::m_pNode->m_tData; 
}


/////////////////////////////////////////////////////////////////////
// CList::iterator_base::operator ++

template <class T, bool bUseCustomAllocator>
inline typename CList<T, bUseCustomAllocator>::iterator_base& CList<T, bUseCustomAllocator>::iterator_base::operator ++ (void) 
{ 
	assert(m_pNode != NULL); 
	m_pNode = m_pNode->m_pNext; 

	return *(this);
}


/////////////////////////////////////////////////////////////////////
// CList::iterator_base::operator --

template <class T, bool bUseCustomAllocator>
inline typename CList<T, bUseCustomAllocator>::iterator_base& CList<T, bUseCustomAllocator>::iterator_base::operator -- (void) 
{ 
	assert(m_pNode != NULL); 
	m_pNode = m_pNode->m_pPrevious;

	return *(this);
}


/////////////////////////////////////////////////////////////////////
// CList::iterator_base::operator ==

template <class T, bool bUseCustomAllocator>
inline bool CList<T, bUseCustomAllocator>::iterator_base::operator == (const iterator_base& cRight) const 
{ 
	return (m_pNode == cRight.m_pNode); 
}


/////////////////////////////////////////////////////////////////////
// CList::iterator_base::operator !=

template <class T, bool bUseCustomAllocator>
inline bool CList<T, bUseCustomAllocator>::iterator_base::operator != (const iterator_base& cRight) const 
{ 
	return (m_pNode != cRight.m_pNode); 
}


/////////////////////////////////////////////////////////////////////
// CList::insert_sorted

template <class T, bool bUseCustomAllocator>
inline typename CList<T, bUseCustomAllocator>::iterator CList<T, bUseCustomAllocator>::insert_sorted(const T& tData)
{
	CNode* pCurrent = m_pStart;
	while (pCurrent != NULL && pCurrent->m_tData < tData)	
		pCurrent = pCurrent->m_pNext;
			
	return insert(pCurrent, tData);
}


/////////////////////////////////////////////////////////////////////
// CList::sort

class CListComparator
{
public:
	template <class T> bool operator () (const T& pOne, const T& pTwo) const { return (pOne->m_tData < pTwo->m_tData); }
};

template <class T, bool bUseCustomAllocator>
inline void CList<T, bUseCustomAllocator>::sort(void)
{
	if (m_uiSize < 2)
		return;
	
	// make random access array
	CArray<CNode*, bUseCustomAllocator> aIndirect;
	aIndirect.reserve(m_uiSize);
	CNode* pCurrent = m_pStart;
	while (pCurrent != NULL)
	{
		aIndirect.push_back(pCurrent);
		pCurrent = pCurrent->m_pNext;
	}
	
	// sort
	aIndirect.sort(CListComparator( ));
	
	// reshuffle list to new shell order
	for (unsigned int i = 1; i < m_uiSize - 1; ++i)
	{
		aIndirect[i]->m_pPrevious = aIndirect[i - 1];
		aIndirect[i]->m_pNext = aIndirect[i + 1];
	}
	m_pStart = aIndirect.front( );
	m_pStart->m_pPrevious = NULL;
	m_pStart->m_pNext = aIndirect[1];
	m_pEnd = aIndirect.back( );
	m_pEnd->m_pNext = NULL;
	m_pEnd->m_pPrevious = aIndirect[m_uiSize - 2];
}


/////////////////////////////////////////////////////////////////////
// CList::Allocate

template <class T, bool bUseCustomAllocator>
inline typename CList<T, bUseCustomAllocator>::CNode* CList<T, bUseCustomAllocator>::Allocate(const T& tData, CNode* pNext, CNode* pPrevious)
{
	return 
	#ifndef SPEEDTREE_NO_ALLOCATORS 
		bUseCustomAllocator ? new ("CList") CNode(tData, pNext, pPrevious) :
	#endif 
		new CNode(tData, pNext, pPrevious);
}


/////////////////////////////////////////////////////////////////////
// CList::Deallocate

template <class T, bool bUseCustomAllocator>
inline void CList<T, bUseCustomAllocator>::Deallocate(CNode* pData)
{
	#ifndef SPEEDTREE_NO_ALLOCATORS 
		bUseCustomAllocator ? st_delete<CNode>(pData) : 
	#endif 
		delete pData;
}


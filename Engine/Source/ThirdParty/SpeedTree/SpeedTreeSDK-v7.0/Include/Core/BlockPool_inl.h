///////////////////////////////////////////////////////////////////////  
//	ObjectPool.inl
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
// CBlockPool::CBlockPool

template <bool bUseCustomAllocator>
ST_INLINE CBlockPool<bUseCustomAllocator>::CBlockPool(size_t uiBlockSize, size_t uiNum) :
	m_pData(NULL),
	m_pFreeLocations(NULL),
	m_uiSize(0),
	m_uiCurrent(0),
	m_uiBlockSize(uiBlockSize)
{
	resize(uiNum);
}


///////////////////////////////////////////////////////////////////////
// CBlockPool::~CBlockPool

template <bool bUseCustomAllocator>
ST_INLINE CBlockPool<bUseCustomAllocator>::~CBlockPool(void)
{
	clear( );
}


///////////////////////////////////////////////////////////////////////
// CBlockPool::clear

template <bool bUseCustomAllocator>
ST_INLINE void CBlockPool<bUseCustomAllocator>::clear(bool bForce)
{
	if (!bForce)
		assert(m_uiCurrent == m_uiSize);

	#ifndef SPEEDTREE_NO_ALLOCATORS
		bUseCustomAllocator ? st_delete_array<char>(m_pData) :
	#endif
		delete[] m_pData;

	#ifndef SPEEDTREE_NO_ALLOCATORS
		bUseCustomAllocator ? st_delete_array<size_t>(m_pFreeLocations) :
	#endif
		delete[] m_pFreeLocations;

	m_pData = NULL;
	m_pFreeLocations = NULL;
	m_uiSize = 0;
	m_uiCurrent = 0;
}


///////////////////////////////////////////////////////////////////////
// CBlockPool::size

template <bool bUseCustomAllocator>
ST_INLINE size_t CBlockPool<bUseCustomAllocator>::size(void) const
{
	return m_uiSize;
}


///////////////////////////////////////////////////////////////////////
// CBlockPool::Resize

template <bool bUseCustomAllocator>
ST_INLINE void	CBlockPool<bUseCustomAllocator>::resize(size_t uiSize)
{
	if (uiSize <= m_uiSize)
		return;

	assert(uiSize > m_uiSize);

	char* pNewData = 
		#ifndef SPEEDTREE_NO_ALLOCATORS
			bUseCustomAllocator ? st_new_array<char>(uiSize * m_uiBlockSize + 4, "CBlockPool") :
		#endif
			new char[uiSize * m_uiBlockSize + 4];

	size_t* pNewFreeLocations = 
		#ifndef SPEEDTREE_NO_ALLOCATORS
			bUseCustomAllocator ? st_new_array<size_t>(uiSize, "CBlockPool") :
		#endif
			new size_t[uiSize];

	if (m_pData != NULL)
	{
		memcpy(pNewData, m_pData, m_uiSize * m_uiBlockSize);
		#ifndef SPEEDTREE_NO_ALLOCATORS
			bUseCustomAllocator ? st_delete_array<char>(m_pData) :
		#endif
			delete[] m_pData;
	}

	if (m_pFreeLocations != NULL)
	{
		if (m_uiCurrent > 0)
			memcpy(pNewFreeLocations, m_pFreeLocations, m_uiCurrent * sizeof(size_t));
		#ifndef SPEEDTREE_NO_ALLOCATORS
			bUseCustomAllocator ? st_delete_array<size_t>(m_pFreeLocations) :
		#endif
			delete[] m_pFreeLocations;
	}

	for (size_t i = m_uiSize; i < uiSize; ++i)
		pNewFreeLocations[m_uiCurrent++] = i * m_uiBlockSize + 4; // +4 is so we can still check to NULL

	m_pData = pNewData;
	m_pFreeLocations = pNewFreeLocations;
	m_uiSize = uiSize;
}


///////////////////////////////////////////////////////////////////////
// CBlockPool::GrabBlock

template <bool bUseCustomAllocator>
ST_INLINE typename CBlockPool<bUseCustomAllocator>::CReference CBlockPool<bUseCustomAllocator>::GrabBlock(void)
{
	if (m_uiCurrent == 0)
		resize(m_uiSize * 2 + 1);

	return (CReference)m_pFreeLocations[--m_uiCurrent];
}


///////////////////////////////////////////////////////////////////////
// CBlockPool::ReleaseBlock

template <bool bUseCustomAllocator>
ST_INLINE void	CBlockPool<bUseCustomAllocator>::ReleaseBlock(CReference& cRef)
{
	m_pFreeLocations[m_uiCurrent++] = (size_t)cRef;
	cRef = NULL;
}


///////////////////////////////////////////////////////////////////////
// CBlockPool::ResolveBlock

template <bool bUseCustomAllocator>
ST_INLINE void* CBlockPool<bUseCustomAllocator>::ResolveBlock(const CReference& cRef) const
{
	assert((size_t)cRef < (m_uiSize * m_uiBlockSize + 4));
	return (cRef ? m_pData + (size_t)cRef : NULL);
}


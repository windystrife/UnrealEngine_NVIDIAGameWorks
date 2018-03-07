///////////////////////////////////////////////////////////////////////  
//	Array.inl
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
// CArray::CArray

template <class T, bool bUseCustomAllocator>
ST_INLINE CArray<T, bUseCustomAllocator>::CArray( ) : 
	m_pData(NULL),
	m_uiSize(0),
	m_uiDataSize(0),
	m_pHeapDesc(NULL),
	m_bExternalMemory(false)
{
}


/////////////////////////////////////////////////////////////////////
// CArray::CArray

template <class T, bool bUseCustomAllocator>
ST_INLINE CArray<T, bUseCustomAllocator>::CArray(size_t uiSize, const T& tDefault) : 
	m_pData(NULL),
	m_uiSize(0),
	m_uiDataSize(0),
	m_pHeapDesc(NULL),
	m_bExternalMemory(false)
{
	resize(uiSize, tDefault);
}


/////////////////////////////////////////////////////////////////////
// CArray::CArray

template <class T, bool bUseCustomAllocator>
ST_INLINE CArray<T, bUseCustomAllocator>::CArray(const CArray<T, bUseCustomAllocator>& cCopy) :
	m_pData(NULL),
	m_uiSize(0),
	m_uiDataSize(0),
	m_bExternalMemory(false)
{
	*this = cCopy;
}


/////////////////////////////////////////////////////////////////////
// CArray::~CArray

template <class T, bool bUseCustomAllocator>
ST_INLINE CArray<T, bUseCustomAllocator>::~CArray( )
{
	if (m_bExternalMemory)
		SetExternalMemory(NULL, 0);
	clear( );
}


/////////////////////////////////////////////////////////////////////
// CArray::clear

template <class T, bool bUseCustomAllocator>
ST_INLINE void CArray<T, bUseCustomAllocator>::clear(void)
{
	if (m_bExternalMemory)
	{
		m_uiSize = 0;
	}
	else
	{
		Deallocate(m_pData);
		m_pData = NULL;
		m_uiSize = m_uiDataSize = 0;
	}
}


/////////////////////////////////////////////////////////////////////
// CArray::empty

template <class T, bool bUseCustomAllocator>
ST_INLINE bool CArray<T, bUseCustomAllocator>::empty(void) const
{
	return !m_uiSize;
}


/////////////////////////////////////////////////////////////////////
// CArray::size

template <class T, bool bUseCustomAllocator>
ST_INLINE size_t CArray<T, bUseCustomAllocator>::size(void) const
{
	return m_uiSize;
}


/////////////////////////////////////////////////////////////////////
// CArray::capacity

template <class T, bool bUseCustomAllocator>
ST_INLINE size_t CArray<T, bUseCustomAllocator>::capacity(void) const
{
	return m_uiDataSize;
}


/////////////////////////////////////////////////////////////////////
// CArray::resize

template <class T, bool bUseCustomAllocator>
ST_INLINE bool CArray<T, bUseCustomAllocator>::resize(size_t uiSize)
{
	if (reserve(uiSize))
	{
		m_uiSize = uiSize;
		return true;
	}

	m_uiSize = m_uiDataSize;

	return false;
}


/////////////////////////////////////////////////////////////////////
// CArray::resize

template <class T, bool bUseCustomAllocator>
ST_INLINE bool CArray<T, bUseCustomAllocator>::resize(size_t uiSize, const T& tDefault)
{
	bool bReturn = false;
	size_t uiEnd = m_uiDataSize;
	if (reserve(uiSize))
	{
		uiEnd = uiSize;
		bReturn = true;
	}

	for (size_t i = m_uiSize; i < uiEnd; ++i)
		m_pData[i] = tDefault;
	m_uiSize = uiEnd;

	return bReturn;
}


/////////////////////////////////////////////////////////////////////
// CArray::reserve

template <class T, bool bUseCustomAllocator>
ST_INLINE bool CArray<T, bUseCustomAllocator>::reserve(size_t uiSize)
{
	if (m_bExternalMemory)
	{
		return (uiSize <= m_uiDataSize);
	}

	if (uiSize > m_uiDataSize)
	{
		T* pNewData = Allocate(uiSize);
		if (m_uiSize > 0)
		{
			T* pNew = pNewData;
			T* pOld = m_pData;
			for (size_t i = 0; i < m_uiSize; ++pNew, ++pOld, ++i)
				*pNew = *pOld;
		}

		Deallocate(m_pData);
		m_pData = pNewData;
		m_uiDataSize = uiSize;
	}

	return true;
}


/////////////////////////////////////////////////////////////////////
// CArray::push_back

template <class T, bool bUseCustomAllocator>
ST_INLINE bool CArray<T, bUseCustomAllocator>::push_back(const T& tNew)
{
	bool bReturn = true;

	if (m_bExternalMemory)
	{
		if (m_uiSize < m_uiDataSize)
			m_pData[m_uiSize++] = tNew;
		else
			bReturn = false;
	}
	else 
	{
		if (m_uiSize == m_uiDataSize)
		{
			if (m_uiDataSize < 8)
				m_uiDataSize = 8;
			reserve(m_uiDataSize * 2 + 1);
		}
		m_pData[m_uiSize++] = tNew;
	}

	return bReturn;
}


/////////////////////////////////////////////////////////////////////
// CArray::pop_back

template <class T, bool bUseCustomAllocator>
ST_INLINE void CArray<T, bUseCustomAllocator>::pop_back(void)
{
	if (m_uiSize > 0)
		--m_uiSize;
}


/////////////////////////////////////////////////////////////////////
// CArray::clip

template <class T, bool bUseCustomAllocator>
ST_INLINE void CArray<T, bUseCustomAllocator>::clip(void)
{
	if (!m_bExternalMemory && m_uiDataSize > m_uiSize)
	{
		if (m_uiSize > 0)
		{
			T* pNewData = Allocate(m_uiSize);
			T* pNew = pNewData;
			T* pOld = m_pData;
			for (size_t i = 0; i < m_uiSize; ++pNew, ++pOld, ++i)
				*pNew = *pOld;
			Deallocate(m_pData);
			m_pData = pNewData;
			m_uiDataSize = m_uiSize;
		}
		else
			clear( );
	}
}


/////////////////////////////////////////////////////////////////////
// CArray::erase_all

template <class T, bool bUseCustomAllocator>
ST_INLINE void CArray<T, bUseCustomAllocator>::erase_all(const T& tErase)
{
	if (m_uiSize > 0)
	{
		iterator iterTest = m_pData + m_uiSize;
		while (--iterTest >= m_pData)
		{
			if (*iterTest == tErase)
				erase(iterTest);
		}
	}
}


/////////////////////////////////////////////////////////////////////
// CArray::front

template <class T, bool bUseCustomAllocator>
ST_INLINE T& CArray<T, bUseCustomAllocator>::front(void)
{
	return m_pData[0];
}


/////////////////////////////////////////////////////////////////////
// CArray::back

template <class T, bool bUseCustomAllocator>
ST_INLINE T& CArray<T, bUseCustomAllocator>::back(void)
{
	return m_pData[m_uiSize - 1];
}


/////////////////////////////////////////////////////////////////////
// CArray::operator []

template <class T, bool bUseCustomAllocator>
ST_INLINE T& CArray<T, bUseCustomAllocator>::operator [] (size_t uiIndex)
{
	st_assert(uiIndex < m_uiSize, "CArray index out of range");

	return m_pData[uiIndex];
}


/////////////////////////////////////////////////////////////////////
// CArray::operator []

template <class T, bool bUseCustomAllocator>
ST_INLINE const T& CArray<T, bUseCustomAllocator>::operator [] (size_t uiIndex) const
{
	st_assert(uiIndex < m_uiSize, "CArray index out of range");

	return m_pData[uiIndex];
}


/////////////////////////////////////////////////////////////////////
// CArray::at

template <class T, bool bUseCustomAllocator>
ST_INLINE T& CArray<T, bUseCustomAllocator>::at(size_t uiIndex)
{
	st_assert(uiIndex < m_uiSize, "CArray index out of range");

	return m_pData[uiIndex];
}


/////////////////////////////////////////////////////////////////////
// CArray::at

template <class T, bool bUseCustomAllocator>
ST_INLINE const T& CArray<T, bUseCustomAllocator>::at(size_t uiIndex) const
{
	st_assert(uiIndex < m_uiSize, "CArray index out of range");

	return m_pData[uiIndex];
}


/////////////////////////////////////////////////////////////////////
// CArray::operator =

template <class T, bool bUseCustomAllocator>
ST_INLINE CArray<T, bUseCustomAllocator>& CArray<T, bUseCustomAllocator>::operator = (const CArray<T, bUseCustomAllocator>& cRight)
{
    if (!m_pHeapDesc)
	    m_pHeapDesc = cRight.m_pHeapDesc;

	if (m_bExternalMemory)
	{
		m_uiSize = cRight.m_uiSize;
		if (m_uiSize > m_uiDataSize)
			m_uiSize = m_uiDataSize;
	}
	else
	{
		if (cRight.m_uiSize > m_uiDataSize)
		{
			T* pNewData = Allocate(cRight.m_uiSize);
			Deallocate(m_pData);
			m_pData = pNewData;
			m_uiDataSize = cRight.m_uiSize;
		}
		m_uiSize = cRight.m_uiSize;
	}

	if (m_uiSize > 0)
	{
		T* pNew = m_pData;
		T* pOld = cRight.m_pData;
		for (size_t i = 0; i < size_t(m_uiSize); ++pNew, ++pOld, ++i)
			*pNew = *pOld;
	}

	return *this;
}


/////////////////////////////////////////////////////////////////////
// CArray::begin

template <class T, bool bUseCustomAllocator>
ST_INLINE T* CArray<T, bUseCustomAllocator>::begin(void)
{
	return m_pData;
}


/////////////////////////////////////////////////////////////////////
// CArray::end

template <class T, bool bUseCustomAllocator>
ST_INLINE T* CArray<T, bUseCustomAllocator>::end(void)
{
	return (m_pData + m_uiSize);
}


/////////////////////////////////////////////////////////////////////
// CArray::begin

template <class T, bool bUseCustomAllocator>
ST_INLINE T const* CArray<T, bUseCustomAllocator>::begin(void) const
{
	return m_pData;
}


/////////////////////////////////////////////////////////////////////
// CArray::end

template <class T, bool bUseCustomAllocator>
ST_INLINE T const* CArray<T, bUseCustomAllocator>::end(void) const
{
	return (m_pData + m_uiSize);
}


/////////////////////////////////////////////////////////////////////
// CArray::erase

template <class T, bool bUseCustomAllocator>
ST_INLINE typename CArray<T, bUseCustomAllocator>::iterator CArray<T, bUseCustomAllocator>::erase(iterator iterWhere)
{
	// make sure iterator is part of this list
	st_assert(m_pData != NULL, "CArray::erase() called before CArray was propertly initialized");
	st_assert(iterWhere >= m_pData && iterWhere < m_pData + m_uiSize, "CArray iterator out of bounds");

	if (iterWhere == m_pData + m_uiSize - 1)
	{
		// at the end, so just decrement size
		--m_uiSize;
	}
	else
	{
		// move the erased one to the end so destructors are called correctly
		char achTemp[sizeof(T)];
		memmove(achTemp, iterWhere, sizeof(T));
		memmove(iterWhere, iterWhere + 1, (m_uiSize - (iterWhere - m_pData) - 1) * sizeof(T));
		--m_uiSize;
		memmove(m_pData + m_uiSize, achTemp, sizeof(T));
	}

	return iterWhere;
}


/////////////////////////////////////////////////////////////////////
// CArray::insert

template <class T, bool bUseCustomAllocator>
ST_INLINE typename CArray<T, bUseCustomAllocator>::iterator CArray<T, bUseCustomAllocator>::insert(iterator iterWhere, const T& tData)
{
	// make sure iterator is part of this list
	#ifdef SPEEDTREE_ITERATOR_DEBUGGING
		assert((m_pData != NULL && iterWhere != NULL) || (m_pData == NULL && iterWhere == NULL));
		if (m_pData != NULL)
		{
			assert(iterWhere >= m_pData);
			assert(iterWhere < m_pData + m_uiSize + 1);
		}
	#endif

	// remember iterator position
	size_t uiIndex = iterWhere - m_pData;

	// this will ensure the array is big enough, etc.
	if (push_back(tData))
	{
		if (m_uiSize > 1)
		{
			// move the new one from the end so destructors are called correctly
			char achTemp[sizeof(T)];
			memmove(achTemp, static_cast<void*>(m_pData + m_uiSize - 1), sizeof(T));
			memmove(static_cast<void*>(m_pData + uiIndex + 1), static_cast<void*>(m_pData + uiIndex), (m_uiSize - uiIndex - 1) * sizeof(T));
			memmove(static_cast<void*>(m_pData + uiIndex), achTemp, sizeof(T));
		}

		return (m_pData + uiIndex);
	}

	return NULL;
}


/////////////////////////////////////////////////////////////////////
// CArray::lower

template <class T, bool bUseCustomAllocator>
ST_INLINE typename CArray<T, bUseCustomAllocator>::iterator CArray<T, bUseCustomAllocator>::lower(const T& tData)
{
	if (m_uiSize == 0 || tData < m_pData[0])
		return m_pData + m_uiSize;

	size_t uiWidth = m_uiSize / 2;
	iterator iterStart = m_pData;
	iterator iterEnd = m_pData + m_uiSize;
	
	while (uiWidth > 0)
	{
		iterator iterMiddle = iterStart + uiWidth;
		if (tData < *iterMiddle)
			iterEnd = iterMiddle;
		else
			iterStart = iterMiddle;
		uiWidth = (iterEnd - iterStart) / 2;
	}
	
	return iterStart;
}


/////////////////////////////////////////////////////////////////////
// CArray::higher

template <class T, bool bUseCustomAllocator>
ST_INLINE typename CArray<T, bUseCustomAllocator>::iterator CArray<T, bUseCustomAllocator>::higher(const T& tData)
{
	iterator iterBound = lower(tData);
	if (iterBound == end( ))
	{
		if (m_uiSize > 0 && tData < *m_pData)
			iterBound = m_pData;
	}
	else if (*iterBound < tData)
		iterBound++;
	return iterBound;
}


/////////////////////////////////////////////////////////////////////
// CArray::lower_and_higher

template <class T, bool bUseCustomAllocator>
void CArray<T, bUseCustomAllocator>::lower_and_higher(const T& tData, iterator& iterLower, iterator& iterHigher)
{
	iterLower = lower(tData);
	iterHigher = iterLower;
	if (iterHigher == end( ))
	{
		if (m_uiSize > 0 && tData < *m_pData)
			iterHigher = m_pData;
	}
	else if (*iterHigher < tData)
		iterHigher++;
}


/////////////////////////////////////////////////////////////////////
// CArray::insert_sorted

template <class T, bool bUseCustomAllocator>
ST_INLINE typename CArray<T, bUseCustomAllocator>::iterator CArray<T, bUseCustomAllocator>::insert_sorted(const T& tData)
{
	return insert(higher(tData), tData);		
}


/////////////////////////////////////////////////////////////////////
// CArray::insert_sorted_unique

template <class T, bool bUseCustomAllocator>
ST_INLINE typename CArray<T, bUseCustomAllocator>::iterator CArray<T, bUseCustomAllocator>::insert_sorted_unique(const T& tData)
{
	iterator iterLower;
	iterator iterHigher;
	lower_and_higher(tData, iterLower, iterHigher);

	if (iterLower == end( ) || iterLower != iterHigher)
		return insert(iterHigher, tData);
	return iterLower;	
}


/////////////////////////////////////////////////////////////////////
// Array sorting functors

class CArrayPointerSwap
{
public:
	template <class T> void operator () (T* pOne, T* pTwo, T* pTemp) const 
	{ 
		*pTemp = *pOne;
		*pOne = *pTwo;
		*pTwo = *pTemp;
	}
};

class CArrayPointerMemorySwap
{
public:
	template <class T> void operator () (T* pOne, T* pTwo, T* pTemp) const 
	{ 
		memmove(pTemp, pOne, sizeof(T));
		memmove(pOne, pTwo, sizeof(T));
		memmove(pTwo, pTemp, sizeof(T));
	}
};

class CArrayPointerCopy
{
public:
	template <class T> void operator () (T* pDest, T* pSrc) const { *pDest = *pSrc; }
};

class CArrayPointerMemoryCopy
{
public:
	template <class T> void operator () (T* pDest, T* pSrc) const { memmove(pDest, pSrc, sizeof(T)); }
};


/////////////////////////////////////////////////////////////////////
// CArray::ArrayQuickSort
	
template <class T, class CompareFunctor, class SwapFunctor, class CopyFunctor>
inline void ArrayQuickSort(T* pBegin, T* pEnd, const CompareFunctor& Compare, const SwapFunctor& Swap, const CopyFunctor& Copy, T* pTemp)
{
	size_t uiSize = pEnd - pBegin + 1;
	
	if (pEnd - pBegin < 16)
	{
		// insertion sort
		for (size_t i = 1; i < uiSize; ++i)
		{
			Copy(pTemp, &pBegin[i]);
			size_t j = i;
			for ( ; j > 0 && Compare(*pTemp, pBegin[j - 1]); --j)
				Copy(&pBegin[j], &pBegin[j - 1]);
			Copy(&pBegin[j], pTemp);
		}
	}
	else
	{
		// median of 3 pivot choice
		T* pMiddle = pBegin + uiSize / 2;
		if (Compare(*pMiddle, *pBegin))
			Swap(pMiddle, pBegin, pTemp);
		if (Compare(*pEnd, *pBegin))
			Swap(pEnd, pBegin, pTemp);
		if (Compare(*pEnd, *pMiddle))
			Swap(pEnd, pMiddle, pTemp);
		
		// position pivot
		T* pPivot = pEnd - 1;
		Swap(pMiddle, pPivot, pTemp);
		
		// partition
		T* pLow(pBegin);
		T* pHigh(pPivot);
		for ( ; ; )
		{
			while (Compare(*(++pLow), *pPivot)) ;
			while (Compare(*pPivot, *(--pHigh))) ;
			if (pLow < pHigh)
				Swap(pLow, pHigh, pTemp);
			else
				break;
		}
		
		// restore pivot
		Swap(pLow, pPivot, pTemp);
		
		// recursive sort
		ArrayQuickSort(pBegin, pLow - 1, Compare, Swap, Copy, pTemp);
		ArrayQuickSort(pLow + 1, pEnd, Compare, Swap, Copy, pTemp);
	}
}


/////////////////////////////////////////////////////////////////////
// CArray::sort

template <class T, bool bUseCustomAllocator>
template <class CompareFunctor>
ST_INLINE void CArray<T, bUseCustomAllocator>::sort(const CompareFunctor& Compare, bool bMemorySwap)
{
	if (m_uiSize < 2)
		return;
	
	if (bMemorySwap)
	{
		char achTemp[sizeof(T)];
		ArrayQuickSort(m_pData, m_pData + m_uiSize - 1, Compare, CArrayPointerMemorySwap( ), CArrayPointerMemoryCopy( ), (T*)achTemp);
	}
	else
	{
		T tTemp;
		ArrayQuickSort(m_pData, m_pData + m_uiSize - 1, Compare, CArrayPointerSwap( ), CArrayPointerCopy( ), &tTemp);
	}
}	


/////////////////////////////////////////////////////////////////////
// CArray::sort

template <class T, bool bUseCustomAllocator>
ST_INLINE void CArray<T, bUseCustomAllocator>::sort(bool bMemorySwap)
{
	if (m_uiSize < 2)
		return;
	
	if (bMemorySwap)
	{
		char achTemp[sizeof(T)];
		ArrayQuickSort(m_pData, m_pData + m_uiSize - 1, CDefaultArraySort( ), CArrayPointerMemorySwap( ), CArrayPointerMemoryCopy( ), (T*)achTemp);
	}
	else
	{
		T tTemp;
		ArrayQuickSort(m_pData, m_pData + m_uiSize - 1, CDefaultArraySort( ), CArrayPointerSwap( ), CArrayPointerCopy( ), &tTemp);
	}
}	
	

/////////////////////////////////////////////////////////////////////
// CArray::Allocate

template <class T, bool bUseCustomAllocator>
ST_INLINE T* CArray<T, bUseCustomAllocator>::Allocate(size_t uiSize)
{
	assert(!m_bExternalMemory);

	return 
	#ifndef SPEEDTREE_NO_ALLOCATORS
		bUseCustomAllocator ? st_new_array<T>(uiSize, m_pHeapDesc ? m_pHeapDesc : "CArray") :
	#endif 
		new T[uiSize];
}


/////////////////////////////////////////////////////////////////////
// CArray::Deallocate

template <class T, bool bUseCustomAllocator>
ST_INLINE void CArray<T, bUseCustomAllocator>::Deallocate(T* pData)
{ 
	assert(!m_bExternalMemory);
	#ifndef SPEEDTREE_NO_ALLOCATORS
		bUseCustomAllocator ? st_delete_array<T>(pData) :
	#endif
		delete[] pData;
}


/////////////////////////////////////////////////////////////////////
// CArray::SetExternalMemory

template <class T, bool bUseCustomAllocator>
ST_INLINE void CArray<T, bUseCustomAllocator>::SetExternalMemory(unsigned char* pMemory, size_t uiSize)
{
	clear( );

	if (m_bExternalMemory)
	{
		// call destructors
		for (size_t i = 0; i < m_uiDataSize; ++i)
			m_pData[i].~T( );

		m_uiDataSize = 0;
		m_pData = NULL;
	}

	if (pMemory == NULL)
	{
		m_bExternalMemory = false;
	}
	else
	{
		m_uiDataSize = uiSize / sizeof(T);
		m_pData = (T*) pMemory;

	    for (size_t i = 0; i < m_uiDataSize; ++i)
		    (void) new (static_cast<void*>(m_pData + i)) T;

		m_bExternalMemory = true;
	}
}


/////////////////////////////////////////////////////////////////////
// CArray::SetHeapDescription

template <class T, bool bUseCustomAllocator>
ST_INLINE void CArray<T, bUseCustomAllocator>::SetHeapDescription(const char* pDesc)
{
	m_pHeapDesc = pDesc;
}




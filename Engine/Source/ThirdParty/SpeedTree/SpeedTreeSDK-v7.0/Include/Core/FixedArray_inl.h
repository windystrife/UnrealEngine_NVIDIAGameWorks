///////////////////////////////////////////////////////////////////////  
//	FixedArray.inl
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
// CFixedArray::CFixedArray

template <class T, size_t uiDataSize>
ST_INLINE CFixedArray<T, uiDataSize>::CFixedArray(void) : 
	m_uiSize(0)
{
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::CFixedArray

template <class T, size_t uiDataSize>
ST_INLINE CFixedArray<T, uiDataSize>::CFixedArray(size_t uiSize, const T& tDefault) : 
	m_uiSize(0)
{
	assert(uiSize < uiDataSize);
	resize(uiSize, tDefault);
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::CFixedArray

template <class T, size_t uiDataSize>
ST_INLINE CFixedArray<T, uiDataSize>::CFixedArray(const CFixedArray<T, uiDataSize>& cCopy) :
	m_uiSize(0)
{
	*this = cCopy;
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::~CFixedArray

template <class T, size_t uiDataSize>
ST_INLINE CFixedArray<T, uiDataSize>::~CFixedArray(void)
{
	clear( );
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::clear

template <class T, size_t uiDataSize>
ST_INLINE void CFixedArray<T, uiDataSize>::clear(void)
{
	m_uiSize = 0;
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::empty

template <class T, size_t uiDataSize>
ST_INLINE bool CFixedArray<T, uiDataSize>::empty(void) const
{
	return !m_uiSize;
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::size

template <class T, size_t uiDataSize>
ST_INLINE size_t CFixedArray<T, uiDataSize>::size(void) const
{
	return m_uiSize;
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::capacity

template <class T, size_t uiDataSize>
ST_INLINE size_t CFixedArray<T, uiDataSize>::capacity(void) const
{
	return uiDataSize;
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::resize

template <class T, size_t uiDataSize>
ST_INLINE bool CFixedArray<T, uiDataSize>::resize(size_t uiSize)
{
	if (reserve(uiSize))
	{
		m_uiSize = uiSize;
		return true;
	}

	m_uiSize = uiDataSize;
	return false;
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::resize

template <class T, size_t uiDataSize>
ST_INLINE bool CFixedArray<T, uiDataSize>::resize(size_t uiSize, const T& tDefault)
{
	bool bReturn = false;
	size_t uiEnd = uiDataSize;
	if (reserve(uiSize))
	{
		uiEnd = uiSize;
		bReturn = true;
	}

	for (size_t i = m_uiSize; i < uiEnd; ++i)
		m_aData[i] = tDefault;
	m_uiSize = uiEnd;

	return bReturn;
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::reserve

template <class T, size_t uiDataSize>
ST_INLINE bool CFixedArray<T, uiDataSize>::reserve(size_t uiSize)
{
	return (uiSize <= uiDataSize);
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::push_back

template <class T, size_t uiDataSize>
ST_INLINE bool CFixedArray<T, uiDataSize>::push_back(const T& tNew)
{
	if (m_uiSize < uiDataSize)
	{
		m_aData[m_uiSize++] = tNew;
		return true;
	}

	return false;
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::pop_back

template <class T, size_t uiDataSize>
ST_INLINE void CFixedArray<T, uiDataSize>::pop_back(void)
{
	if (m_uiSize > 0)
		--m_uiSize;
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::erase_all

template <class T, size_t uiDataSize>
ST_INLINE void CFixedArray<T, uiDataSize>::erase_all(const T& tErase)
{
	if (m_uiSize > 0)
	{
		iterator iterTest = m_aData + m_uiSize;
		while (--iterTest >= m_aData)
		{
			if (*iterTest == tErase)
				erase(iterTest);
		}
	}
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::front

template <class T, size_t uiDataSize>
ST_INLINE T& CFixedArray<T, uiDataSize>::front(void)
{
	return m_aData[0];
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::back

template <class T, size_t uiDataSize>
ST_INLINE T& CFixedArray<T, uiDataSize>::back(void)
{
	return m_aData[m_uiSize - 1];
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::operator []

template <class T, size_t uiDataSize>
ST_INLINE T& CFixedArray<T, uiDataSize>::operator [] (size_t uiIndex)
{
	assert(uiIndex < m_uiSize);

	return m_aData[uiIndex];
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::operator []

template <class T, size_t uiDataSize>
ST_INLINE const T& CFixedArray<T, uiDataSize>::operator [] (size_t uiIndex) const
{
	assert(uiIndex < m_uiSize);

	return m_aData[uiIndex];
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::at

template <class T, size_t uiDataSize>
ST_INLINE T& CFixedArray<T, uiDataSize>::at(size_t uiIndex)
{
	assert(uiIndex < m_uiSize);

	return m_aData[uiIndex];
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::at

template <class T, size_t uiDataSize>
ST_INLINE const T& CFixedArray<T, uiDataSize>::at(size_t uiIndex) const
{
	assert(uiIndex < m_uiSize);

	return m_aData[uiIndex];
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::operator =

template <class T, size_t uiDataSize>
ST_INLINE CFixedArray<T, uiDataSize>& CFixedArray<T, uiDataSize>::operator = (const CFixedArray<T, uiDataSize>& cRight)
{
	m_uiSize = cRight.m_uiSize;
	if (cRight.m_uiSize > 0)
	{
		T* pNew = m_aData;
		const T* pOld = cRight.m_aData;
		for (size_t i = 0; i < size_t(m_uiSize); ++pNew, ++pOld, ++i)
			*pNew = *pOld;
	}

	return *this;
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::begin

template <class T, size_t uiDataSize>
ST_INLINE T* CFixedArray<T, uiDataSize>::begin(void)
{
	return m_aData;
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::end

template <class T, size_t uiDataSize>
ST_INLINE T* CFixedArray<T, uiDataSize>::end(void)
{
	return (m_aData + m_uiSize);
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::begin

template <class T, size_t uiDataSize>
ST_INLINE T const* CFixedArray<T, uiDataSize>::begin(void) const
{
	return m_aData;
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::end

template <class T, size_t uiDataSize>
ST_INLINE T const* CFixedArray<T, uiDataSize>::end(void) const
{
	return (m_aData + m_uiSize);
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::erase

template <class T, size_t uiDataSize>
ST_INLINE typename CFixedArray<T, uiDataSize>::iterator CFixedArray<T, uiDataSize>::erase(iterator iterWhere)
{
	#ifdef SPEEDTREE_ITERATOR_DEBUGGING
		// make sure iterator is part of this list
		assert(iterWhere >= m_aData);
		assert(iterWhere < m_aData + m_uiSize);
	#endif

	if (iterWhere == m_aData + m_uiSize - 1)
	{
		// at the end, so just decrement size
		--m_uiSize;
	}
	else
	{
		// move the erased one to the end so destructors are called correctly
		char achTemp[sizeof(T)];
		memmove(achTemp, iterWhere, sizeof(T));
		memmove(iterWhere, iterWhere + 1, (m_uiSize - (iterWhere - m_aData) - 1) * sizeof(T));
		--m_uiSize;
		memmove(m_aData + m_uiSize, achTemp, sizeof(T));
	}

	return iterWhere;
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::insert

template <class T, size_t uiDataSize>
ST_INLINE typename CFixedArray<T, uiDataSize>::iterator CFixedArray<T, uiDataSize>::insert(iterator iterWhere, const T& tData)
{
	#ifdef SPEEDTREE_ITERATOR_DEBUGGING
		// make sure iterator is part of this list
		assert((m_aData != NULL && iterWhere != NULL) || (m_aData == NULL && iterWhere == NULL));
		if (m_aData != NULL)
		{
			assert(iterWhere >= m_aData);
			assert(iterWhere < m_aData + m_uiSize + 1);
		}
	#endif

	// remember iterator position
	size_t uiIndex = iterWhere - m_aData;

	// this will ensure the array is big enough, etc.
	if (push_back(tData))
	{
		if (m_uiSize > 1)
		{
			// move the new one from the end so destructors are called correctly
			char achTemp[sizeof(T)];
			memmove(achTemp, m_aData + m_uiSize - 1, sizeof(T));
			memmove(m_aData + uiIndex + 1, m_aData + uiIndex, (m_uiSize - uiIndex - 1) * sizeof(T));
			memmove(m_aData + uiIndex, achTemp, sizeof(T));
		}
		return (m_aData + uiIndex);
	}
		
	return NULL;
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::lower

template <class T, size_t uiDataSize>
ST_INLINE typename CFixedArray<T, uiDataSize>::iterator CFixedArray<T, uiDataSize>::lower(const T& tData)
{
	if (m_uiSize == 0 || tData < m_aData[0])
		return m_aData + m_uiSize;

	size_t uiWidth = m_uiSize / 2;
	iterator iterStart = m_aData;
	iterator iterEnd = m_aData + m_uiSize;
	
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
// CFixedArray::higher

template <class T, size_t uiDataSize>
ST_INLINE typename CFixedArray<T, uiDataSize>::iterator CFixedArray<T, uiDataSize>::higher(const T& tData)
{
	iterator iterBound = lower(tData);
	if (iterBound == end( ))
	{
		if (m_uiSize > 0 && tData < *m_aData)
			iterBound = m_aData;
	}
	else if (*iterBound < tData)
		iterBound++;
	return iterBound;
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::lower_and_higher

template <class T, size_t uiDataSize>
void CFixedArray<T, uiDataSize>::lower_and_higher(const T& tData, iterator& iterLower, iterator& iterHigher)
{
	iterLower = lower(tData);
	iterHigher = iterLower;
	if (iterHigher == end( ))
	{
		if (m_uiSize > 0 && tData < *m_aData)
			iterHigher = m_aData;
	}
	else if (*iterHigher < tData)
		iterHigher++;
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::insert_sorted

template <class T, size_t uiDataSize>
ST_INLINE typename CFixedArray<T, uiDataSize>::iterator CFixedArray<T, uiDataSize>::insert_sorted(const T& tData)
{
	return insert(higher(tData), tData);		
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::insert_sorted_unique

template <class T, size_t uiDataSize>
ST_INLINE typename CFixedArray<T, uiDataSize>::iterator CFixedArray<T, uiDataSize>::insert_sorted_unique(const T& tData)
{
	iterator iterLower;
	iterator iterHigher;
	lower_and_higher(tData, iterLower, iterHigher);

	if (iterLower == end( ) || iterLower != iterHigher)
		return insert(iterHigher, tData);
	return iterLower;	
}


/////////////////////////////////////////////////////////////////////
// CFixedArray::sort

template <class T, size_t uiDataSize>
template <class CompareFunctor>
ST_INLINE void CFixedArray<T, uiDataSize>::sort(const CompareFunctor& Compare, bool bMemorySwap)
{
	if (m_uiSize < 2)
		return;
	
	if (bMemorySwap)
	{
		char achTemp[sizeof(T)];
		ArrayQuickSort(m_aData, m_aData + m_uiSize - 1, Compare, CArrayPointerMemorySwap( ), CArrayPointerMemoryCopy( ), (T*)achTemp);
	}
	else
	{
		T tTemp;
		ArrayQuickSort(m_aData, m_aData + m_uiSize - 1, Compare, CArrayPointerSwap( ), CArrayPointerCopy( ), &tTemp);
	}
}	


/////////////////////////////////////////////////////////////////////
// CFixedArray::sort

template <class T, size_t uiDataSize>
ST_INLINE void CFixedArray<T, uiDataSize>::sort(bool bMemorySwap)
{
	if (m_uiSize < 2)
		return;
	
	
	if (bMemorySwap)
	{
		char achTemp[sizeof(T)];
		ArrayQuickSort(m_aData, m_aData + m_uiSize - 1, CDefaultArraySort( ), CArrayPointerMemorySwap( ), CArrayPointerMemoryCopy( ), (T*)achTemp);
	}
	else
	{
		T tTemp;
		ArrayQuickSort(m_aData, m_aData + m_uiSize - 1, CDefaultArraySort( ), CArrayPointerSwap( ), CArrayPointerCopy( ), &tTemp);
	}
}	


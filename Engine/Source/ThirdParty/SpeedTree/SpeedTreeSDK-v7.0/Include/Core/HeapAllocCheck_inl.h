///////////////////////////////////////////////////////////////////////  
//  HeapAllocCheck.inl
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


#ifdef SPEEDTREE_RUNTIME_HEAP_CHECK

	///////////////////////////////////////////////////////////////////////
	//  CHeapAllocCheck::CHeapAllocCheck

	template<class T>
	ST_INLINE CHeapAllocCheck<T>::CHeapAllocCheck(const T& tContainer, ESdkLimit eLimit, const char* pSourceFilename, st_int32 nSourceLineNum) :
		m_pContainer(&tContainer),
		m_eLimit(eLimit),
		m_pSourceFilename(pSourceFilename),
		m_nSourceLineNum(nSourceLineNum)
	{
		m_siInitCapacity = m_pContainer->capacity( );
	}


	///////////////////////////////////////////////////////////////////////
	//  CHeapAllocCheck::~CHeapAllocCheck

	template<class T>
	ST_INLINE CHeapAllocCheck<T>::~CHeapAllocCheck( )
	{
		assert(m_pContainer);

		const char* c_apLimitNames[SDK_LIMIT_COUNT] = 
		{
			"m_nMaxBaseTrees",
			"m_nMaxVisibleTreeCells",
			"m_nMaxVisibleGrassCells",
			"m_nMaxVisibleTerrainCells",
			"m_nMaxTreeInstancesInAnyCell",
			"m_nMaxPerBaseGrassInstancesInAnyCell"
		};

		size_t siCurrentCapacity = m_pContainer->capacity( );
		if (siCurrentCapacity > m_siInitCapacity)
			CCore::SetError("Heap allocation @ %s:%d, increasing SHeapReserves::%s should prevent this; capacity (not size) went from %d to %d\n",
				m_pSourceFilename, m_nSourceLineNum, c_apLimitNames[m_eLimit], m_siInitCapacity, siCurrentCapacity);
	}

#endif

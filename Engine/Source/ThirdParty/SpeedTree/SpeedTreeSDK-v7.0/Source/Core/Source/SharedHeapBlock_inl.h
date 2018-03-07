///////////////////////////////////////////////////////////////////////  
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
// CSharedHeapBlock::CSharedHeapBlock

ST_INLINE CSharedHeapBlock::CSharedHeapBlock( ) :
	m_pBuffer(NULL),
	m_siSize(0),
	m_bInUse(false)
{
}


/////////////////////////////////////////////////////////////////////
// CSharedHeapBlock::Lock

ST_INLINE st_byte* CSharedHeapBlock::Lock(size_t siSizeInBytes, const char* pOwner)
{
	st_byte* pReturn = NULL;

	if (!m_bInUse)
	{
		m_bInUse = true;
		m_strOwner = CFixedString(pOwner);

		// if current buffer isn't big enough, dump the current buffer and allocate another
		if (siSizeInBytes > m_siSize)
		{
			st_delete_array<st_byte>(m_pBuffer);
			m_siSize = siSizeInBytes;
			m_pBuffer = st_new_array<st_byte>(m_siSize, "CSharedHeapBlock::Lock", ALLOC_TYPE_TEMPORARY);
		}

		pReturn = m_pBuffer;
	}
	else
		CCore::SetError("CSharedHeapBlock::Lock(), overlapping tmp buffer requests; likely CCore::UnlockTmpBuffer was not called");

	return pReturn;
}


/////////////////////////////////////////////////////////////////////
// CSharedHeapBlock::Unlock

ST_INLINE st_bool CSharedHeapBlock::Unlock(void)
{
	st_bool bSuccess = false;

	if (m_bInUse)
	{
		m_bInUse = false;
		m_strOwner.clear( );

		bSuccess = true;
	}
	else
		CCore::SetError("CSharedHeapBlock::Unlock() called when buffer was not locked");

	return bSuccess;
}


/////////////////////////////////////////////////////////////////////
// CSharedHeapBlock::Delete

ST_INLINE st_bool CSharedHeapBlock::Delete(void)
{
	st_bool bSuccess = false;

	if (!m_bInUse)
	{
		st_delete_array<st_byte>(m_pBuffer);
		m_siSize = 0;

		bSuccess = true;
	}
	else
		CCore::SetError("CSharedHeapBlock::Delete() called when buffer was locked");

	return bSuccess;
}


/////////////////////////////////////////////////////////////////////
// CSharedHeapBlock::IsAvailable

ST_INLINE st_bool CSharedHeapBlock::IsAvailable(void) const
{
	return !m_bInUse;
}


/////////////////////////////////////////////////////////////////////
// CSharedHeapBlock::GetOwner

ST_INLINE const char* CSharedHeapBlock::GetOwner(void) const
{
	return m_strOwner.c_str( );
}


/////////////////////////////////////////////////////////////////////
// CSharedHeapBlock::Size

ST_INLINE size_t CSharedHeapBlock::Size(void) const
{
	return m_siSize;
}



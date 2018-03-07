///////////////////////////////////////////////////////////////////////  
//	StaticArray.inl
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
//	Class CStaticArray
//
//	Interface is nearly identical to CArray, but a CStaticArray's size
//	is constant after its initial creation.  It uses the TmpHeapBlock
//	functions in Core to avoid new heap allocations upon each creation.

template <class T>
class ST_DLL_LINK CStaticArray : public CArray<T>
{
public:
		CStaticArray(size_t siNumElements, const char* pOwner, bool bResize = true) :
			m_bInUse(true),
			m_nBlockHandle(-1)
		{
			size_t siBlockSize = siNumElements * sizeof(T);
			st_byte* pHeapBlock = CCore::TmpHeapBlockLock(siBlockSize, pOwner, m_nBlockHandle);
			assert(pHeapBlock);
			CArray<T>::SetExternalMemory(pHeapBlock, siBlockSize);
			if (bResize)
				CArray<T>::resize(siNumElements);
		}

		~CStaticArray( )
		{
			if (m_bInUse)
			{
				assert(m_nBlockHandle > -1);
				CCore::TmpHeapBlockUnlock(m_nBlockHandle);
			}
		}

		void Release(void)
		{
			if (m_bInUse)
			{
				assert(m_nBlockHandle > -1);
				CCore::TmpHeapBlockUnlock(m_nBlockHandle);
				m_bInUse = false;
			}
		}

private:
		bool		m_bInUse;
		int			m_nBlockHandle;
};


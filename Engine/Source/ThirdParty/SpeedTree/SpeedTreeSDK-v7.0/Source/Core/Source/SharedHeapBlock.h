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
// Preprocessor

#pragma once
#include "Core/Types.h"


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
	//  Structure CSharedHeapBlock

	class CSharedHeapBlock
	{
	public:
			friend class CCore;

							CSharedHeapBlock( );

			st_byte*		Lock(size_t siSizeInBytes, const char* pOwner);
			st_bool			Unlock(void);
			st_bool			Delete(void);
			st_bool			IsAvailable(void) const;
			const char*		GetOwner(void) const;
			size_t			Size(void) const;

	private:
			st_byte*		m_pBuffer;
			size_t			m_siSize;
			CFixedString	m_strOwner;
			st_bool			m_bInUse;
	};

	// include inline functions
	#include "SharedHeapBlock_inl.h"

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif


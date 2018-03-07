///////////////////////////////////////////////////////////////////////  
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


///////////////////////////////////////////////////////////////////////  
//  Preprocessor

#pragma once
#include "Core/Core.h"
#include "Core/Mutex.h"


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
	//  Constants

	const st_int32 c_nInitialErrorReserve = 20;
	const st_int32 c_nMaxErrors = 20;
	const st_int32 c_nInvalidErrorSlot = -1;


	///////////////////////////////////////////////////////////////////////  
	//  Class CErrorHandler

	class CErrorHandler
	{
	public:
			void										SetError(const char* pError);
			const char*									GetError(void);

	private:
			CArray<CBasicFixedString<1024>, false>		m_astrErrors;
			SpeedTree::CMutex							m_cMutex;
	};

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif


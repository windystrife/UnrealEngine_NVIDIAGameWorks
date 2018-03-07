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

#include "ErrorHandler.h"
using namespace SpeedTree;


///////////////////////////////////////////////////////////////////////  
//  CErrorHandler::SetError

void CErrorHandler::SetError(const char* pError)
{
	m_cMutex.Lock( );

	// first use?
	if (m_astrErrors.capacity( ) == 0)
	{
		m_astrErrors.SetHeapDescription("CErrorHandler");
		m_astrErrors.reserve(c_nInitialErrorReserve);
	}

	// insert error in the front
	m_astrErrors.insert(m_astrErrors.begin( ), pError);

	m_cMutex.Unlock( );
}


///////////////////////////////////////////////////////////////////////  
//  CErrorHandler::GetError

const char* CErrorHandler::GetError(void)
{
	const char* pError = NULL;

	m_cMutex.Lock( );

	if (!m_astrErrors.empty( ))
	{
		// pull errors from the back
		pError = m_astrErrors.back( ).c_str( );
		m_astrErrors.pop_back( );
	}

	m_cMutex.Unlock( );

	return pError;
}


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

#include "Core/Memory.h"
using namespace SpeedTree;


///////////////////////////////////////////////////////////////////////
//  CHeapSystem static variables

CAllocator*		CHeapSystem::m_pAllocator = NULL;
size_t			CHeapSystem::m_siCurrentUsage = 0;
size_t			CHeapSystem::m_siPeakUsage = 0;
size_t			CHeapSystem::m_siNumAllocs = 0;


///////////////////////////////////////////////////////////////////////
//  CHeapSystem::Allocator

CAllocator*& CHeapSystem::Allocator(void)
{
	return m_pAllocator;
}


///////////////////////////////////////////////////////////////////////
//  CHeapSystem::CurrentUse

size_t&	CHeapSystem::CurrentUse(void)
{
	return m_siCurrentUsage;
}


///////////////////////////////////////////////////////////////////////
//  CHeapSystem::PeakUse

size_t&	CHeapSystem::PeakUse(void)
{
	return m_siPeakUsage;
}


///////////////////////////////////////////////////////////////////////
//  CHeapSystem::NumAllocs

size_t&	CHeapSystem::NumAllocs(void)
{
	return m_siNumAllocs;
}

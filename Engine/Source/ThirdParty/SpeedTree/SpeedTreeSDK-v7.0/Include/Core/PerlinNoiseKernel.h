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
#include "Core/ExportBegin.h"
#include "Core/Array.h"
#include "Core/Random.h"


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
	//  Class CPerlinNoiseKernel

	class ST_DLL_LINK CPerlinNoiseKernel
	{
	public:
								CPerlinNoiseKernel(st_int32 nSize = 256);

			st_float32			Kernel(st_int32 nCol, st_int32 nRow) const;
			st_float32			BilinearSample(st_float32 x, st_float32 y) const; // xy range = [0, kernel_size-1]

	private:
			st_int32			m_nSize;
			CArray<st_float32>	m_aKernel;
	};

	// include inline functions
	#include "Core/PerlinNoiseKernel_inl.h"

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif

#include "Core/ExportEnd.h"

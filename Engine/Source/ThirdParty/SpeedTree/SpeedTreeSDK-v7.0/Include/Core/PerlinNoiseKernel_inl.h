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
//  CPerlinNoiseKernel::CPerlinNoiseKernel

ST_INLINE CPerlinNoiseKernel::CPerlinNoiseKernel(st_int32 nSize) :
	m_nSize(nSize)
{
	m_aKernel.resize(m_nSize * m_nSize);

	CRandom cDice;
	for (st_int32 i = 0; i < m_nSize * m_nSize; ++i)
		m_aKernel[i] = cDice.GetFloat(0.0f, 1.0f);
}


///////////////////////////////////////////////////////////////////////  
//  CPerlinNoiseKernel::Kernel

ST_INLINE st_float32 CPerlinNoiseKernel::Kernel(st_int32 nCol, st_int32 nRow) const
{
	assert(size_t(nRow * m_nSize + nCol) < m_aKernel.size( ));

	return m_aKernel[nRow * m_nSize + nCol];
}


///////////////////////////////////////////////////////////////////////  
//  CPerlinNoiseKernel::BilinearSample

ST_INLINE st_float32 CPerlinNoiseKernel::BilinearSample(st_float32 x, st_float32 y) const
{
	// adjust parameters
	x = fabs(x);
	y = fabs(y);

	// get fractional part of x and y
	st_float32 fFractX = x - st_int32(x);
	st_float32 fFractY = y - st_int32(y);

	// wrap around
	int x1 = (st_int32(x) + m_nSize) % m_nSize;
	int y1 = (st_int32(y) + m_nSize) % m_nSize;

	// neighbor values
	int x2 = (x1 + m_nSize - 1) % m_nSize;
	int y2 = (y1 + m_nSize - 1) % m_nSize;

	// smooth the noise with bilinear interpolation
	float fBilinearSample = 0.0f;
	fBilinearSample += fFractX       * fFractY       * Kernel(x1, y1);
	fBilinearSample += fFractX       * (1 - fFractY) * Kernel(x1, y2);
	fBilinearSample += (1 - fFractX) * fFractY       * Kernel(x2, y1);
	fBilinearSample += (1 - fFractX) * (1 - fFractY) * Kernel(x2, y2);

	return fBilinearSample;
}


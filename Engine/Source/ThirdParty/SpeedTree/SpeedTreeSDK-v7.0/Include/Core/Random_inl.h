///////////////////////////////////////////////////////////////////////  
//	Random_inl.h
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
// Preprocessor/Includes

#ifdef _WIN32 // Windows or Xbox 360
	#pragma warning(disable : 4146)
#endif


/////////////////////////////////////////////////////////////////////
// CRandom::CRandom

ST_INLINE CRandom::CRandom( )
{
	Seed(0);
}


/////////////////////////////////////////////////////////////////////
// CRandom::CRandom

ST_INLINE CRandom::CRandom(st_uint32 uiSeed)
{
	Seed(uiSeed);
}


/////////////////////////////////////////////////////////////////////
// CRandom::CRandom

ST_INLINE CRandom::CRandom(const CRandom& cCopy)
{
	Seed(cCopy.m_uiSeed);
}


/////////////////////////////////////////////////////////////////////
// CRandom::~CRandom

ST_INLINE CRandom::~CRandom( )
{

}


/////////////////////////////////////////////////////////////////////
// CRandom::Seed

ST_INLINE void CRandom::Seed(st_uint32 uiSeed)
{
	m_uiSeed = uiSeed;
	memset(m_auiTable, 0, (SIZE + 1) * sizeof(st_uint32));
	st_uint32* pS = m_auiTable;
	st_uint32* pR = m_auiTable;

	*pS = m_uiSeed & 0xffffffff;
	++pS;
	for (st_uint32 i = 1; i < SIZE; ++i)
	{
		*pS = (1812433253U * (*pR ^ (*pR >> 30)) + i) & 0xffffffff;
		++pS;
		++pR;
	}

	Reload( );
}


/////////////////////////////////////////////////////////////////////
// CRandom::GetInteger

ST_INLINE st_int32 CRandom::GetInteger(st_int32 nLow, st_int32 nHigh)
{
	if (nHigh == nLow)
		return nHigh;
	else
		return (GetRawInteger( ) % (nHigh - nLow + 1) + nLow);
}


/////////////////////////////////////////////////////////////////////
// CRandom::GetFloat

ST_INLINE st_float32 CRandom::GetFloat(st_float32 fLow, st_float32 fHigh)
{
	return ((st_float32(GetRawInteger( )) + 0.5f) * (1.0f / 4294967296.0f)) * (fHigh - fLow) + fLow;
}


/////////////////////////////////////////////////////////////////////
// CRandom::GetDouble

ST_INLINE st_float64 CRandom::GetDouble(st_float64 fLow, st_float64 fHigh)
{
	return ((st_float64(GetRawInteger( )) + 0.5) * (1.0 / 4294967296.0)) * (fHigh - fLow) + fLow;
}


/////////////////////////////////////////////////////////////////////
// CRandom::GetGaussianFloat

ST_INLINE st_float32 CRandom::GetGaussianFloat(void)
{
	st_float32 fX1 = 0.0f;
	st_float32 fX2 = 0.0f;
	st_float32 fW = 1.0f;
 
    while (fW >= 1.0f)
	{
		fX1 = 2.0f * ((st_float32(GetRawInteger( )) + 0.5f) * (1.0f / 4294967296.0f)) - 1.0f;
		fX2 = 2.0f * ((st_float32(GetRawInteger( )) + 0.5f) * (1.0f / 4294967296.0f)) - 1.0f;
		fW = fX1 * fX1 + fX2 * fX2;
    }

	return (fX1 * sqrt((-2.0f * log(fW)) / fW));
}


/////////////////////////////////////////////////////////////////////
// CRandom::GetGaussianDouble

ST_INLINE st_float64 CRandom::GetGaussianDouble(void)
{
	st_float64 fX1 = 0.0;
	st_float64 fX2 = 0.0;
	st_float64 fW = 1.0;
 
    while (fW >= 1.0)
	{
		fX1 = 2.0 * ((st_float64(GetRawInteger( )) + 0.5) * (1.0 / 4294967296.0)) - 1.0;
		fX2 = 2.0 * ((st_float64(GetRawInteger( )) + 0.5) * (1.0 / 4294967296.0)) - 1.0;
		fW = fX1 * fX1 + fX2 * fX2;
    }

	return (fX1 * sqrt((-2.0 * log(fW)) / fW));
}


/////////////////////////////////////////////////////////////////////
// CRandom::Reload

ST_INLINE void CRandom::Reload(void)
{
	st_uint32* pTemp = m_auiTable;

	for	(st_uint32 i = SIZE - PERIOD; i > 0; --i)
	{
		*pTemp = Twist(pTemp[PERIOD], pTemp[0], pTemp[1]);
		++pTemp;
	}

	for (st_uint32 i = PERIOD; i > 0; --i)
	{
		*pTemp = Twist(pTemp[PERIOD - SIZE], pTemp[0], pTemp[1]);
		++pTemp;
	}

	*pTemp = Twist(pTemp[PERIOD - SIZE], pTemp[0], m_auiTable[0]);

	m_nCount = SIZE;
	m_pNext = m_auiTable;
}


/////////////////////////////////////////////////////////////////////
// CRandom::GetRawInteger

ST_INLINE st_uint32 CRandom::GetRawInteger(void)
{
	if (!m_nCount) 
		Reload( );

	--m_nCount;
		
	st_uint32 uiTemp;
	uiTemp = *m_pNext;
	++m_pNext;
	uiTemp ^= (uiTemp >> 11);
	uiTemp ^= (uiTemp << 7) & 0x9d2c5680;
	uiTemp ^= (uiTemp << 15) & 0xefc60000;
	uiTemp ^= (uiTemp >> 18);

	return uiTemp;
}


/////////////////////////////////////////////////////////////////////
// CRandom::Twist

ST_INLINE st_uint32 CRandom::Twist(st_uint32 uiPrime, st_uint32 uiInput0, st_uint32 uiInput1) const
{
	return (uiPrime ^ (((uiInput0 & 0x80000000) | (uiInput1 & 0x7fffffff)) >> 1) ^ (-(uiInput1 & 0x00000001) & 0x9908b0df));
}

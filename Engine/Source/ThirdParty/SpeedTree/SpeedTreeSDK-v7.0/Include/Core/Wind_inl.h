///////////////////////////////////////////////////////////////////////
//  Wind.inl
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
// CWind::SetParams

ST_INLINE void CWind::SetParams(const CWind::SParams& sParams)
{
	m_sParams = sParams;
}


///////////////////////////////////////////////////////////////////////
// CWind::GetParams

ST_INLINE const CWind::SParams& CWind::GetParams(void) const
{
	return m_sParams;
}


///////////////////////////////////////////////////////////////////////
// CWind::EnableGusting

ST_INLINE void CWind::EnableGusting(st_bool bEnabled)
{
	m_bGustingEnabled = bEnabled;
}


///////////////////////////////////////////////////////////////////////
// CWind::SetGustFrequency

ST_INLINE void CWind::SetGustFrequency(st_float32 fGustFreq)
{
	m_sParams.m_fGustFrequency = fGustFreq;
}


///////////////////////////////////////////////////////////////////////
// CWind::SetTreeValues

ST_INLINE void CWind::SetTreeValues(const Vec3& vBranchAnchor, st_float32 fMaxBranchLength)
{
	m_afBranchWindAnchor[0] = vBranchAnchor.x;
	m_afBranchWindAnchor[1] = vBranchAnchor.y;
	m_afBranchWindAnchor[2] = vBranchAnchor.z;
	m_fMaxBranchLevel1Length = fMaxBranchLength;
}


///////////////////////////////////////////////////////////////////////
// CWind::GetBranchAnchor

ST_INLINE const st_float32* CWind::GetBranchAnchor(void) const
{
	return m_afBranchWindAnchor;
}


///////////////////////////////////////////////////////////////////////
// CWind::GetMaxBranchLength

ST_INLINE st_float32 CWind::GetMaxBranchLength(void) const
{
	return m_fMaxBranchLevel1Length;
}


///////////////////////////////////////////////////////////////////////
// Helper function: ScaleWindCurve

ST_INLINE void ScaleWindCurve(st_float32 afCurve[c_nNumWindPointsInCurves], st_float32 fScalar)
{
	for (st_int32 i = 0; i < c_nNumWindPointsInCurves; ++i)
		afCurve[i] *= fScalar;
}


///////////////////////////////////////////////////////////////////////
// CWind::Normalize
/// Normalizes the the incoming vector (\a pVector).

ST_INLINE void CWind::Normalize(st_float32* pVector)
{
	st_float32 fMagnitude = sqrtf(pVector[0] * pVector[0] + pVector[1] * pVector[1] + pVector[2] * pVector[2]);
	if (fMagnitude != 0.0f)
	{
		pVector[0] /= fMagnitude;
		pVector[1] /= fMagnitude;
		pVector[2] /= fMagnitude;
	}
	else
	{
		pVector[0] = 0.0f;
		pVector[1] = 0.0f;
		pVector[2] = 0.0f;
	}
}


///////////////////////////////////////////////////////////////////////
//  CWind::SetOption

ST_INLINE void CWind::SetOption(EOptions eOption, st_bool bState)
{ 
	m_abOptions[eOption] = bState; 
}


///////////////////////////////////////////////////////////////////////
//  CWind::IsOptionEnabled

ST_INLINE st_bool CWind::IsOptionEnabled(EOptions eOption) const
{ 
	return m_abOptions[eOption];
}


///////////////////////////////////////////////////////////////////////
//  CWind::IsGlobalWindEnabled

ST_INLINE st_bool CWind::IsGlobalWindEnabled(void) const
{ 
	return IsOptionEnabled(GLOBAL_WIND) ||
		   IsOptionEnabled(GLOBAL_PRESERVE_SHAPE);
}


///////////////////////////////////////////////////////////////////////
//  CWind::IsBranchWindEnabled

ST_INLINE st_bool CWind::IsBranchWindEnabled(void) const
{
	for (st_int32 i = BRANCH_SIMPLE_1; i <= BRANCH_OSC_COMPLEX_2; ++i)
		if (IsOptionEnabled(EOptions(i)))
			return true;

	return false;
}


///////////////////////////////////////////////////////////////////////
//  CWind::GetShaderTable

ST_INLINE const st_float32* CWind::GetShaderTable(void) const
{ 
	return m_afShaderTable;
}

///////////////////////////////////////////////////////////////////////  
//  Core.inl
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
//  SLodProfile::SLodProfile

inline SLodProfile::SLodProfile( ) :
	m_fHighDetail3dDistance(300.0f),
	m_fBillboardStartDistance(1300.0f),
	m_fLowDetail3dDistance(1200.0f),
	m_fBillboardFinalDistance(1500.0f),
	m_bLodIsPresent(true)
{
	ComputeDerived( );
	assert(IsValid( ));
}


///////////////////////////////////////////////////////////////////////
//  SLodProfile::IsValid

inline st_bool SLodProfile::IsValid(void) const
{
	return (m_fHighDetail3dDistance < m_fLowDetail3dDistance) &&
		   (m_fBillboardStartDistance < m_fBillboardFinalDistance) &&
		   (m_fLowDetail3dDistance < m_fBillboardStartDistance) &&
		   (m_f3dRange >= 0.0f) &&
		   (m_fBillboardRange >= 0.0f);
}


///////////////////////////////////////////////////////////////////////
//  SLodProfile::ComputeDerived

inline void SLodProfile::ComputeDerived(void)
{
	m_f3dRange = m_fLowDetail3dDistance - m_fHighDetail3dDistance;
	m_fBillboardRange = m_fBillboardFinalDistance - m_fBillboardStartDistance;
}


///////////////////////////////////////////////////////////////////////
//  SLodProfile::Scale

inline void SLodProfile::Scale(st_float32 fScalar)
{
	m_fHighDetail3dDistance *= fScalar;
	m_fLowDetail3dDistance *= fScalar;
	m_fBillboardStartDistance *= fScalar;
	m_fBillboardFinalDistance *= fScalar;

	ComputeDerived( );
}


///////////////////////////////////////////////////////////////////////
//  SLodProfile::Square

inline void SLodProfile::Square(SLodProfile& sSquaredProfile) const
{
	sSquaredProfile.m_fHighDetail3dDistance = m_fHighDetail3dDistance * m_fHighDetail3dDistance;
	sSquaredProfile.m_fLowDetail3dDistance = m_fLowDetail3dDistance * m_fLowDetail3dDistance;
	sSquaredProfile.m_fBillboardStartDistance = m_fBillboardStartDistance * m_fBillboardStartDistance;
	sSquaredProfile.m_fBillboardFinalDistance = m_fBillboardFinalDistance * m_fBillboardFinalDistance;

	sSquaredProfile.ComputeDerived( );

	sSquaredProfile.m_bLodIsPresent = m_bLodIsPresent;
}


///////////////////////////////////////////////////////////////////////
//  SLodProfile::operator const st_float32*

inline SLodProfile::operator const st_float32*(void) const
{
	return &m_fHighDetail3dDistance;
}


///////////////////////////////////////////////////////////////////////
//  CCore::GetFilename

inline const st_char* CCore::GetFilename(void) const
{
	return m_strFilename.c_str( );
}


///////////////////////////////////////////////////////////////////////
//  CCore::GetGeometry

inline const SGeometry* CCore::GetGeometry(void) const
{
	return &m_sGeometry;
}


///////////////////////////////////////////////////////////////////////
//  CCore::GetExtents

inline const CExtents& CCore::GetExtents(void) const
{
	return m_cExtents;
}


///////////////////////////////////////////////////////////////////////
//  CCore::IsGrassModel

inline st_bool CCore::IsGrassModel(void) const
{
	return m_bGrassModel;
}


///////////////////////////////////////////////////////////////////////
//  CCore::AreTexCoordsFlipped

inline st_bool CCore::AreTexCoordsFlipped(void) const
{
    return m_bTexCoordsFlipped;
}


///////////////////////////////////////////////////////////////////////
//  CCore::GetLodProfile

inline const SLodProfile& CCore::GetLodProfile(void) const
{
	return m_sLodProfile;
}


///////////////////////////////////////////////////////////////////////
//  CCore::GetLodProfileSquared

inline const SLodProfile& CCore::GetLodProfileSquared(void) const
{
	return m_sLodProfileSquared;
}


///////////////////////////////////////////////////////////////////////
//  CCore::SetLodProfile

inline st_bool CCore::SetLodProfile(const SLodProfile& sLodProfile)
{
	st_bool bSuccess = false;

	if (sLodProfile.IsValid( ))
	{
		// standard profile
		m_sLodProfile = sLodProfile;
		m_sLodProfile.ComputeDerived( );

		// profile squared
		sLodProfile.Square(m_sLodProfileSquared);

		bSuccess = true;
	}
	else
		CCore::SetError("CCore::SetLodRange, one of the near/start values exceeds its corresponding far/end value");

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////
//  CCore::ComputeLodSnapshot

inline st_int32 CCore::ComputeLodSnapshot(st_float32 fLod) const
{
	st_int32 nLodLevel = -1;

	if (fLod <= 0.0f) // middle of billboard transition
	{
		nLodLevel = st_int8(m_sGeometry.m_nNumLods > 0 ? m_sGeometry.m_nNumLods - 1 : -1);
	}
	else if (fLod >= 1.0f) // fully 3D
	{
		nLodLevel = (m_sGeometry.m_nNumLods > 0) ? 0 : -1;
	}
	else // middle of 3D transition, high to low (no billboard is active)
	{
		const st_float32 c_fMirrorLodValue = 1.0f - fLod;
		nLodLevel = st_int8(c_fMirrorLodValue * m_sGeometry.m_nNumLods);
	}

	return nLodLevel;
}


///////////////////////////////////////////////////////////////////////
//  CCore::ComputeLodByDistance

inline st_float32 CCore::ComputeLodByDistance(st_float32 fDistance) const
{
	st_float32 fLod = -1.0f;

	if (fDistance < m_sLodProfile.m_fHighDetail3dDistance)
		fLod = 1.0f;
	else if (fDistance < m_sLodProfile.m_fLowDetail3dDistance)
		fLod = 1.0f - (fDistance - m_sLodProfile.m_fHighDetail3dDistance) / m_sLodProfile.m_f3dRange;
	else if (fDistance < m_sLodProfile.m_fBillboardStartDistance)
		fLod = 0.0f;
	else if (fDistance < m_sLodProfile.m_fBillboardFinalDistance)
		fLod = -(fDistance - m_sLodProfile.m_fBillboardStartDistance) / m_sLodProfile.m_fBillboardRange;

	return fLod;
}


///////////////////////////////////////////////////////////////////////
//  CCore::ComputeLodByDistanceSquared

inline st_float32 CCore::ComputeLodByDistanceSquared(st_float32 fDistanceSquared) const
{
	st_float32 fLod = -1.0f;

	if (fDistanceSquared < m_sLodProfileSquared.m_fHighDetail3dDistance)
		fLod = 1.0f;
	else if (fDistanceSquared < m_sLodProfileSquared.m_fLowDetail3dDistance)
		fLod = 1.0f - (fDistanceSquared - m_sLodProfileSquared.m_fHighDetail3dDistance) / m_sLodProfileSquared.m_f3dRange;
	else if (fDistanceSquared < m_sLodProfileSquared.m_fBillboardStartDistance)
		fLod = 0.0f;
	else if (fDistanceSquared < m_sLodProfileSquared.m_fBillboardFinalDistance)
		fLod = -(fDistanceSquared - m_sLodProfileSquared.m_fBillboardStartDistance) / m_sLodProfileSquared.m_fBillboardRange;

	return fLod;
}


///////////////////////////////////////////////////////////////////////
//  CCore::ComputeLodTransition
//
//	Note: Optimization is disabled for Xbox 360 as the optimizer causes 
//  it to compute incorrect values.

// todo: rework for 360
#ifdef _XBOX
	#pragma optimize("", off)
#endif

inline st_float32 CCore::ComputeLodTransition(st_float32 fLod, st_int32 nNumDiscreteLevels)
{
	if (nNumDiscreteLevels == 0)
		return 1.0f;

	st_float32 fLodClamped = st_max(0.0f, fLod);
	st_float32 fSpacing = 1.0f / st_float32(nNumDiscreteLevels);

	st_float32 fMod = fLodClamped - st_int32(fLodClamped / fSpacing) * fSpacing;

	if (fLod <= 0.0f)
		return 0.0f;
	else
		return (fMod == 0.0f) ? 1.0f : fMod / fSpacing;
}

#ifdef _XBOX
	#pragma optimize("", on)
#endif


///////////////////////////////////////////////////////////////////////
//  CCore::GetWind

inline CWind& CCore::GetWind(void)
{
	return m_cWind;
}


///////////////////////////////////////////////////////////////////////
//  CCore::GetWind

inline const CWind& CCore::GetWind(void) const
{
	return m_cWind;
}


///////////////////////////////////////////////////////////////////////
//  CCore::GetWindShaderTable

inline const st_float32* CCore::GetWindShaderTable(void) const
{
	return m_cWind.GetShaderTable( );
}


///////////////////////////////////////////////////////////////////////
//  CCore::GetCollisionObjects

inline const SCollisionObject* CCore::GetCollisionObjects(st_int32& nNumObjects) const
{
	nNumObjects = m_nNumCollisionObjects;
	
	return m_pCollisionObjects;
}


///////////////////////////////////////////////////////////////////////
//  CCore::SetHueVariationParams

inline void CCore::SetHueVariationParams(const SHueVariationParams& sParams)
{
	m_sHueVariationParams = sParams;
}


///////////////////////////////////////////////////////////////////////
//  CCore::GetHueVariationParams

inline const CCore::SHueVariationParams& CCore::GetHueVariationParams(void) const
{
	return m_sHueVariationParams;
}


///////////////////////////////////////////////////////////////////////
//  CCore::SetAmbientImageScalar

inline void CCore::SetAmbientImageScalar(st_float32 fScalar)
{
	m_fAmbientImageScalar = fScalar;
}


///////////////////////////////////////////////////////////////////////
//  CCore::GetAmbientImageScalar

inline st_float32 CCore::GetAmbientImageScalar(void) const
{
	return m_fAmbientImageScalar;
}


///////////////////////////////////////////////////////////////////////
//  CCore::GetUserString

inline const char* CCore::GetUserString(EUserStringOrdinal eOrdinal) const
{
	return m_pUserStrings[eOrdinal];
}


///////////////////////////////////////////////////////////////////////
//  CCore::GetUserData

inline void* CCore::GetUserData(void) const
{
	return m_pUserData;
}


///////////////////////////////////////////////////////////////////////
//  CCore::SetUserData

inline void CCore::SetUserData(void* pUserData)
{
	m_pUserData = pUserData;
}


///////////////////////////////////////////////////////////////////////
//  CCore::UncompressVec3

inline Vec3 CCore::UncompressVec3(const st_uint8* pCompressedVector)
{
	return Vec3(UncompressScalar(pCompressedVector[0]), UncompressScalar(pCompressedVector[1]), UncompressScalar(pCompressedVector[2]));
}


///////////////////////////////////////////////////////////////////////
//  CCore::UncompressScalar

inline st_float32 CCore::UncompressScalar(st_uint8 uiCompressedScalar)
{
	return 2.0f * ((st_float32(uiCompressedScalar) - 0.5f) / 255.0f - 0.5f);
}


///////////////////////////////////////////////////////////////////////
//  CCore::CompressScalar

inline st_uint8 CCore::CompressScalar(st_float32 fUncompressedScalar)
{
	return st_uint8((255 * (fUncompressedScalar * 0.5f + 0.5f)) + 0.5f);
}


///////////////////////////////////////////////////////////////////////
//  CCore::CompressVec3

inline void CCore::CompressVec3(st_uint8 auiCompressedValue[3], const Vec3& vVector)
{
	auiCompressedValue[0] = CompressScalar(vVector.x);
	auiCompressedValue[1] = CompressScalar(vVector.y);
	auiCompressedValue[2] = CompressScalar(vVector.z);
}


///////////////////////////////////////////////////////////////////////
//  CCore::GetVertexPropertyDesc

inline const SVertexPropertyDesc& CCore::GetVertexPropertyDesc(EVertexProperty eProperty)
{
	static const SVertexPropertyDesc asDescs[VERTEX_PROPERTY_COUNT + 2] = // +1 for UNASSIGNED, +1 for PAD
	{
		{ 1, "UNASSIGNED",					"UASS" },
		{ 3, "Position",					"posi" } ,
		{ 2, "DiffuseTexCoords",			"texd" },
		{ 3, "Normal",						"norm" },
		{ 3, "LodPosition",					"lodp" },
		{ 1, "GeometryTypeHint",			"hint" },
		{ 3, "LeafCardCorner",				"lfcc" },
		{ 1, "LeafCardLodScalar",			"lfls" },
		{ 1, "LeafCardSelfShadowOffset",	"lfso" },
		{ 4, "WindBranchData",				"wbrn" },
		{ 3, "WindExtraData",				"wext" },
		{ 1, "WindFlags",					"wflg" },
		{ 3, "LeafAnchorPoint",				"lanc" },
		{ 1, "BoneID",						"bnid" },
		{ 3, "BranchSeamDiffuse",			"bsdf" },
		{ 2, "BranchSeamDetail",			"bsdt" },
		{ 2, "DetailTexCoords",				"texl" },
		{ 3, "Tangent",						"tang" },
		{ 2, "LightMapTexCoords",			"lmap" },
		{ 1, "AmbientOcclusion",			"aocc" },
		{ 1, "Pad",							"pad" }
	};

	return asDescs[eProperty + 1];  // first enum, VERTEX_PROPERTY_UNASSIGNED, starts at -1
}


///////////////////////////////////////////////////////////////////////
//  CCore::GetPixelPropertyDesc

inline const SPixelPropertyDesc& CCore::GetPixelPropertyDesc(EPixelProperty eProperty)
{
	static const SPixelPropertyDesc asDescs[PIXEL_PROPERTY_COUNT + 1] = 
	{
		{ 4, "Projection",					"proj" }, 
		{ 1, "FogScalar",					"fogs" }, 
		{ 3, "FogColor",					"fogc" }, 
		{ 2, "DiffuseTexCoords",			"texd" }, 
		{ 2, "DetailTexCoords",				"texl" }, 
		{ 3, "PerVertexLightingColor",		"pvlc" }, 
		{ 3, "NormalInTangentSpace",		"nmts" }, 
		{ 3, "Normal",						"norm" }, 
		{ 3, "Binormal",					"bnrm" }, 
		{ 3, "Tangent",						"tang" }, 
		{ 3, "SpecularHalfVector",			"shvc" }, 
		{ 1, "PerVertexSpecularDot",		"pvsd" }, 
		{ 1, "PerVertexAmbientContrast",	"pvac" }, 
		{ 1, "FadeToBillboard",				"f2bb" }, 
		{ 1, "TransmissionFactor",			"tran" }, 
		{ 1, "RenderEffectsFade",			"efad" }, 
		{ 1, "AmbientOcclusion",			"  ao" }, 
		{ 3, "BranchSeamDiffuse",			"bsdt" }, 
		{ 2, "BranchSeamDetail",			"bslt" }, 
		{ 1, "ShadowDepth",					"sdep" }, 
		{ 4, "ShadowMapProjection0",		"smp0" }, 
		{ 4, "ShadowMapProjection1",		"smp1" }, 
		{ 4, "ShadowMapProjection2",		"smp2" }, 
		{ 4, "ShadowMapProjection3",		"smp3" }, 
		{ 3, "HueVariation",				"huev" }
	};

	return asDescs[eProperty];
}


///////////////////////////////////////////////////////////////////////
//  CCore::SwizzleName

inline const st_char* CCore::ComponentName(st_int32 nComponent)
{
	if (nComponent > -1 && nComponent < 4)
	{
		static const char* asNames[4] = { "x", "y", "z", "w" };
		return asNames[nComponent];
	}
	else
		return "unknown";
}


///////////////////////////////////////////////////////////////////////
//  CCore::SrtBuffer

inline const st_byte* CCore::SrtBuffer(void) const
{
	return (m_pSrtBufferExternal ? m_pSrtBufferExternal : m_pSrtBufferOwned);
}






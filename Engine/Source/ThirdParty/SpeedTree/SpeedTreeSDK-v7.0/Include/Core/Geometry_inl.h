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
//  SVerticalBillboards::SVerticalBillboards

ST_INLINE SVerticalBillboards::SVerticalBillboards( ) :
	m_nNumBillboards(0),
	m_pTexCoords(NULL),
	m_pRotated(NULL),
	m_nNumCutoutVertices(0),
    m_pCutoutVertices(NULL),
	m_nNumCutoutIndices(0),
	m_pCutoutIndices(NULL)
{
	m_fWidth = m_fTopPos = m_fBottomPos = -1.0f;
}


///////////////////////////////////////////////////////////////////////  
//  SVerticalBillboards::~SVerticalBillboards

ST_INLINE SVerticalBillboards::~SVerticalBillboards( )
{
	#ifndef NDEBUG
		m_pRotated = NULL; // allocated elsewhere
		m_pTexCoords = NULL; // allocated elsewhere
		m_fWidth = m_fTopPos = m_fBottomPos = -1.0f;
		m_nNumBillboards = 0;
	#endif
}


///////////////////////////////////////////////////////////////////////  
//  SHorizontalBillboard::SHorizontalBillboard

ST_INLINE SHorizontalBillboard::SHorizontalBillboard( ) :
	m_bPresent(false)
{
	st_int16 i = 0;
	for (i = 0; i < 4; ++i)
		m_avPositions[i].Set(-1.0f, -1.0f, -1.0f);
}


///////////////////////////////////////////////////////////////////////  
//  SHorizontalBillboard::~SHorizontalBillboard

ST_INLINE SHorizontalBillboard::~SHorizontalBillboard( )
{
	#ifndef NDEBUG
		m_bPresent = false;
		st_int16 i = 0;
		for (i = 0; i < 4; ++i)
			m_avPositions[i].Set(-1.0f, -1.0f, -1.0f);
	#endif
}


///////////////////////////////////////////////////////////////////////  
//  SCollisionObject::SCollisionObject

ST_INLINE SCollisionObject::SCollisionObject( ) :
	m_pUserString(NULL),
	m_fRadius(-1.0f)
{
}


///////////////////////////////////////////////////////////////////////  
//  SCollisionObject::SCollisionObject

ST_INLINE SCollisionObject::~SCollisionObject( )
{
	#ifndef NDEBUG
		m_vCenter1 = Vec3( );
		m_vCenter2 = Vec3( );
		m_fRadius = -1.0f;
	#endif
}


///////////////////////////////////////////////////////////////////////  
//  SDrawCall::SDrawCall

ST_INLINE SDrawCall::SDrawCall( ) :
	m_pRenderState(NULL),
	m_nRenderStateIndex(-1),
	m_nNumVertices(0),
	m_pVertexData(NULL),
	m_nNumIndices(0),
	m_b32BitIndices(false),
	m_pIndexData(NULL)
{
}


///////////////////////////////////////////////////////////////////////  
//  SDrawCall::~SDrawCall

ST_INLINE SDrawCall::~SDrawCall( )
{
}


///////////////////////////////////////////////////////////////////////  
//  SBone::SBone

ST_INLINE SBone::SBone( ) :
	m_nID(-1),
	m_nParentID(-1),
	m_fRadius(0.0f),
	m_fMass(0.0f),
	m_fMassWithChildren(0.0f),
	m_bBreakable(false)
{
}


///////////////////////////////////////////////////////////////////////  
//  SLod::SLod

ST_INLINE SLod::SLod( ) :
	m_nNumDrawCalls(0),
	m_pDrawCalls(NULL),
	m_nNumBones(0),
	m_pBones(NULL)
{
}


///////////////////////////////////////////////////////////////////////  
//  SLod::~SLod

ST_INLINE SLod::~SLod( )
{
}


///////////////////////////////////////////////////////////////////////  
//  SGeometry::SGeometry

ST_INLINE SGeometry::SGeometry( ) :
	m_nNum3dRenderStates(0),
	m_bShadowCastIncluded(false),
	m_nNumLods(0),
	m_pLods(NULL)
{
	memset(m_p3dRenderStates, 0, sizeof(m_p3dRenderStates));
}


///////////////////////////////////////////////////////////////////////  
//  SGeometry::~SGeometry

ST_INLINE SGeometry::~SGeometry( )
{
	// intentionally empty
}


///////////////////////////////////////////////////////////////////////
//  SDrawCall::GetProperty

ST_INLINE st_bool SDrawCall::GetProperty(EVertexProperty eProperty, st_int32 nVertex, st_float32 afValues[4]) const
{
	st_bool bSuccess = false;

	assert(static_cast<const st_byte*>(m_pVertexData));
	assert(static_cast<const SRenderState*>(m_pRenderState));
	assert(m_pRenderState->m_sVertexDecl.m_uiVertexSize > 0);
	assert(nVertex > -1 && nVertex < m_nNumVertices);

	const SVertexDecl::SProperty& sProperty = m_pRenderState->m_sVertexDecl.m_asProperties[eProperty];
	const st_int32 c_nNumComponents = sProperty.NumComponents( );
	if (c_nNumComponents > 0)
	{
		bSuccess = true;

		// no type conversion necessary, just copy
		if (sProperty.m_eFormat == VERTEX_FORMAT_FULL_FLOAT)
		{
			for (st_int32 i = 0; i < c_nNumComponents; ++i)
				afValues[i] = st_float32(*reinterpret_cast<const st_float32*>(m_pVertexData + nVertex * m_pRenderState->m_sVertexDecl.m_uiVertexSize + sProperty.m_auiOffsets[i]));
		}
		// convert from st_float16s
		else if (sProperty.m_eFormat == VERTEX_FORMAT_HALF_FLOAT)
		{
			for (st_int32 i = 0; i < c_nNumComponents; ++i)
				afValues[i] = st_float32(st_float16(*reinterpret_cast<const st_float16*>(m_pVertexData + nVertex * m_pRenderState->m_sVertexDecl.m_uiVertexSize + sProperty.m_auiOffsets[i])));
		}
		// convert from bytes
		else if (sProperty.m_eFormat == VERTEX_FORMAT_BYTE)
		{
			for (st_int32 i = 0; i < c_nNumComponents; ++i)
				afValues[i] = CCore::UncompressScalar(m_pVertexData[nVertex * m_pRenderState->m_sVertexDecl.m_uiVertexSize + sProperty.m_auiOffsets[i]]);
		}
		else
			bSuccess = false;
	}

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////
//  SDrawCall::GetProperty

ST_INLINE st_bool SDrawCall::GetProperty(EVertexProperty eProperty, st_int32 nVertex, st_float16 ahfValues[4]) const
{
	st_bool bSuccess = false;

	assert(static_cast<const st_byte*>(m_pVertexData));
	assert(static_cast<const SRenderState*>(m_pRenderState));
	assert(m_pRenderState->m_sVertexDecl.m_uiVertexSize > 0);
	assert(nVertex > -1 && nVertex < m_nNumVertices);

	const SVertexDecl::SProperty& sProperty = m_pRenderState->m_sVertexDecl.m_asProperties[eProperty];
	const st_int32 c_nNumComponents = sProperty.NumComponents( );
	assert(c_nNumComponents <= VERTEX_COMPONENT_COUNT);
	if (c_nNumComponents > 0)
	{
		bSuccess = true;

		// no conversion necessary, just copy
		if (sProperty.m_eFormat == VERTEX_FORMAT_HALF_FLOAT)
		{
			for (st_int32 i = 0; i < c_nNumComponents; ++i)
				ahfValues[i] = st_float16(*reinterpret_cast<const st_float16*>(m_pVertexData + nVertex * m_pRenderState->m_sVertexDecl.m_uiVertexSize + sProperty.m_auiOffsets[i]));
		}
		// convert from full floats
		else if (sProperty.m_eFormat == VERTEX_FORMAT_FULL_FLOAT)
		{
			for (st_int32 i = 0; i < c_nNumComponents; ++i)
				ahfValues[i] = st_float16(st_float32(*reinterpret_cast<const st_float32*>(m_pVertexData + nVertex * m_pRenderState->m_sVertexDecl.m_uiVertexSize + sProperty.m_auiOffsets[i])));
		}
		// convert from bytes
		else if (sProperty.m_eFormat == VERTEX_FORMAT_BYTE)
		{
			for (st_int32 i = 0; i < c_nNumComponents; ++i)
				ahfValues[i] = st_float16(CCore::UncompressScalar(m_pVertexData[nVertex * m_pRenderState->m_sVertexDecl.m_uiVertexSize + sProperty.m_auiOffsets[i]]));
		}
		else
			bSuccess = false;
	}

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////
//  SDrawCall::GetProperty

ST_INLINE st_bool SDrawCall::GetProperty(EVertexProperty eProperty, st_int32 nVertex, st_byte abValues[4]) const
{
	st_bool bSuccess = false;

	assert(static_cast<const st_byte*>(m_pVertexData));
	assert(static_cast<const SRenderState*>(m_pRenderState));
	assert(m_pRenderState->m_sVertexDecl.m_uiVertexSize > 0);
	assert(nVertex > -1 && nVertex < m_nNumVertices);

	const SVertexDecl::SProperty& sProperty = m_pRenderState->m_sVertexDecl.m_asProperties[eProperty];
	const st_int32 c_nNumComponents = sProperty.NumComponents( );
	assert(c_nNumComponents <= VERTEX_COMPONENT_COUNT);
	if (c_nNumComponents > 0 && sProperty.m_eFormat == VERTEX_FORMAT_BYTE)
	{
		for (st_int32 i = 0; i < c_nNumComponents; ++i)
			abValues[i] = m_pVertexData[nVertex * m_pRenderState->m_sVertexDecl.m_uiVertexSize + sProperty.m_auiOffsets[i]];

		bSuccess = true;
	}

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////
//  SDrawCall::SetProperty

ST_INLINE st_bool SDrawCall::SetProperty(EVertexProperty eProperty, st_int32 nVertex, const st_float32 afValues[4])
{
	st_bool bSuccess = false;

	assert(static_cast<const st_byte*>(m_pVertexData));
	assert(static_cast<const SRenderState*>(m_pRenderState));
	assert(m_pRenderState->m_sVertexDecl.m_uiVertexSize > 0);
	assert(nVertex > -1 && nVertex < m_nNumVertices);

	const SVertexDecl::SProperty& sProperty = m_pRenderState->m_sVertexDecl.m_asProperties[eProperty];
	const st_int32 c_nNumComponents = sProperty.NumComponents( );
	assert(c_nNumComponents <= VERTEX_COMPONENT_COUNT);
	if (c_nNumComponents > 0 && sProperty.m_eFormat == VERTEX_FORMAT_FULL_FLOAT)
	{
		for (st_int32 i = 0; i < c_nNumComponents; ++i)
			*reinterpret_cast<st_float32*>((st_byte*) m_pVertexData + nVertex * m_pRenderState->m_sVertexDecl.m_uiVertexSize + sProperty.m_auiOffsets[i]) = afValues[i];

		bSuccess = true;
	}

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////
//  SDrawCall::SetProperty

ST_INLINE st_bool SDrawCall::SetProperty(EVertexProperty eProperty, st_int32 nVertex, const st_float16 ahfValues[4])
{
	st_bool bSuccess = false;

	assert(static_cast<const st_byte*>(m_pVertexData));
	assert(static_cast<const SRenderState*>(m_pRenderState));
	assert(m_pRenderState->m_sVertexDecl.m_uiVertexSize > 0);
	assert(nVertex > -1 && nVertex < m_nNumVertices);

	const SVertexDecl::SProperty& sProperty = m_pRenderState->m_sVertexDecl.m_asProperties[eProperty];
	const st_int32 c_nNumComponents = sProperty.NumComponents( );
	assert(c_nNumComponents <= VERTEX_COMPONENT_COUNT);
	if (c_nNumComponents > 0 && sProperty.m_eFormat == VERTEX_FORMAT_HALF_FLOAT)
	{
		for (st_int32 i = 0; i < c_nNumComponents; ++i)
			*reinterpret_cast<st_float16*>((st_byte*) m_pVertexData + nVertex * m_pRenderState->m_sVertexDecl.m_uiVertexSize + sProperty.m_auiOffsets[i]) = ahfValues[i];

		bSuccess = true;
	}

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////
//  SDrawCall::SetProperty

ST_INLINE st_bool SDrawCall::SetProperty(EVertexProperty eProperty, st_int32 nVertex, const st_byte abValues[4])
{
	st_bool bSuccess = false;

	assert(static_cast<const st_byte*>(m_pVertexData));
	assert(static_cast<const SRenderState*>(m_pRenderState));
	assert(m_pRenderState->m_sVertexDecl.m_uiVertexSize > 0);
	assert(nVertex > -1 && nVertex < m_nNumVertices);

	const SVertexDecl::SProperty& sProperty = m_pRenderState->m_sVertexDecl.m_asProperties[eProperty];
	const st_int32 c_nNumComponents = sProperty.NumComponents( );
	assert(c_nNumComponents <= VERTEX_COMPONENT_COUNT);
	if (c_nNumComponents > 0 && sProperty.m_eFormat == VERTEX_FORMAT_BYTE)
	{
		for (st_int32 i = 0; i < c_nNumComponents; ++i)
			((st_byte*) m_pVertexData)[nVertex * m_pRenderState->m_sVertexDecl.m_uiVertexSize + sProperty.m_auiOffsets[i]] = abValues[i];

		bSuccess = true;
	}

	return bSuccess;
}

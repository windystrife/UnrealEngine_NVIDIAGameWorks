///////////////////////////////////////////////////////////////////////  
//	Extents.inl
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
// CExtents::CExtents

ST_INLINE CExtents::CExtents( )
{
	Reset( );
}


/////////////////////////////////////////////////////////////////////
// CExtents::CExtents

ST_INLINE CExtents::CExtents(const st_float32 afExtents[6]) :
	m_vMin(afExtents),
	m_vMax(afExtents + 3)
{
}


/////////////////////////////////////////////////////////////////////
// CExtents::CExtents

ST_INLINE CExtents::CExtents(const Vec3& cMin, const Vec3& cMax) :
	m_vMin(cMin),
	m_vMax(cMax)
{
}


/////////////////////////////////////////////////////////////////////
// CExtents::~CExtents

ST_INLINE CExtents::~CExtents( )
{
}


/////////////////////////////////////////////////////////////////////
// CExtents::Reset

ST_INLINE void CExtents::Reset(void)
{
	m_vMin.Set(FLT_MAX, FLT_MAX, FLT_MAX);
	m_vMax.Set(-FLT_MAX, -FLT_MAX, -FLT_MAX);
}


/////////////////////////////////////////////////////////////////////
// CExtents::SetToZeros

ST_INLINE void CExtents::SetToZeros(void)
{
	m_vMin.Set(0.0f, 0.0f, 0.0f);
	m_vMax.Set(0.0f, 0.0f, 0.0f);
}


/////////////////////////////////////////////////////////////////////
// CExtents::Order
//
// If mins are not <= max values, then swap them

ST_INLINE void CExtents::Order(void)
{
	st_float32 fTmp;

	if (m_vMin.x > m_vMax.x)
	{
		fTmp = m_vMin.x;
		m_vMin.x = m_vMax.x;
		m_vMax.x = fTmp;
	}

	if (m_vMin.y > m_vMax.y)
	{
		fTmp = m_vMin.y;
		m_vMin.y = m_vMax.y;
		m_vMax.y = fTmp;
	}

	if (m_vMin.z > m_vMax.z)
	{
		fTmp = m_vMin.z;
		m_vMin.z = m_vMax.z;
		m_vMax.z = fTmp;
	}
}


/////////////////////////////////////////////////////////////////////
// CExtents::Valid

ST_INLINE bool CExtents::Valid(void) const
{
	return (m_vMin.x != FLT_MAX || 
			m_vMin.y != FLT_MAX || 
			m_vMin.z != FLT_MAX || 
			m_vMax.x != -FLT_MAX || 
			m_vMax.y != -FLT_MAX || 
			m_vMax.z != -FLT_MAX);
}


/////////////////////////////////////////////////////////////////////
// CExtents::ExpandAround

ST_INLINE void CExtents::ExpandAround(const st_float32 afPoint[3])
{
	m_vMin.x = (afPoint[0] < m_vMin.x) ? afPoint[0] : m_vMin.x;
	m_vMin.y = (afPoint[1] < m_vMin.y) ? afPoint[1] : m_vMin.y;
	m_vMin.z = (afPoint[2] < m_vMin.z) ? afPoint[2] : m_vMin.z;

	m_vMax.x = (afPoint[0] > m_vMax.x) ? afPoint[0] : m_vMax.x;
	m_vMax.y = (afPoint[1] > m_vMax.y) ? afPoint[1] : m_vMax.y;
	m_vMax.z = (afPoint[2] > m_vMax.z) ? afPoint[2] : m_vMax.z;
}


/////////////////////////////////////////////////////////////////////
// CExtents::ExpandAround

ST_INLINE void CExtents::ExpandAround(const Vec3& vPoint)
{
	m_vMin.x = (vPoint.x < m_vMin.x) ? vPoint.x : m_vMin.x;
	m_vMin.y = (vPoint.y < m_vMin.y) ? vPoint.y : m_vMin.y;
	m_vMin.z = (vPoint.z < m_vMin.z) ? vPoint.z : m_vMin.z;

	m_vMax.x = (vPoint.x > m_vMax.x) ? vPoint.x : m_vMax.x;
	m_vMax.y = (vPoint.y > m_vMax.y) ? vPoint.y : m_vMax.y;
	m_vMax.z = (vPoint.z > m_vMax.z) ? vPoint.z : m_vMax.z;
}


/////////////////////////////////////////////////////////////////////
// CExtents::ExpandAround

ST_INLINE void CExtents::ExpandAround(const Vec3& vPoint, float fRadius)
{
	Vec3 vPointMin(vPoint.x - fRadius, vPoint.y - fRadius, vPoint.z - fRadius);
	Vec3 vPointMax(vPoint.x + fRadius, vPoint.y + fRadius, vPoint.z + fRadius);

	m_vMin[0] = (vPointMin.x < m_vMin[0]) ? vPointMin.x : m_vMin[0];
	m_vMin[1] = (vPointMin.y < m_vMin[1]) ? vPointMin.y : m_vMin[1];
	m_vMin[2] = (vPointMin.z < m_vMin[2]) ? vPointMin.z : m_vMin[2];

	m_vMax[0] = (vPointMax.x > m_vMax[0]) ? vPointMax.x : m_vMax[0];
	m_vMax[1] = (vPointMax.y > m_vMax[1]) ? vPointMax.y : m_vMax[1];
	m_vMax[2] = (vPointMax.z > m_vMax[2]) ? vPointMax.z : m_vMax[2];
}


/////////////////////////////////////////////////////////////////////
// CExtents::ExpandAround

ST_INLINE void CExtents::ExpandAround(const CExtents& cOther)
{
	m_vMin[0] = (cOther.m_vMin[0] < m_vMin[0]) ? cOther.m_vMin[0] : m_vMin[0];
	m_vMin[1] = (cOther.m_vMin[1] < m_vMin[1]) ? cOther.m_vMin[1] : m_vMin[1];
	m_vMin[2] = (cOther.m_vMin[2] < m_vMin[2]) ? cOther.m_vMin[2] : m_vMin[2];

	m_vMax[0] = (cOther.m_vMax[0] > m_vMax[0]) ? cOther.m_vMax[0] : m_vMax[0];
	m_vMax[1] = (cOther.m_vMax[1] > m_vMax[1]) ? cOther.m_vMax[1] : m_vMax[1];
	m_vMax[2] = (cOther.m_vMax[2] > m_vMax[2]) ? cOther.m_vMax[2] : m_vMax[2];
}


/////////////////////////////////////////////////////////////////////
// CExtents::Scale

ST_INLINE void CExtents::Scale(st_float32 fScalar)
{
	m_vMin *= fScalar;
	m_vMax *= fScalar;
}


/////////////////////////////////////////////////////////////////////
// CExtents::Translate

ST_INLINE void CExtents::Translate(const Vec3& vTranslation)
{
	m_vMin += vTranslation;
	m_vMax += vTranslation;
}


/////////////////////////////////////////////////////////////////////
// CExtents::Orient

ST_INLINE void CExtents::Orient(const Vec3& vUp, const Vec3& vRight)
{
	// make all 8 box corners
	Vec3 aVertices[8] = 
	{
		Vec3(m_vMin.x, m_vMin.y, m_vMin.z),
		Vec3(m_vMax.x, m_vMin.y, m_vMin.z),
		Vec3(m_vMin.x, m_vMax.y, m_vMin.z),
		Vec3(m_vMax.x, m_vMax.y, m_vMin.z),
		Vec3(m_vMin.x, m_vMin.y, m_vMax.z),
		Vec3(m_vMax.x, m_vMin.y, m_vMax.z),
		Vec3(m_vMin.x, m_vMax.y, m_vMax.z),
		Vec3(m_vMax.x, m_vMax.y, m_vMax.z)
	};
	
	// derive right vector
    Vec3 vOut = vUp.Cross(vRight).Normalize( );
	if (!CCoordSys::IsLeftHanded( ) && CCoordSys::IsYAxisUp( ))
		vOut *= -1.0f;

	// build orientation matrix
    Mat3x3 mOrient(vRight, vOut, vUp);

	// add transformed points to new extents
	m_vMin.Set(FLT_MAX, FLT_MAX, FLT_MAX);
	m_vMax.Set(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	for (st_int32 i = 0; i < 8; ++i)
		ExpandAround(mOrient * aVertices[i]);
}


/////////////////////////////////////////////////////////////////////
// CExtents::Rotate

ST_INLINE void CExtents::Rotate(st_float32 fRadians)
{
	Mat3x3 mRot;
	CCoordSys::RotateUpAxis(mRot, fRadians);

	Vec3 vCorner1 = mRot * m_vMin;
	Vec3 vCorner2 = mRot * m_vMax;
	Vec3 vCorner3 = mRot * Vec3(m_vMin.x, m_vMax.y, m_vMin.z);
	Vec3 vCorner4 = mRot * Vec3(m_vMax.x, m_vMin.y, m_vMin.z);

	Reset( );
	ExpandAround(vCorner1);
	ExpandAround(vCorner2);
	ExpandAround(vCorner3);
	ExpandAround(vCorner4);
}


/////////////////////////////////////////////////////////////////////
// CExtents::ComputeRadiusFromCenter3D

ST_INLINE st_float32 CExtents::ComputeRadiusFromCenter3D(void) const
{
	return GetCenter( ).Distance(m_vMin);
}


/////////////////////////////////////////////////////////////////////
// CExtents::ComputeRadiusSquaredFromCenter3D

ST_INLINE st_float32 CExtents::ComputeRadiusSquaredFromCenter3D(void) const
{
	return GetCenter( ).DistanceSquared(m_vMin);
}


/////////////////////////////////////////////////////////////////////
// CExtents::ComputeRadiusFromCenter2D

ST_INLINE st_float32 CExtents::ComputeRadiusFromCenter2D(void) const
{
	Vec3 vCenter2D = CCoordSys::ConvertToStd(GetCenter( ));
	vCenter2D.z = 0.0f;
	Vec3 vMin2D = CCoordSys::ConvertToStd(Min( ));
	vMin2D.z = 0.0f;

	return vCenter2D.Distance(vMin2D);
}


/////////////////////////////////////////////////////////////////////
// CExtents::Min

ST_INLINE const Vec3& CExtents::Min(void) const
{
	return m_vMin;
}


/////////////////////////////////////////////////////////////////////
// CExtents::Max

ST_INLINE const Vec3& CExtents::Max(void) const
{
	return m_vMax;
}


/////////////////////////////////////////////////////////////////////
// CExtents::Midpoint

ST_INLINE st_float32 CExtents::Midpoint(st_uint32 uiAxis) const
{
	assert(uiAxis < 3);

	return 0.5f * (m_vMin[uiAxis] + m_vMax[uiAxis]);
}


/////////////////////////////////////////////////////////////////////
// CExtents::GetCenter

ST_INLINE Vec3 CExtents::GetCenter(void) const
{
	return (m_vMin + m_vMax) * 0.5f;
}


/////////////////////////////////////////////////////////////////////
// CExtents::GetDiagonal

ST_INLINE Vec3 CExtents::GetDiagonal(void) const
{
	return m_vMax - GetCenter( );
}


/////////////////////////////////////////////////////////////////////
// CExtents::GetHeight

ST_INLINE st_float32 CExtents::GetHeight(void) const
{
	return CCoordSys::UpComponent(m_vMax);
}


/////////////////////////////////////////////////////////////////////
// CExtents::operator float*

ST_INLINE CExtents::operator float*(void)
{
	return &(m_vMin.x);
}


/////////////////////////////////////////////////////////////////////
// CExtents::operator const st_float32*

ST_INLINE CExtents::operator const st_float32*(void) const
{
	return &(m_vMin.x);
}


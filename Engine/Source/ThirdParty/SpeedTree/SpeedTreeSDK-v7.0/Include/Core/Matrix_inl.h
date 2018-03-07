///////////////////////////////////////////////////////////////////////  
//  Matrix.inl
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
//  Mat3x3(bool)

ST_INLINE Mat3x3::Mat3x3(bool bSetToIdentity)
{
	if (bSetToIdentity)
		SetIdentity( );
}


///////////////////////////////////////////////////////////////////////  
//  Mat3x3(st_float32 [9])

ST_INLINE Mat3x3::Mat3x3(const st_float32 afInit[9])
{
	memcpy(m_afSingle, afInit, 9 * sizeof(st_float32));
}


///////////////////////////////////////////////////////////////////////  
//  Mat3x3(const Vec3& vRight, const Vec3& vOut, const Vec3& vUp)

ST_INLINE Mat3x3::Mat3x3(const Vec3& vRight, const Vec3& vOut, const Vec3& vUp)
{
	m_afSingle[0] = vRight.x;
	m_afSingle[3] = vRight.y;
	m_afSingle[6] = vRight.z;

	m_afSingle[1] = vOut.x;
	m_afSingle[4] = vOut.y;
	m_afSingle[7] = vOut.z;

	m_afSingle[2] = vUp.x;
	m_afSingle[5] = vUp.y;
	m_afSingle[8] = vUp.z;
}


///////////////////////////////////////////////////////////////////////  
//  Mat3x3::operator st_float32*

ST_INLINE Mat3x3::operator st_float32*(void)
{
	return m_afSingle;
}


///////////////////////////////////////////////////////////////////////  
//  Mat3x3::operator const st_float32*

ST_INLINE Mat3x3::operator const st_float32*(void) const
{
	return m_afSingle;
}


///////////////////////////////////////////////////////////////////////  
//  Mat3x3::operator*

ST_INLINE Mat3x3 Mat3x3::operator*(const Mat3x3& vIn) const
{
    Mat3x3 cTmp;

	for (int i = 0; i < 3; ++i)
	{
		cTmp.m_afRowCol[i][0] = m_afRowCol[i][0] * vIn.m_afRowCol[0][0] +
								m_afRowCol[i][1] * vIn.m_afRowCol[1][0] +
								m_afRowCol[i][2] * vIn.m_afRowCol[2][0];
		cTmp.m_afRowCol[i][1] = m_afRowCol[i][0] * vIn.m_afRowCol[0][1] +
								m_afRowCol[i][1] * vIn.m_afRowCol[1][1] +
								m_afRowCol[i][2] * vIn.m_afRowCol[2][1];
		cTmp.m_afRowCol[i][2] = m_afRowCol[i][0] * vIn.m_afRowCol[0][2] +
								m_afRowCol[i][1] * vIn.m_afRowCol[1][2] +
								m_afRowCol[i][2] * vIn.m_afRowCol[2][2];
	}

    return cTmp;
}


///////////////////////////////////////////////////////////////////////  
//  Mat3x3::operator*

ST_INLINE Vec3 Mat3x3::operator*(const Vec3& vIn) const
{
	return Vec3(m_afRowCol[0][0] * vIn.x + m_afRowCol[0][1] * vIn.y + m_afRowCol[0][2] * vIn.z,
				m_afRowCol[1][0] * vIn.x + m_afRowCol[1][1] * vIn.y + m_afRowCol[1][2] * vIn.z,
				m_afRowCol[2][0] * vIn.x + m_afRowCol[2][1] * vIn.y + m_afRowCol[2][2] * vIn.z);
}


///////////////////////////////////////////////////////////////////////  
//  Mat3x3::operator==

ST_INLINE bool Mat3x3::operator==(const Mat3x3& vIn) const
{
	return (memcmp(m_afSingle, vIn.m_afSingle, 9 * sizeof(st_float32)) == 0);
}


///////////////////////////////////////////////////////////////////////  
//  Mat3x3::operator!=

ST_INLINE bool Mat3x3::operator!=(const Mat3x3& vIn) const
{
	return (memcmp(m_afSingle, vIn.m_afSingle, 9 * sizeof(st_float32)) != 0);
}


///////////////////////////////////////////////////////////////////////  
//  Mat3x3::SetIdentity

ST_INLINE void Mat3x3::SetIdentity(void)
{
	memset(m_afSingle, 0, 9 * sizeof(st_float32));
	m_afSingle[0] = m_afSingle[4] = m_afSingle[8] = 1.0f;					
}


///////////////////////////////////////////////////////////////////////  
//  Mat3x3::Set

ST_INLINE void Mat3x3::Set(const st_float32 afValue[9])
{
	memcpy(m_afSingle, afValue, 9 * sizeof(st_float32));
}


///////////////////////////////////////////////////////////////////////  
//  Mat3x3::RotateX

ST_INLINE void Mat3x3::RotateX(st_float32 fRadians)
{
	Mat3x3 mRotate;

	st_float32 fCosine = cosf(fRadians);
	st_float32 fSine = sinf(fRadians);

	mRotate.m_afRowCol[1][1] = fCosine;
	mRotate.m_afRowCol[1][2] = fSine;
	mRotate.m_afRowCol[2][1] = -fSine;
	mRotate.m_afRowCol[2][2] = fCosine;

	// this function can be further optimized by hardcoding
	// the multiplication here and removing terms with 0.0 multipliers
	*this = mRotate * *this;
}


///////////////////////////////////////////////////////////////////////  
//  Mat3x3::RotateY

ST_INLINE void Mat3x3::RotateY(st_float32 fRadians)
{
	Mat3x3 mRotate;

	st_float32 fCosine = cosf(fRadians);
	st_float32 fSine = sinf(fRadians);

	mRotate.m_afRowCol[0][0] = fCosine;
	mRotate.m_afRowCol[0][2] = -fSine;
	mRotate.m_afRowCol[2][0] = fSine;
	mRotate.m_afRowCol[2][2] = fCosine;

	// this function can be further optimized by hardcoding
	// the multiplication here and removing terms with 0.0 multipliers
	*this = mRotate * *this;
}


///////////////////////////////////////////////////////////////////////  
//  Mat3x3::RotateZ

ST_INLINE void Mat3x3::RotateZ(st_float32 fRadians)
{
	Mat3x3 mRotate;

	st_float32 fCosine = cosf(fRadians);
	st_float32 fSine = sinf(fRadians);

	mRotate.m_afRowCol[0][0] = fCosine;
	mRotate.m_afRowCol[1][0] = -fSine;
	mRotate.m_afRowCol[0][1] = fSine;
	mRotate.m_afRowCol[1][1] = fCosine;

	// this function can be further optimized by hardcoding
	// the multiplication here and removing terms with 0.0 multipliers
	*this = mRotate * *this;
}


///////////////////////////////////////////////////////////////////////  
//  Mat3x3::RotateArbitrary

ST_INLINE void Mat3x3::RotateArbitrary(const Vec3& vAxis, st_float32 fRadians)
{
	st_float32 s = sinf(fRadians);
	st_float32 c = cosf(fRadians);
	st_float32 t = 1.0f - c;

	Vec3 vNormalizedAxis(vAxis);
	vNormalizedAxis.Normalize( );
	const st_float32& x = vNormalizedAxis.x;
	const st_float32& y = vNormalizedAxis.y;
	const st_float32& z = vNormalizedAxis.z;

	Mat3x3 mRotate;
	mRotate.m_afRowCol[0][0] = t * x * x + c;
	mRotate.m_afRowCol[0][1] = t * x * y + s * z;
	mRotate.m_afRowCol[0][2] = t * x * z - s * y;
	mRotate.m_afRowCol[1][0] = t * x * y - s * z;
	mRotate.m_afRowCol[1][1] = t * y * y + c;
	mRotate.m_afRowCol[1][2] = t * y * z + s * x;
	mRotate.m_afRowCol[2][0] = t * x * z + s * y;
	mRotate.m_afRowCol[2][1] = t * y * z - s * x;
	mRotate.m_afRowCol[2][2] = t * z * z + c;

	*this = mRotate * *this;
}


///////////////////////////////////////////////////////////////////////  
//  Mat3x3::Scale

ST_INLINE void Mat3x3::Scale(st_float32 x, st_float32 y, st_float32 z)
{
	m_afSingle[0] *= x;
	m_afSingle[1] *= x;
	m_afSingle[2] *= x;
	m_afSingle[3] *= y;
	m_afSingle[4] *= y;
	m_afSingle[5] *= y;
	m_afSingle[6] *= z;
	m_afSingle[7] *= z;
	m_afSingle[8] *= z;
}


///////////////////////////////////////////////////////////////////////  
//  Mat3x3::Scale

ST_INLINE void Mat3x3::Scale(const Vec3& vScalar)
{
	Scale(vScalar.x, vScalar.y, vScalar.z);
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4(bool)

ST_INLINE Mat4x4::Mat4x4(bool bSetToIdentity)
{
	if (bSetToIdentity)
		SetIdentity( );
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4(st_float32 [16])

ST_INLINE Mat4x4::Mat4x4(const st_float32 afInit[16])
{
	memcpy(m_afSingle, afInit, 16 * sizeof(st_float32));
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4(16 st_float32s)

ST_INLINE Mat4x4::Mat4x4(st_float32 m00, st_float32 m01, st_float32 m02, st_float32 m03,
					  st_float32 m10, st_float32 m11, st_float32 m12, st_float32 m13,
					  st_float32 m20, st_float32 m21, st_float32 m22, st_float32 m23,
					  st_float32 m30, st_float32 m31, st_float32 m32, st_float32 m33)
{
	st_float32* pMatrix = m_afSingle;

	*pMatrix++ = m00;
	*pMatrix++ = m01;
	*pMatrix++ = m02;
	*pMatrix++ = m03;
	*pMatrix++ = m10;
	*pMatrix++ = m11;
	*pMatrix++ = m12;
	*pMatrix++ = m13;
	*pMatrix++ = m20;
	*pMatrix++ = m21;
	*pMatrix++ = m22;
	*pMatrix++ = m23;
	*pMatrix++ = m30;
	*pMatrix++ = m31;
	*pMatrix++ = m32;
	*pMatrix = m33;
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::operator st_float32*

ST_INLINE Mat4x4::operator st_float32*(void)
{
	return m_afSingle;
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::operator const st_float32*

ST_INLINE Mat4x4::operator const st_float32*(void) const
{
	return m_afSingle;
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::operator*

ST_INLINE Mat4x4 Mat4x4::operator*(const Mat4x4& vIn) const
{
	Mat4x4 cTmp;

	for (int i = 0; i < 4; ++i)
	{
		cTmp.m_afRowCol[i][0] = m_afRowCol[i][0] * vIn.m_afRowCol[0][0] +
								m_afRowCol[i][1] * vIn.m_afRowCol[1][0] +
								m_afRowCol[i][2] * vIn.m_afRowCol[2][0] +
								m_afRowCol[i][3] * vIn.m_afRowCol[3][0];
		cTmp.m_afRowCol[i][1] = m_afRowCol[i][0] * vIn.m_afRowCol[0][1] +
								m_afRowCol[i][1] * vIn.m_afRowCol[1][1] +
								m_afRowCol[i][2] * vIn.m_afRowCol[2][1] +
								m_afRowCol[i][3] * vIn.m_afRowCol[3][1];
		cTmp.m_afRowCol[i][2] = m_afRowCol[i][0] * vIn.m_afRowCol[0][2] +
								m_afRowCol[i][1] * vIn.m_afRowCol[1][2] +
								m_afRowCol[i][2] * vIn.m_afRowCol[2][2] +
								m_afRowCol[i][3] * vIn.m_afRowCol[3][2];
		cTmp.m_afRowCol[i][3] = m_afRowCol[i][0] * vIn.m_afRowCol[0][3] +
								m_afRowCol[i][1] * vIn.m_afRowCol[1][3] +
								m_afRowCol[i][2] * vIn.m_afRowCol[2][3] +
								m_afRowCol[i][3] * vIn.m_afRowCol[3][3];
	}

	return cTmp;
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::operator*

ST_INLINE Mat4x4 Mat4x4::operator*=(const Mat4x4& vIn)
{
	for (int i = 0; i < 4; ++i)
	{
		m_afRowCol[i][0] = m_afRowCol[i][0] * vIn.m_afRowCol[0][0] +
						   m_afRowCol[i][1] * vIn.m_afRowCol[1][0] +
						   m_afRowCol[i][2] * vIn.m_afRowCol[2][0] +
						   m_afRowCol[i][3] * vIn.m_afRowCol[3][0];
		m_afRowCol[i][1] = m_afRowCol[i][0] * vIn.m_afRowCol[0][1] +
						   m_afRowCol[i][1] * vIn.m_afRowCol[1][1] +
						   m_afRowCol[i][2] * vIn.m_afRowCol[2][1] +
						   m_afRowCol[i][3] * vIn.m_afRowCol[3][1];
		m_afRowCol[i][2] = m_afRowCol[i][0] * vIn.m_afRowCol[0][2] +
						   m_afRowCol[i][1] * vIn.m_afRowCol[1][2] +
						   m_afRowCol[i][2] * vIn.m_afRowCol[2][2] +
						   m_afRowCol[i][3] * vIn.m_afRowCol[3][2];
		m_afRowCol[i][3] = m_afRowCol[i][0] * vIn.m_afRowCol[0][3] +
						   m_afRowCol[i][1] * vIn.m_afRowCol[1][3] +
						   m_afRowCol[i][2] * vIn.m_afRowCol[2][3] +
						   m_afRowCol[i][3] * vIn.m_afRowCol[3][3];
	}

	return *this;
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::operator*

ST_INLINE Vec3 Mat4x4::operator*(const Vec3& vIn) const
{
	return Vec3(m_afSingle[0] * vIn.x + m_afSingle[4] * vIn.y + m_afSingle[8] * vIn.z + m_afSingle[12],
				m_afSingle[1] * vIn.x + m_afSingle[5] * vIn.y + m_afSingle[9] * vIn.z + m_afSingle[13],
				m_afSingle[2] * vIn.x + m_afSingle[6] * vIn.y + m_afSingle[10] * vIn.z + m_afSingle[14]);
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::operator*

ST_INLINE Vec4 Mat4x4::operator*(const Vec4& vIn) const
{
	return Vec4(m_afSingle[0] * vIn.x + m_afSingle[4] * vIn.y + m_afSingle[8] * vIn.z + m_afSingle[12] * vIn.w,
				m_afSingle[1] * vIn.x + m_afSingle[5] * vIn.y + m_afSingle[9] * vIn.z + m_afSingle[13] * vIn.w,
				m_afSingle[2] * vIn.x + m_afSingle[6] * vIn.y + m_afSingle[10] * vIn.z + m_afSingle[14] * vIn.w,
			    m_afSingle[3] * vIn.x + m_afSingle[7] * vIn.y + m_afSingle[11] * vIn.z + m_afSingle[15] * vIn.w);
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::operator==

ST_INLINE bool Mat4x4::operator==(const Mat4x4& vIn) const
{
	return (memcmp(m_afSingle, vIn.m_afSingle, 16 * sizeof(st_float32)) == 0);
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::operator!=

ST_INLINE bool Mat4x4::operator!=(const Mat4x4& vIn) const
{
	return (memcmp(m_afSingle, vIn.m_afSingle, 16 * sizeof(st_float32)) != 0);
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::operator=

ST_INLINE Mat4x4& Mat4x4::operator=(const Mat4x4& vIn)
{
	memcpy(m_afSingle, vIn.m_afSingle, sizeof(m_afSingle));

	return *this;
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::SetIdentity

ST_INLINE void Mat4x4::SetIdentity(void)
{
	memset(m_afSingle, 0, 16 * sizeof(st_float32));
	m_afSingle[0] = m_afSingle[5] = m_afSingle[10] = m_afSingle[15] = 1.0f;
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::Set

ST_INLINE void Mat4x4::Set(const st_float32 afValue[16])
{
	memcpy(m_afSingle, afValue, 16 * sizeof(st_float32));
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::Set

ST_INLINE void Mat4x4::Set(st_float32 m00, st_float32 m01, st_float32 m02, st_float32 m03,
						st_float32 m10, st_float32 m11, st_float32 m12, st_float32 m13,
						st_float32 m20, st_float32 m21, st_float32 m22, st_float32 m23,
						st_float32 m30, st_float32 m31, st_float32 m32, st_float32 m33)
{
	st_float32* pMatrix = m_afSingle;

	*pMatrix++ = m00;
	*pMatrix++ = m01;
	*pMatrix++ = m02;
	*pMatrix++ = m03;
	*pMatrix++ = m10;
	*pMatrix++ = m11;
	*pMatrix++ = m12;
	*pMatrix++ = m13;
	*pMatrix++ = m20;
	*pMatrix++ = m21;
	*pMatrix++ = m22;
	*pMatrix++ = m23;
	*pMatrix++ = m30;
	*pMatrix++ = m31;
	*pMatrix++ = m32;
	*pMatrix = m33;
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::GetVectorComponents

ST_INLINE void Mat4x4::GetVectorComponents(Vec3& vUp, Vec3& vOut, Vec3& vRight)
{
	vRight.Set(m_afSingle[0], m_afSingle[4], m_afSingle[8]);
	vUp.Set(m_afSingle[1], m_afSingle[5], m_afSingle[9]);
	vOut.Set(m_afSingle[2], m_afSingle[6], m_afSingle[10]);
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::Invert

ST_INLINE bool Mat4x4::Invert(Mat4x4& mResult) const
{
	const st_float32 c_fTiny = 1.0e-20f;

	st_float32 fA0 = m_afSingle[ 0] * m_afSingle[ 5] - m_afSingle[ 1] * m_afSingle[ 4];
	st_float32 fA1 = m_afSingle[ 0] * m_afSingle[ 6] - m_afSingle[ 2] * m_afSingle[ 4];
	st_float32 fA2 = m_afSingle[ 0] * m_afSingle[ 7] - m_afSingle[ 3] * m_afSingle[ 4];
	st_float32 fA3 = m_afSingle[ 1] * m_afSingle[ 6] - m_afSingle[ 2] * m_afSingle[ 5];
	st_float32 fA4 = m_afSingle[ 1] * m_afSingle[ 7] - m_afSingle[ 3] * m_afSingle[ 5];
	st_float32 fA5 = m_afSingle[ 2] * m_afSingle[ 7] - m_afSingle[ 3] * m_afSingle[ 6];
	st_float32 fB0 = m_afSingle[ 8] * m_afSingle[13] - m_afSingle[ 9] * m_afSingle[12];
	st_float32 fB1 = m_afSingle[ 8] * m_afSingle[14] - m_afSingle[10] * m_afSingle[12];
	st_float32 fB2 = m_afSingle[ 8] * m_afSingle[15] - m_afSingle[11] * m_afSingle[12];
	st_float32 fB3 = m_afSingle[ 9] * m_afSingle[14] - m_afSingle[10] * m_afSingle[13];
	st_float32 fB4 = m_afSingle[ 9] * m_afSingle[15] - m_afSingle[11] * m_afSingle[13];
	st_float32 fB5 = m_afSingle[10] * m_afSingle[15] - m_afSingle[11] * m_afSingle[14];

	st_float32 fDet = fA0 * fB5 - fA1 * fB4 + fA2 * fB3 + fA3 * fB2 - fA4 * fB1 + fA5 * fB0;
	if (fabs(fDet) <= c_fTiny)
		return false;

	mResult.SetIdentity( );
	mResult.m_afSingle[ 0] =  m_afSingle[ 5] * fB5 - m_afSingle[ 6] * fB4 + m_afSingle[ 7] * fB3;
	mResult.m_afSingle[ 4] = -m_afSingle[ 4] * fB5 + m_afSingle[ 6] * fB2 - m_afSingle[ 7] * fB1;
	mResult.m_afSingle[ 8] =  m_afSingle[ 4] * fB4 - m_afSingle[ 5] * fB2 + m_afSingle[ 7] * fB0;
	mResult.m_afSingle[12] = -m_afSingle[ 4] * fB3 + m_afSingle[ 5] * fB1 - m_afSingle[ 6] * fB0;
	mResult.m_afSingle[ 1] = -m_afSingle[ 1] * fB5 + m_afSingle[ 2] * fB4 - m_afSingle[ 3] * fB3;
	mResult.m_afSingle[ 5] =  m_afSingle[ 0] * fB5 - m_afSingle[ 2] * fB2 + m_afSingle[ 3] * fB1;
	mResult.m_afSingle[ 9] = -m_afSingle[ 0] * fB4 + m_afSingle[ 1] * fB2 - m_afSingle[ 3] * fB0;
	mResult.m_afSingle[13] =  m_afSingle[ 0] * fB3 - m_afSingle[ 1] * fB1 + m_afSingle[ 2] * fB0;
	mResult.m_afSingle[ 2] =  m_afSingle[13] * fA5 - m_afSingle[14] * fA4 + m_afSingle[15] * fA3;
	mResult.m_afSingle[ 6] = -m_afSingle[12] * fA5 + m_afSingle[14] * fA2 - m_afSingle[15] * fA1;
	mResult.m_afSingle[10] =  m_afSingle[12] * fA4 - m_afSingle[13] * fA2 + m_afSingle[15] * fA0;
	mResult.m_afSingle[14] = -m_afSingle[12] * fA3 + m_afSingle[13] * fA1 - m_afSingle[14] * fA0;
	mResult.m_afSingle[ 3] = -m_afSingle[ 9] * fA5 + m_afSingle[10] * fA4 - m_afSingle[11] * fA3;
	mResult.m_afSingle[ 7] =  m_afSingle[ 8] * fA5 - m_afSingle[10] * fA2 + m_afSingle[11] * fA1;
	mResult.m_afSingle[11] = -m_afSingle[ 8] * fA4 + m_afSingle[ 9] * fA2 - m_afSingle[11] * fA0;
	mResult.m_afSingle[15] =  m_afSingle[ 8] * fA3 - m_afSingle[ 9] * fA1 + m_afSingle[10] * fA0;

	st_float32 fInvDet = 1.0f / fDet;
	mResult.m_afSingle[ 0] *= fInvDet;
	mResult.m_afSingle[ 1] *= fInvDet;
	mResult.m_afSingle[ 2] *= fInvDet;
	mResult.m_afSingle[ 3] *= fInvDet;
	mResult.m_afSingle[ 4] *= fInvDet;
	mResult.m_afSingle[ 5] *= fInvDet;
	mResult.m_afSingle[ 6] *= fInvDet;
	mResult.m_afSingle[ 7] *= fInvDet;
	mResult.m_afSingle[ 8] *= fInvDet;
	mResult.m_afSingle[ 9] *= fInvDet;
	mResult.m_afSingle[10] *= fInvDet;
	mResult.m_afSingle[11] *= fInvDet;
	mResult.m_afSingle[12] *= fInvDet;
	mResult.m_afSingle[13] *= fInvDet;
	mResult.m_afSingle[14] *= fInvDet;
	mResult.m_afSingle[15] *= fInvDet;

	return true;
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::Multiply4f

ST_INLINE void Mat4x4::Multiply4f(const st_float32 afIn[4], st_float32 afResult[4]) const
{
	afResult[0] = m_afRowCol[0][0] * afIn[0] + m_afRowCol[0][1] * afIn[1] + m_afRowCol[0][2] * afIn[2] + m_afRowCol[0][3] * afIn[3];
	afResult[1] = m_afRowCol[1][0] * afIn[0] + m_afRowCol[1][1] * afIn[1] + m_afRowCol[1][2] * afIn[2] + m_afRowCol[1][3] * afIn[3];
	afResult[2] = m_afRowCol[2][0] * afIn[0] + m_afRowCol[2][1] * afIn[1] + m_afRowCol[2][2] * afIn[2] + m_afRowCol[2][3] * afIn[3];
	afResult[3] = m_afRowCol[3][0] * afIn[0] + m_afRowCol[3][1] * afIn[1] + m_afRowCol[3][2] * afIn[2] + m_afRowCol[3][3] * afIn[3];
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::RotateX

ST_INLINE void Mat4x4::RotateX(st_float32 fRadians)
{
	st_float32 fCosine = cosf(fRadians);
	st_float32 fSine = sinf(fRadians);

	Mat4x4 mTmp;
	mTmp.m_afRowCol[1][1] = fCosine;
	mTmp.m_afRowCol[1][2] = fSine;
	mTmp.m_afRowCol[2][1] = -fSine;
	mTmp.m_afRowCol[2][2] = fCosine;

	// this function can be further optimized by hardcoding
	// the multiplication here and removing terms with 0.0 multipliers
	*this = mTmp * *this;
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::RotateY

ST_INLINE void Mat4x4::RotateY(st_float32 fRadians)
{
	st_float32 fCosine = cosf(fRadians);
	st_float32 fSine = sinf(fRadians);

	Mat4x4 mTmp;
	mTmp.m_afRowCol[0][0] = fCosine;
	mTmp.m_afRowCol[0][2] = -fSine;
	mTmp.m_afRowCol[2][0] = fSine;
	mTmp.m_afRowCol[2][2] = fCosine;

	// this function can be further optimized by hardcoding
	// the multiplication here and removing terms with 0.0 multipliers
	*this = mTmp * *this;
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::RotateZ

ST_INLINE void Mat4x4::RotateZ(st_float32 fRadians)
{
	st_float32 fCosine = cosf(fRadians);
	st_float32 fSine = sinf(fRadians);

	Mat4x4 mTmp;
	mTmp.m_afRowCol[0][0] = fCosine;
	mTmp.m_afRowCol[1][0] = -fSine;
	mTmp.m_afRowCol[0][1] = fSine;
	mTmp.m_afRowCol[1][1] = fCosine;

	// this function can be further optimized by hardcoding
	// the multiplication here and removing terms with 0.0 multipliers
	*this = mTmp * *this;
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::RotateArbitrary

ST_INLINE void Mat4x4::RotateArbitrary(const Vec3& vAxis, st_float32 fRadians)
{
	st_float32 s = sinf(fRadians);
	st_float32 c = cosf(fRadians);
	st_float32 t = 1.0f - c;

	Vec3 vNormalizedAxis(vAxis);
	vNormalizedAxis.Normalize( );
	const st_float32& x = vNormalizedAxis.x;
	const st_float32& y = vNormalizedAxis.y;
	const st_float32& z = vNormalizedAxis.z;

	Mat4x4 mTmp;
	mTmp.m_afRowCol[0][0] = t * x * x + c;
	mTmp.m_afRowCol[0][1] = t * x * y + s * z;
	mTmp.m_afRowCol[0][2] = t * x * z - s * y;
	mTmp.m_afRowCol[1][0] = t * x * y - s * z;
	mTmp.m_afRowCol[1][1] = t * y * y + c;
	mTmp.m_afRowCol[1][2] = t * y * z + s * x;
	mTmp.m_afRowCol[2][0] = t * x * z + s * y;
	mTmp.m_afRowCol[2][1] = t * y * z - s * x;
	mTmp.m_afRowCol[2][2] = t * z * z + c;

	*this = mTmp * *this;
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::Scale

ST_INLINE void Mat4x4::Scale(st_float32 x, st_float32 y, st_float32 z)
{
	Mat4x4 mTmp;

	mTmp.m_afRowCol[0][0] = x;
	mTmp.m_afRowCol[1][1] = y;
	mTmp.m_afRowCol[2][2] = z;

	*this = mTmp * *this;
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::Scale

ST_INLINE void Mat4x4::Scale(const Vec3& vScalar)
{
	Scale(vScalar.x, vScalar.y, vScalar.z);
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::Translate

ST_INLINE void Mat4x4::Translate(st_float32 x, st_float32 y, st_float32 z)
{
	Mat4x4 mTmp;

	mTmp[12] = x;
	mTmp[13] = y;
	mTmp[14] = z;

	*this = mTmp * *this;
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::Translate

ST_INLINE void Mat4x4::Translate(const Vec3& vTranslate)
{
	Translate(vTranslate.x, vTranslate.y, vTranslate.z);
}


////////////////////////////////////////////////////////////
// MatSwap

template <class T> ST_INLINE void MatSwap(T& tA, T& tB)
{
	T tTemp = tA;
	tA = tB; 
	tB = tTemp;
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::Transpose

ST_INLINE Mat4x4 Mat4x4::Transpose(void) const
{
	Mat4x4 mResult = *this;

	MatSwap(mResult.m_afSingle[1], mResult.m_afSingle[4]);
	MatSwap(mResult.m_afSingle[2], mResult.m_afSingle[8]);
	MatSwap(mResult.m_afSingle[6], mResult.m_afSingle[9]);
	MatSwap(mResult.m_afSingle[3], mResult.m_afSingle[12]);
	MatSwap(mResult.m_afSingle[7], mResult.m_afSingle[13]);
	MatSwap(mResult.m_afSingle[11], mResult.m_afSingle[14]);			

	return mResult;
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::LookAt

ST_INLINE void Mat4x4::LookAt(const Vec3& vEye, const Vec3& vCenter, const Vec3& vUp)
{
	Vec3 vF(vCenter - vEye);
	vF.Normalize( );

	Vec3 vUpPrime(vUp);
	vUpPrime.Normalize( );

	Vec3 vS = vF.Cross(vUpPrime);
	Vec3 vU = vS.Cross(vF);

	Mat4x4 mTemp;
	mTemp.m_afRowCol[0][0] = vS[0];
	mTemp.m_afRowCol[1][0] = vS[1];
	mTemp.m_afRowCol[2][0] = vS[2];

	mTemp.m_afRowCol[0][1] = vU[0];
	mTemp.m_afRowCol[1][1] = vU[1];
	mTemp.m_afRowCol[2][1] = vU[2];

	mTemp.m_afRowCol[0][2] = -vF[0];
	mTemp.m_afRowCol[1][2] = -vF[1];
	mTemp.m_afRowCol[2][2] = -vF[2];

	*this = *this * mTemp;

	Translate(-vEye);
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::Ortho

ST_INLINE void Mat4x4::Ortho(st_float32 fLeft, st_float32 fRight, st_float32 fBottom, st_float32 fTop, st_float32 fNear, st_float32 fFar, bool bOpenGL)
{
	Mat4x4 mTemp;
	if (bOpenGL)
	{
		// mimic glOrtho
		mTemp.m_afSingle[0] = 2.0f / (fRight - fLeft);
		mTemp.m_afSingle[5] = 2.0f / (fTop - fBottom);
		mTemp.m_afSingle[10] = -2.0f / (fFar - fNear);
		mTemp.m_afSingle[12] = -(fRight + fLeft) / (fRight - fLeft);
		mTemp.m_afSingle[13] = -(fTop + fBottom) / (fTop - fBottom);
		mTemp.m_afSingle[14] = -(fFar + fNear) / (fFar - fNear);
		mTemp.m_afSingle[15] = 1.0f;
	}
	else
	{
		// as defined in "Real-Time Rendering" by Akenine-Moller & Eric Haines
		//mTemp.m_afSingle[0] = 2.0f / (fRight - fLeft);
		//mTemp.m_afSingle[5] = 2.0f / (fTop - fBottom);
		//mTemp.m_afSingle[10] = -1.0f / (fFar - fNear);
		//mTemp.m_afSingle[12] = -(fRight + fLeft) / (fRight - fLeft);
		//mTemp.m_afSingle[13] = -(fTop + fBottom) / (fTop - fBottom);
		//mTemp.m_afSingle[14] = -fNear / (fFar - fNear);
		//mTemp.m_afSingle[15] = 1.0f;

		// as defined in D3DXMatrixOrthoOffCenterRH documentation
		mTemp.m_afSingle[0] = 2.0f / (fRight - fLeft);
		mTemp.m_afSingle[5] = 2.0f / (fTop - fBottom);
		mTemp.m_afSingle[10] = 1.0f / (fNear - fFar);
		mTemp.m_afSingle[12] = (fLeft + fRight) / (fLeft - fRight);
		mTemp.m_afSingle[13] = (fTop + fBottom) / (fBottom - fTop);
		mTemp.m_afSingle[14] = fNear / (fNear - fFar);
		mTemp.m_afSingle[15] = 1.0f;
	}

	*this = *this * mTemp;
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::Frustum

ST_INLINE void Mat4x4::Frustum(st_float32 fLeft, st_float32 fRight, st_float32 fBottom, st_float32 fTop, st_float32 fNear, st_float32 fFar)
{
	Mat4x4 mTemp;

	st_float32 a = (fRight + fLeft) / (fRight - fLeft);
	st_float32 b = (fTop + fBottom) / (fTop - fBottom);
	st_float32 c = -(fFar + fNear) / (fFar - fNear);
	st_float32 d = -(2.0f * (fFar * fNear)) / (fFar - fNear);

	mTemp.m_afSingle[0] = 2.0f * fNear / (fRight - fLeft);
	mTemp.m_afSingle[5] = 2.0f * fNear / (fTop - fBottom);
	mTemp.m_afSingle[8] = a;
	mTemp.m_afSingle[9] = b;
	mTemp.m_afSingle[10] = c;
	mTemp.m_afSingle[11] = -1.0f;
	mTemp.m_afSingle[14] = d;
	mTemp.m_afSingle[15] = 0.0f;

	*this = *this * mTemp;
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::Perspective

ST_INLINE void Mat4x4::Perspective(st_float32 fFieldOfView, st_float32 fAspectRatio, st_float32 fNear, st_float32 fFar)
{
	Mat4x4 mTemp;

	st_float32 f = 1.0f / tan(0.5f * fFieldOfView);

	mTemp.m_afSingle[0] = f / fAspectRatio;
	mTemp.m_afSingle[5] = f;
	mTemp.m_afSingle[10] = (fFar + fNear) / (fNear - fFar);
	mTemp.m_afSingle[11] = -1.0f;
	mTemp.m_afSingle[14] = 2.0f * fFar * fNear / (fNear - fFar);
	mTemp.m_afSingle[15] = 0.0f;

	*this = *this * mTemp;
}


///////////////////////////////////////////////////////////////////////  
//  Mat4x4::AdjustPerspectiveNearAndFar

ST_INLINE void Mat4x4::AdjustPerspectiveNearAndFar(st_float32 fNear, st_float32 fFar)
{
	m_afSingle[10] = (fFar + fNear) / (fNear - fFar);
	m_afSingle[14] = 2.0f * fFar * fNear / (fNear - fFar);
}







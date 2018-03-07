///////////////////////////////////////////////////////////////////////  
//  Vector.inl
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
//  Vec2::Vec2

ST_INLINE Vec2::Vec2( ) :
	x(0.0f),
	y(0.0f)
{
}


///////////////////////////////////////////////////////////////////////  
//  Vec2::Vec2

ST_INLINE Vec2::Vec2(st_float32 _x, st_float32 _y) :
	x(_x),
	y(_y)
{
}


///////////////////////////////////////////////////////////////////////  
//  Vec2::Vec2

ST_INLINE Vec2::Vec2(const st_float32 afPos[2]) :
	x(afPos[0]),
	y(afPos[1])
{
}


///////////////////////////////////////////////////////////////////////  
//  Vec2::operator+

ST_INLINE Vec2 Vec2::operator+(const Vec2& vIn) const
{
	return Vec2(x + vIn.x, y + vIn.y);
}


///////////////////////////////////////////////////////////////////////  
//  Vec2::operator+=

ST_INLINE Vec2 Vec2::operator+=(const Vec2& vIn)
{
	x += vIn.x;
	y += vIn.y;

	return *this;
}


///////////////////////////////////////////////////////////////////////  
//  Vec2::operator==

ST_INLINE st_bool Vec2::operator==(const Vec2& vIn) const
{
    return (vIn.x == x && vIn.y == y);
}


///////////////////////////////////////////////////////////////////////  
//  Vec2::Set

ST_INLINE void Vec2::Set(st_float32 _x, st_float32 _y)
{
    x = _x;
    y = _y;
}


///////////////////////////////////////////////////////////////////////  
//  Vec2::Distance

ST_INLINE st_float32 Vec2::Distance(const Vec2& vIn) const
{
    return sqrt((vIn.x - x) * (vIn.x - x) + (vIn.y - y) * (vIn.y - y));
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::Vec3

ST_INLINE Vec3::Vec3( ) :
	x(0.0f),
	y(0.0f),
	z(0.0f)
{
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::Vec3

ST_INLINE Vec3::Vec3(st_float32 _x, st_float32 _y, st_float32 _z) :
	x(_x),
	y(_y),
	z(_z)
{
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::Vec3

ST_INLINE Vec3::Vec3(st_float32 _x, st_float32 _y) :
	x(_x),
	y(_y),
	z(0.0f)
{
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::Vec3

ST_INLINE Vec3::Vec3(const st_float32 afPos[3]) :
	x(afPos[0]),
	y(afPos[1]),
	z(afPos[2])
{
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::operator[]

ST_INLINE st_float32& Vec3::operator[](int nIndex)
{
	return *(&x + nIndex);
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::operator st_float32*

ST_INLINE Vec3::operator st_float32*(void)
{
	return &x;
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::operator st_float32*

ST_INLINE Vec3::operator const st_float32*(void) const
{
	return &x;
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::operator<

ST_INLINE bool Vec3::operator<(const Vec3& vIn) const
{
	if (x == vIn.x)
	{
		if (y == vIn.y)
			return z < vIn.z;
		else 
			return y < vIn.y;
	}
	else
		return x < vIn.x;
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::operator==

ST_INLINE bool Vec3::operator==(const Vec3& vIn) const
{
	return (x == vIn.x && y == vIn.y && z == vIn.z);
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::operator!=

ST_INLINE bool Vec3::operator!=(const Vec3& vIn) const
{
	return (x != vIn.x || y != vIn.y || z != vIn.z);	
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::operator-

ST_INLINE Vec3 Vec3::operator-(const Vec3& vIn) const
{
	return Vec3(x - vIn.x, y - vIn.y, z - vIn.z);
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::operator+

ST_INLINE Vec3 Vec3::operator+(const Vec3& vIn) const
{
	return Vec3(x + vIn.x, y + vIn.y, z + vIn.z);
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::operator+=

ST_INLINE Vec3 Vec3::operator+=(const Vec3& vIn)
{
	x += vIn.x;
	y += vIn.y;
	z += vIn.z;

	return *this;
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::operator*

ST_INLINE Vec3 Vec3::operator*(const Vec3& vIn) const
{
	return Vec3(x * vIn.x, y * vIn.y, z * vIn.z);
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::operator*=

ST_INLINE Vec3 Vec3::operator*=(const Vec3& vIn)
{
	x *= vIn.x;
	y *= vIn.y;
	z *= vIn.z;

	return *this;
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::operator/

ST_INLINE Vec3 Vec3::operator/(const Vec3& vIn) const
{
	return Vec3(x / vIn.x, y / vIn.y, z / vIn.z);
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::operator/=

ST_INLINE Vec3 Vec3::operator/=(const Vec3& vIn)
{
	x /= vIn.x;
	y /= vIn.y;
	z /= vIn.z;

	return *this;
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::operator*

ST_INLINE Vec3 Vec3::operator*(st_float32 fScalar) const
{
	return Vec3(x * fScalar, y * fScalar, z * fScalar);
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::operator*=

ST_INLINE Vec3 Vec3::operator*=(st_float32 fScalar)
{
	x *= fScalar;
	y *= fScalar;
	z *= fScalar;

	return *this;
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::operator/

ST_INLINE Vec3 Vec3::operator/(st_float32 fDivisor) const
{
	return Vec3(x / fDivisor, y / fDivisor, z / fDivisor);
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::operator/=

ST_INLINE Vec3 Vec3::operator/=(st_float32 fDivisor)
{
	x /= fDivisor;
	y /= fDivisor;
	z /= fDivisor;

	return *this;
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::operator-

ST_INLINE Vec3 Vec3::operator-(void) const
{
	return Vec3(-x, -y, -z);
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::Set

ST_INLINE void Vec3::Set(st_float32 _x, st_float32 _y, st_float32 _z)
{
	x = _x;
	y = _y;
	z = _z;
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::Set

ST_INLINE void Vec3::Set(st_float32 _x, st_float32 _y)
{
	x = _x;
	y = _y;
	z = 0.0f;
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::Set

ST_INLINE void Vec3::Set(const st_float32 afPos[3])
{
	x = afPos[0];
	y = afPos[1];
	z = afPos[2];
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::Cross

ST_INLINE Vec3 Vec3::Cross(const Vec3& vIn) const
{
	return Vec3(y * vIn.z - z * vIn.y, z * vIn.x - x * vIn.z, x * vIn.y - y * vIn.x);
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::Distance

ST_INLINE st_float32 Vec3::Distance(const Vec3& vIn) const
{
	return ((*this - vIn).Magnitude( ));	
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::DistanceSquared

ST_INLINE st_float32 Vec3::DistanceSquared(const Vec3& vIn) const
{
	return (*this - vIn).MagnitudeSquared( );	
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::Dot

ST_INLINE st_float32 Vec3::Dot(const Vec3& vIn) const
{
	return (x * vIn.x + y * vIn.y + z * vIn.z);	
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::Magnitude

ST_INLINE st_float32 Vec3::Magnitude(void) const
{
	return st_float32(sqrt(MagnitudeSquared( )));
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::MagnitudeSquared

ST_INLINE st_float32 Vec3::MagnitudeSquared(void) const
{
	return (x * x + y * y + z * z);	
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::Normalize

ST_INLINE Vec3 Vec3::Normalize(void)
{
	st_float32 fMagnitude = Magnitude( );
	if (fMagnitude > FLT_EPSILON)
		Scale(1.0f / fMagnitude);

	return *this;
}


///////////////////////////////////////////////////////////////////////  
//  Vec3::Scale

ST_INLINE void Vec3::Scale(st_float32 fScalar)
{
	x *= fScalar;
	y *= fScalar;
	z *= fScalar;
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::Vec4

ST_INLINE Vec4::Vec4( ) :
	x(0.0f),
	y(0.0f),
	z(0.0f),
	w(1.0f)
{
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::Vec4

ST_INLINE Vec4::Vec4(st_float32 _x, st_float32 _y, st_float32 _z, st_float32 _w) :
	x(_x),
	y(_y),
	z(_z),
	w(_w)
{
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::Vec4

ST_INLINE Vec4::Vec4(st_float32 _x, st_float32 _y, st_float32 _z) :
	x(_x),
	y(_y),
	z(_z),
	w(1.0f)
{
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::Vec4

ST_INLINE Vec4::Vec4(st_float32 _x, st_float32 _y) :
	x(_x),
	y(_y),
	z(0.0f),
	w(1.0f)
{
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::Vec4

ST_INLINE Vec4::Vec4(const Vec3& vVec, st_float32 _w) :
	x(vVec.x),
	y(vVec.y),
	z(vVec.z),
	w(_w)
{
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::Vec4

ST_INLINE Vec4::Vec4(const st_float32 afPos[4]) :
	x(afPos[0]),
	y(afPos[1]),
	z(afPos[2]),
	w(afPos[3])
{
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::operator[]

ST_INLINE st_float32& Vec4::operator[](int nIndex)
{
	return *(&x + nIndex);
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::operator st_float32*

ST_INLINE Vec4::operator st_float32*(void)
{
	return &x;
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::operator st_float32*

ST_INLINE Vec4::operator const st_float32*(void) const
{
	return &x;
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::operator==

ST_INLINE bool Vec4::operator==(const Vec4& vIn) const
{
	return (x == vIn.x && y == vIn.y && z == vIn.z && w == vIn.w);	
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::operator!=

ST_INLINE bool Vec4::operator!=(const Vec4& vIn) const
{
	return (x != vIn.x || y != vIn.y || z != vIn.z || w != vIn.w);	
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::operator*

ST_INLINE Vec4 Vec4::operator*(const Vec4& vIn) const
{
	return Vec4(x * vIn.x, y * vIn.y, z * vIn.z, w * vIn.w);
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::operator*

ST_INLINE Vec4 Vec4::operator*(st_float32 fScalar) const
{
	return Vec4(x * fScalar, y * fScalar, z * fScalar, w * fScalar);
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::operator*=

ST_INLINE Vec4 Vec4::operator*=(st_float32 fScalar)
{
	x *= fScalar;
	y *= fScalar;
	z *= fScalar;
	w *= fScalar;

	return *this;
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::operator/

ST_INLINE Vec4 Vec4::operator/(st_float32 fDivisor) const
{
	return Vec4(x / fDivisor, y / fDivisor, z / fDivisor, w / fDivisor);
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::operator/=

ST_INLINE Vec4 Vec4::operator/=(st_float32 fDivisor)
{
	x /= fDivisor;
	y /= fDivisor;
	z /= fDivisor;
	w /= fDivisor;

	return *this;
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::Set

ST_INLINE Vec4 Vec4::operator+(const Vec4& vIn) const
{
	return Vec4(x + vIn.x, y + vIn.y, z + vIn.z, w + vIn.w);
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::Set

ST_INLINE void Vec4::Set(st_float32 _x, st_float32 _y, st_float32 _z, st_float32 _w)
{
	x = _x;
	y = _y;
	z = _z;
	w = _w;
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::Set

ST_INLINE void Vec4::Set(st_float32 _x, st_float32 _y, st_float32 _z)
{
	x = _x;
	y = _y;
	z = _z;
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::Set

ST_INLINE void Vec4::Set(st_float32 _x, st_float32 _y)
{
	x = _x;
	y = _y;
}


///////////////////////////////////////////////////////////////////////  
//  Vec4::Set

ST_INLINE void Vec4::Set(const st_float32 afPos[4])
{
	x = afPos[0];
	y = afPos[1];
	z = afPos[2];
	w = afPos[3];
}



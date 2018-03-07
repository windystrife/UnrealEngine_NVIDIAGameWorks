///////////////////////////////////////////////////////////////////////  
//  Fixed.inl
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
//  CFixedNumber::CFixedNumber

inline CFixedNumber::CFixedNumber( ) : 
	m_iValue(0) 
{ 
}


///////////////////////////////////////////////////////////////////////  
//  CFixedNumber::CFixedNumber

inline CFixedNumber::CFixedNumber(const CFixedNumber& cOther) : 
	m_iValue(cOther.m_iValue) 
{ 
}


///////////////////////////////////////////////////////////////////////  
//  CFixedNumber::CFixedNumber

inline CFixedNumber::CFixedNumber(st_float32 fValue) : 
	m_iValue(st_int32(fValue * m_fOneOverStep)) 
{ 
}


///////////////////////////////////////////////////////////////////////  
//  CFixedNumber::operator=

inline CFixedNumber& CFixedNumber::operator=(const CFixedNumber& cOther) 
{ 
	m_iValue = cOther.m_iValue; 
	return *this; 
}


///////////////////////////////////////////////////////////////////////  
//  CFixedNumber::operator=

inline CFixedNumber& CFixedNumber::operator=(float fValue)
{ 
	m_iValue = st_int32(fValue * m_fOneOverStep); 
	return *this; 
}


///////////////////////////////////////////////////////////////////////  
//  CFixedNumber::operator<

inline bool CFixedNumber::operator<(const CFixedNumber& cOther) const 
{ 
	return (m_iValue < cOther.m_iValue);
}


///////////////////////////////////////////////////////////////////////  
//  CFixedNumber::operator==

inline bool CFixedNumber::operator==(const CFixedNumber& cOther) const 
{ 
	return (m_iValue == cOther.m_iValue);
}


///////////////////////////////////////////////////////////////////////  
//  CFixedNumber::operator!=

inline bool CFixedNumber::operator!=(const CFixedNumber& cOther) const 
{ 
	return (m_iValue != cOther.m_iValue);
}


///////////////////////////////////////////////////////////////////////  
//  CFixedNumber::operator+

inline CFixedNumber CFixedNumber::operator+(const CFixedNumber& cOther) const 
{ 
	CFixedNumber cReturn; 
	cReturn.m_iValue = m_iValue + cOther.m_iValue; 
	return cReturn; 
}


///////////////////////////////////////////////////////////////////////  
//  CFixedNumber::operator-

inline CFixedNumber CFixedNumber::operator-(const CFixedNumber& cOther) const 
{ 
	CFixedNumber cReturn; 
	cReturn.m_iValue = m_iValue - cOther.m_iValue; 
	return cReturn; 
}


///////////////////////////////////////////////////////////////////////  
//  CFixedNumber::operator-

inline CFixedNumber CFixedNumber::operator-(void) const 
{ 
	CFixedNumber cReturn; 
	cReturn.m_iValue = -m_iValue; 
	return cReturn; 
}


///////////////////////////////////////////////////////////////////////  
//  CFixedNumber::ToFloat

inline st_float32 CFixedNumber::ToFloat(void) 
{ 
	return (m_iValue * m_fStep); 
}


///////////////////////////////////////////////////////////////////////  
//  CFixedNumber::SetBitsUsedForFraction

inline void CFixedNumber::SetBitsUsedForFraction(st_uint32 uiDigits) 
{ 
	m_uiBitsUsedForFraction = uiDigits;
	m_fOneOverStep = st_float32(1 << m_uiBitsUsedForFraction);
	m_fStep = (1.0f / m_fOneOverStep);
}


///////////////////////////////////////////////////////////////////////  
//  CFixedVec3::CFixedVec3

inline CFixedVec3::CFixedVec3( ) :
	x(0.0f),
	y(0.0f),
	z(0.0f)
{
}


///////////////////////////////////////////////////////////////////////  
//  CFixedVec3::CFixedVec3

inline CFixedVec3::CFixedVec3(st_float32 _x, st_float32 _y, st_float32 _z) :
	x(_x),
	y(_y),
	z(_z)
{
}


///////////////////////////////////////////////////////////////////////  
//  CFixedVec3::CFixedVec3

inline CFixedVec3::CFixedVec3(st_float32 _x, st_float32 _y) :
	x(_x),
	y(_y),
	z(0.0f)
{
}


///////////////////////////////////////////////////////////////////////  
//  CFixedVec3::CFixedVec3

inline CFixedVec3::CFixedVec3(const st_float32 afPos[3]) :
	x(afPos[0]),
	y(afPos[1]),
	z(afPos[2])
{
}


///////////////////////////////////////////////////////////////////////  
//  CFixedVec3::CFixedVec3

inline CFixedVec3::CFixedVec3(CFixedNumber _x, CFixedNumber _y, CFixedNumber _z) :
	x(_x),
	y(_y),
	z(_z)
{
}


///////////////////////////////////////////////////////////////////////  
//  CFixedVec3::CFixedVec3

inline CFixedVec3::CFixedVec3(CFixedNumber _x, CFixedNumber _y) :
	x(_x),
	y(_y),
	z(0.0f)
{
}


///////////////////////////////////////////////////////////////////////  
//  CFixedVec3::operator[]

inline CFixedNumber& CFixedVec3::operator[](int nIndex)
{
	return *(&x + nIndex);
}


///////////////////////////////////////////////////////////////////////  
//  CFixedVec3::operator<

inline bool CFixedVec3::operator<(const CFixedVec3& vIn) const
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
//  CFixedVec3::operator==

inline bool CFixedVec3::operator==(const CFixedVec3& vIn) const
{
	return (x == vIn.x && y == vIn.y && z == vIn.z);
}


///////////////////////////////////////////////////////////////////////  
//  CFixedVec3::operator!=

inline bool CFixedVec3::operator!=(const CFixedVec3& vIn) const
{
	return (x != vIn.x || y != vIn.y || z != vIn.z);	
}


///////////////////////////////////////////////////////////////////////  
//  CFixedVec3::operator-

inline CFixedVec3 CFixedVec3::operator-(const CFixedVec3& vIn) const
{
	return CFixedVec3(x - vIn.x, y - vIn.y, z - vIn.z);
}


///////////////////////////////////////////////////////////////////////  
//  CFixedVec3::operator+

inline CFixedVec3 CFixedVec3::operator+(const CFixedVec3& vIn) const
{
	return CFixedVec3(x + vIn.x, y + vIn.y, z + vIn.z);
}


///////////////////////////////////////////////////////////////////////  
//  CFixedVec3::operator-

inline CFixedVec3 CFixedVec3::operator-(void) const
{
	return CFixedVec3(-x, -y, -z);
}


///////////////////////////////////////////////////////////////////////  
//  CFixedVec3::Set

inline void CFixedVec3::Set(st_float32 _x, st_float32 _y, st_float32 _z)
{
	x = _x;
	y = _y;
	z = _z;
}


///////////////////////////////////////////////////////////////////////  
//  CFixedVec3::Set

inline void CFixedVec3::Set(st_float32 _x, st_float32 _y)
{
	x = _x;
	y = _y;
	z = 0.0f;
}


///////////////////////////////////////////////////////////////////////  
//  CFixedVec3::Set

inline void CFixedVec3::Set(const st_float32 afPos[3])
{
	x = afPos[0];
	y = afPos[1];
	z = afPos[2];
}


///////////////////////////////////////////////////////////////////////  
//  CFixedVec3::Set

inline void CFixedVec3::Set(CFixedNumber _x, CFixedNumber _y, CFixedNumber _z)
{
	x = _x;
	y = _y;
	z = _z;
}


///////////////////////////////////////////////////////////////////////  
//  CFixedVec3::Set

inline void CFixedVec3::Set(CFixedNumber _x, CFixedNumber _y)
{
	x = _x;
	y = _y;
	z = 0.0f;
}


///////////////////////////////////////////////////////////////////////  
//  CFixedVec3::Set

inline Vec3 CFixedVec3::ToVec3(void)
{
	return Vec3(x.ToFloat( ), y.ToFloat( ), z.ToFloat( ));
}



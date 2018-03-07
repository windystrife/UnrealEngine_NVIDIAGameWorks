///////////////////////////////////////////////////////////////////////  
//  Fixed.h
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
#include "Core/Types.h"
#include "Core/Vector.h"


///////////////////////////////////////////////////////////////////////  
//  All SpeedTree SDK classes and variables are under the namespace "SpeedTree"

namespace SpeedTree
{

	///////////////////////////////////////////////////////////////////////  
	//  Class CFixedNumber

	class ST_DLL_LINK CFixedNumber
	{
	public:
							CFixedNumber( );
							CFixedNumber(const CFixedNumber& cOther);
							CFixedNumber(st_float32 fValue);

			CFixedNumber&	operator=(const CFixedNumber& cOther);
			CFixedNumber&	operator=(float fValue);

			bool			operator<(const CFixedNumber& cOther) const;
			bool			operator==(const CFixedNumber& cOther) const;
			bool			operator!=(const CFixedNumber& cOther) const;
			CFixedNumber	operator+(const CFixedNumber& cOther) const;
			CFixedNumber	operator-(const CFixedNumber& cOther) const;
			CFixedNumber	operator-(void) const;

			st_float32		ToFloat(void);

			// call this function once before using CFixedNumber and never change it afterwards (default = 8)
	static	void			SetBitsUsedForFraction(st_uint32 uiDigits);

	private:
			st_int32		m_iValue;

	static	st_uint32		m_uiBitsUsedForFraction;
	static	st_float32		m_fOneOverStep;
	static	st_float32		m_fStep;
	};


	///////////////////////////////////////////////////////////////////////  
	//  Class CFixedVec3

	class ST_DLL_LINK CFixedVec3
	{
	public:
							CFixedVec3( );									// defaults to (0, 0, 0)
							CFixedVec3(st_float32 _x, st_float32 _y, st_float32 _z);
							CFixedVec3(st_float32 _x, st_float32 _y);		// z defaults to 0
							CFixedVec3(const st_float32 afPos[3]);
							CFixedVec3(CFixedNumber _x, CFixedNumber _y, CFixedNumber _z);
							CFixedVec3(CFixedNumber _x, CFixedNumber _y);	// z defaults to 0

			// operators
			CFixedNumber&	operator[](int nIndex);
			bool			operator<(const CFixedVec3& vIn) const;
			bool			operator==(const CFixedVec3& vIn) const;
			bool			operator!=(const CFixedVec3& vIn) const;
			CFixedVec3		operator-(const CFixedVec3& vIn) const;
			CFixedVec3		operator+(const CFixedVec3& vIn) const;
			CFixedVec3		operator-(void) const;

			// utility
			void			Set(st_float32 _x, st_float32 _y, st_float32 _z);
			void			Set(st_float32 _x, st_float32 _y);
			void			Set(const st_float32 afPos[3]);
			void			Set(CFixedNumber _x, CFixedNumber _y, CFixedNumber _z);
			void			Set(CFixedNumber _x, CFixedNumber _y);

			// conversion
			Vec3			ToVec3(void);

			CFixedNumber	x, y, z;
	};

	// include inline functions
	#include "Core/Fixed_inl.h"

} // end namespace SpeedTree

#include "Core/ExportEnd.h"


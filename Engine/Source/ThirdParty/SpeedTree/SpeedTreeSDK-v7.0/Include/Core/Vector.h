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
//  Preprocessor

#pragma once
#include "Core/ExportBegin.h"
#include "Core/Types.h"
#include <limits>

#if defined(__GNUC__) || defined(__psp2__)
	#include <math.h>
	#include <cfloat>
#else
	#include <cmath>
	#include <cfloat>
#endif


///////////////////////////////////////////////////////////////////////  
//  Packing

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(push, 4)
#endif


///////////////////////////////////////////////////////////////////////  
//  All SpeedTree SDK classes and variables are under the namespace "SpeedTree"

namespace SpeedTree
{

	///////////////////////////////////////////////////////////////////////  
	//  Commonly used mathematical constants

	static const st_float32 c_fPi = 3.14159265358979323846f;
	static const st_float32 c_fTwoPi = 6.28318530717958647692528f;
	static const st_float32 c_fHalfPi = 1.57079632679489661923f;
	static const st_float32 c_fQuarterPi = 0.785398163397448309615f;

	#ifndef INT_MAX
		#define INT_MAX __INT_MAX__
	#endif
	#ifndef FLT_MAX
		#define FLT_MAX __FLT_MAX__
	#endif

	///////////////////////////////////////////////////////////////////////  
	//  RadToDeg

	ST_INLINE st_float32 RadToDeg(st_float32 fRadians) { return fRadians * 57.2957795f; }
	ST_INLINE st_float32 DegToRad(st_float32 fDegrees) { return fDegrees * 0.01745329252f; }


	///////////////////////////////////////////////////////////////////////  
	//  Forward references

	class Mat3x3;
	class Mat4x4;


	///////////////////////////////////////////////////////////////////////  
	//  Class Vec2

	class ST_DLL_LINK Vec2
	{
	public:
							Vec2( );								// defaults to (0, 0, 0)
                            Vec2(st_float32 _x, st_float32 _y);
							Vec2(const st_float32 afPos[2]);
                            
            Vec2			operator+(const Vec2& vIn) const;
            Vec2			operator+=(const Vec2& vIn);
            st_bool			operator==(const Vec2& vIn) const;

            void            Set(st_float32 _x, st_float32 _y);
            st_float32      Distance(const Vec2& vIn) const;
            
            st_float32      x, y;
    };
    

	///////////////////////////////////////////////////////////////////////  
	//  Class Vec3

	class ST_DLL_LINK Vec3
	{
	public:
							Vec3( );								// defaults to (0, 0, 0)
							Vec3(st_float32 _x, st_float32 _y, st_float32 _z);
							Vec3(st_float32 _x, st_float32 _y);		// z defaults to 0
							Vec3(const st_float32 afPos[3]);

			// operators
			st_float32&		operator[](int nIndex);
							operator st_float32*(void);
							operator const st_float32*(void) const;
			bool			operator<(const Vec3& vIn) const;
			bool			operator==(const Vec3& vIn) const;
			bool			operator!=(const Vec3& vIn) const;
			Vec3			operator-(const Vec3& vIn) const;
			Vec3			operator+(const Vec3& vIn) const;
			Vec3			operator+=(const Vec3& vIn);
			Vec3			operator*(const Vec3& vIn) const;
			Vec3			operator*=(const Vec3& vIn);
			Vec3			operator/(const Vec3& vIn) const;
			Vec3			operator/=(const Vec3& vIn);
			Vec3			operator*(st_float32 fScalar) const;
			Vec3			operator*=(st_float32 fScalar);
			Vec3			operator/(st_float32 fDivisor) const;
			Vec3			operator/=(st_float32 fDivisor);
			Vec3			operator-(void) const;

			// utility
			void			Set(st_float32 _x, st_float32 _y, st_float32 _z);
			void			Set(st_float32 _x, st_float32 _y);
			void			Set(const st_float32 afPos[3]);

			// mathematical operators
			Vec3			Cross(const Vec3& vIn) const;
			st_float32		Distance(const Vec3& vIn) const;
			st_float32		DistanceSquared(const Vec3& vIn) const;
			st_float32		Dot(const Vec3& vIn) const;
			st_float32		Magnitude(void) const;
			st_float32		MagnitudeSquared(void) const;
			Vec3			Normalize(void);
			void			Scale(st_float32 fScalar);

			st_float32		x, y, z;
	};


	///////////////////////////////////////////////////////////////////////  
	//  Class Vec4

	class ST_DLL_LINK Vec4
	{
	public:
							Vec4( );												// defaults to (0.0, 0.0, 0.0, 1.0)
							Vec4(st_float32 _x, st_float32 _y, st_float32 _z, st_float32 _w);
							Vec4(st_float32 _x, st_float32 _y, st_float32 _z);		// w defaults to 1.0
							Vec4(st_float32 _x, st_float32 _y);						// z defaults to 0.0, w to 1.0
							Vec4(const Vec3& vVec, st_float32 _w);
							Vec4(const st_float32 afPos[4]);

			// operators
			st_float32&		operator[](int nIndex);
							operator st_float32*(void);
							operator const st_float32*(void) const;
			bool			operator==(const Vec4& vIn) const;
			bool			operator!=(const Vec4& vIn) const;
			Vec4			operator*(const Vec4& vIn) const;
			Vec4			operator*(st_float32 fScalar) const;
			Vec4			operator*=(st_float32 fScalar);
			Vec4			operator/(st_float32 fScalar) const;
			Vec4			operator/=(st_float32 fScalar);
			Vec4			operator+(const Vec4& vIn) const;

			// utility
			void			Set(st_float32 _x, st_float32 _y, st_float32 _z, st_float32 _w);
			void			Set(st_float32 _x, st_float32 _y, st_float32 _z);
			void			Set(st_float32 _x, st_float32 _y);
			void			Set(const st_float32 afPos[4]);

			st_float32		x, y, z, w;
	};

	// include inline functions
	#include "Core/Vector_inl.h"

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif

#include "Core/ExportEnd.h"


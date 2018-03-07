///////////////////////////////////////////////////////////////////////  
//  Matrix.h
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
#include "Core/Vector.h"
#if defined(__GNUC__) || defined(__psp2__)
	#include <string.h>
#else
	#include <cstring>
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
	//  Class Mat3x3

	class ST_DLL_LINK Mat3x3
	{
	public:
							Mat3x3(bool bSetToIdentity = true);
							Mat3x3(const st_float32 afInit[9]);
							Mat3x3(const Vec3& vRight, const Vec3& vOut, const Vec3& vUp);

			// operators
							operator st_float32*(void);
							operator const st_float32*(void) const;
			Mat3x3			operator*(const Mat3x3& vIn) const;
			Vec3			operator*(const Vec3& vIn) const;
			bool			operator==(const Mat3x3& vIn) const;
			bool			operator!=(const Mat3x3& vIn) const;

			// utility
			void			SetIdentity(void);
			void			Set(const st_float32 afValue[9]);

			// mathematical operations
			void			RotateX(st_float32 fRadians);
			void			RotateY(st_float32 fRadians);
			void			RotateZ(st_float32 fRadians);
			void			RotateArbitrary(const Vec3& vAxis, st_float32 fRadius);
			void			Scale(st_float32 x, st_float32 y, st_float32 z);
			void			Scale(const Vec3& vScalar);

			union
			{
				st_float32	m_afSingle[9];
				st_float32	m_afRowCol[3][3];

				// m_afSingle[0] = m_afRowCol[0][0]
				// m_afSingle[1] = m_afRowCol[0][1]
				// m_afSingle[2] = m_afRowCol[0][2]
				// m_afSingle[3] = m_afRowCol[1][0]
				// m_afSingle[4] = m_afRowCol[1][1]
				// m_afSingle[5] = m_afRowCol[1][2]
				// m_afSingle[6] = m_afRowCol[2][0]
				// m_afSingle[7] = m_afRowCol[2][1]
				// m_afSingle[8] = m_afRowCol[2][2]
			};
	};


	///////////////////////////////////////////////////////////////////////  
	//  Class Mat4x4

	class ST_DLL_LINK Mat4x4
	{
	public:
							Mat4x4(bool bSetToIdentity = true);
							Mat4x4(const st_float32 afInit[16]);
							Mat4x4(st_float32 m00, st_float32 m01, st_float32 m02, st_float32 m03,
								   st_float32 m10, st_float32 m11, st_float32 m12, st_float32 m13,
								   st_float32 m20, st_float32 m21, st_float32 m22, st_float32 m23,
								   st_float32 m30, st_float32 m31, st_float32 m32, st_float32 m33);

			// operators
							operator st_float32*(void);
							operator const st_float32*(void) const;
			Mat4x4			operator*(const Mat4x4& mIn) const;
			Mat4x4			operator*=(const Mat4x4& mIn);
			Vec3			operator*(const Vec3& vIn) const;
			Vec4			operator*(const Vec4& vIn) const;
			bool			operator==(const Mat4x4& mIn) const;
			bool			operator!=(const Mat4x4& mIn) const;
			Mat4x4&			operator=(const Mat4x4& mIn);

			// utility
			void			SetIdentity(void);
			void			Set(const st_float32 afValue[16]);
			void			Set(st_float32 m00, st_float32 m01, st_float32 m02, st_float32 m03,
								st_float32 m10, st_float32 m11, st_float32 m12, st_float32 m13,
								st_float32 m20, st_float32 m21, st_float32 m22, st_float32 m23,
								st_float32 m30, st_float32 m31, st_float32 m32, st_float32 m33);
			void			GetVectorComponents(Vec3& vUp, Vec3& vOut, Vec3& vRight);

			// mathematical operations
			bool			Invert(Mat4x4& mResult) const;
			void			Multiply4f(const st_float32 afIn[4], st_float32 afResult[4]) const;
			void			RotateX(st_float32 fRadians);
			void			RotateY(st_float32 fRadians);
			void			RotateZ(st_float32 fRadians);
			void			RotateArbitrary(const Vec3& vAxis, st_float32 fRadius);
			void			Scale(st_float32 x, st_float32 y, st_float32 z);
			void			Scale(const Vec3& vScalar);
			void			Translate(st_float32 x, st_float32 y, st_float32 z);
			void			Translate(const Vec3& vTranslate);
			Mat4x4			Transpose(void) const;

			// view matrix setups
			void			LookAt(const Vec3& vEye, const Vec3& vCenter, const Vec3& vUp);
			void			Ortho(st_float32 fLeft, st_float32 fRight, st_float32 fBottom, st_float32 fTop, st_float32 fNear, st_float32 fFar, bool bOpenGL = false);
			void			Frustum(st_float32 fLeft, st_float32 fRight, st_float32 fBottom, st_float32 fTop, st_float32 fNear, st_float32 fFar);
			void			Perspective(st_float32 fFieldOfView, st_float32 fAspectRatio, st_float32 fNear, st_float32 fFar);
			void			AdjustPerspectiveNearAndFar(st_float32 fNear, st_float32 fFar);

			union
			{
				st_float32	m_afSingle[16];
				st_float32	m_afRowCol[4][4];

				// m_afSingle[0]  = m_afRowCol[0][0]
				// m_afSingle[1]  = m_afRowCol[0][1]
				// m_afSingle[2]  = m_afRowCol[0][2]
				// m_afSingle[3]  = m_afRowCol[0][3]
				// m_afSingle[4]  = m_afRowCol[1][0]
				// m_afSingle[5]  = m_afRowCol[1][1]
				// m_afSingle[6]  = m_afRowCol[1][2]
				// m_afSingle[7]  = m_afRowCol[1][3]
				// m_afSingle[8]  = m_afRowCol[2][0]
				// m_afSingle[9]  = m_afRowCol[2][1]
				// m_afSingle[10] = m_afRowCol[2][2]
				// m_afSingle[11] = m_afRowCol[2][3]
				// m_afSingle[12] = m_afRowCol[3][0]
				// m_afSingle[13] = m_afRowCol[3][1]
				// m_afSingle[14] = m_afRowCol[3][2]
				// m_afSingle[15] = m_afRowCol[3][3]
			};
	};

	// include inline functions
	#include "Core/Matrix_inl.h"

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif

#include "Core/ExportEnd.h"

///////////////////////////////////////////////////////////////////////  
//	Extents.h
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
// Preprocessor

#pragma once
#include "Core/ExportBegin.h"
#include "Core/CoordSys.h"
#include "Core/Vector.h"
#include <cfloat>
#include <cassert>


///////////////////////////////////////////////////////////////////////  
//  Packing

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(push, 4)
#endif


///////////////////////////////////////////////////////////////////////  
//  All SpeedTree SDK classes and variables are under the namespace "SpeedTree"

namespace SpeedTree
{

	/////////////////////////////////////////////////////////////////////
	// class CExtents
	//
	// Represents an axis-aligned bounding box

	class ST_DLL_LINK CExtents
	{
	public:
							CExtents( );
							CExtents(const st_float32 afExtents[6]); // [0-2] = min(x,y,z), [3-5] = max(x,y,z)
							CExtents(const Vec3& cMin, const Vec3& cMax);
							~CExtents( );											

			void			Reset(void);
			void			SetToZeros(void);
			void			Order(void);
			bool			Valid(void) const;

			void			ExpandAround(const st_float32 afPoint[3]);
			void			ExpandAround(const Vec3& vPoint);
			void			ExpandAround(const Vec3& vPoint, st_float32 fRadius);
			void			ExpandAround(const CExtents& cOther);
			void			Scale(st_float32 fScalar);
			void			Translate(const Vec3& vTranslation);
			void			Orient(const Vec3& vUp, const Vec3& vRight);
			void			Rotate(st_float32 fRadians);	// around 'up' axis

			st_float32		ComputeRadiusFromCenter3D(void) const;
			st_float32		ComputeRadiusSquaredFromCenter3D(void) const;
			st_float32		ComputeRadiusFromCenter2D(void) const;

			const Vec3&		Min(void) const;
			const Vec3&		Max(void) const;
			st_float32		Midpoint(st_uint32 uiAxis) const;
			Vec3			GetCenter(void) const;
			Vec3			GetDiagonal(void) const;

			st_float32		GetHeight(void) const; // will return the up component of m_vMax, using CCoordSys

							operator st_float32*(void);
							operator const st_float32*(void) const;

	private:
			Vec3			m_vMin;
			Vec3			m_vMax;
	};

	// include inline functions
	#include "Core/Extents_inl.h"

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif

#include "Core/ExportEnd.h"




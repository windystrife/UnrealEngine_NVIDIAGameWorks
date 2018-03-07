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

#include "Core/CoordSys.h"
#include <cassert>
using namespace SpeedTree;


///////////////////////////////////////////////////////////////////////
//  Static variables

static CDefaultCoordinateSystem g_cDefaultCoordSys;

const CCoordSysBase* CCoordSys::m_pCoordSys = &g_cDefaultCoordSys;
CCoordSys::ECoordSysType CCoordSys::m_eCoordSysType = COORD_SYS_RIGHT_HANDED_Z_UP;

const Vec3 CDefaultCoordinateSystem::m_vOut(0.0f, 1.0f, 0.0f);
const Vec3 CDefaultCoordinateSystem::m_vRight(1.0f, 0.0f, 0.0f);
const Vec3 CDefaultCoordinateSystem::m_vUp(0.0f, 0.0f, 1.0f);

const CRHCS_Yup g_cRHCS_Yup;
const CLHCS_Yup g_cLHCS_Yup;
const CLHCS_Zup g_cLHCS_Zup;


///////////////////////////////////////////////////////////////////////
//  CCoordSys::SetCoordSys

void CCoordSys::SetCoordSys(ECoordSysType eType, const CCoordSysBase* pCustomConverter)
{
	m_eCoordSysType = eType;

	if (eType == COORD_SYS_CUSTOM)
	{
		// custom
		assert(pCustomConverter);
		m_pCoordSys = pCustomConverter;
	}
	else
		m_pCoordSys = GetBuiltInConverter(eType);

	assert(m_pCoordSys);
}


///////////////////////////////////////////////////////////////////////
//  CCoordSys::CoordSysName

const char* CCoordSys::CoordSysName(ECoordSysType eType)
{
	const char* c_apNames[COORD_SYS_CUSTOM + 1] =
	{
		"right-handed, Z up",
		"right-handed, Y up",
		"left-handed, Z up",
		"left-handed, Y up",
		"custom"
	};

	return c_apNames[eType];
}


///////////////////////////////////////////////////////////////////////
//  CCoordSys::IsDefaultCoordSys

st_bool CCoordSys::IsDefaultCoordSys(void)
{
	return (m_pCoordSys == &g_cDefaultCoordSys);
}


///////////////////////////////////////////////////////////////////////
//  CCoordSys::GetBuiltInConverter

const CCoordSysBase* CCoordSys::GetBuiltInConverter(ECoordSysType eType)
{
	switch (eType)
	{
	case COORD_SYS_RIGHT_HANDED_Z_UP:
		return &g_cDefaultCoordSys;
	case COORD_SYS_RIGHT_HANDED_Y_UP:
		return &g_cRHCS_Yup;
	case COORD_SYS_LEFT_HANDED_Z_UP:
		return &g_cLHCS_Zup;
	case COORD_SYS_LEFT_HANDED_Y_UP:
		return &g_cLHCS_Yup;
	default:
		return NULL;
	}
}


///////////////////////////////////////////////////////////////////////
//  CCoordSys::GetCoordSysType

CCoordSys::ECoordSysType CCoordSys::GetCoordSysType(void)
{
	return m_eCoordSysType;
}


///////////////////////////////////////////////////////////////////////
//  CCoordSys::IsLeftHanded

st_bool CCoordSys::IsLeftHanded(void)
{
	assert(m_pCoordSys);
	return m_pCoordSys->IsLeftHanded( );
}


///////////////////////////////////////////////////////////////////////
//  CCoordSys::IsYAxisUp

st_bool CCoordSys::IsYAxisUp(void)
{
	assert(m_pCoordSys);
	return m_pCoordSys->IsYAxisUp( );
}


///////////////////////////////////////////////////////////////////////
//  CCoordSys::ConvertToStd

Vec3 CCoordSys::ConvertToStd(const st_float32 afCoord[3])
{
	assert(m_pCoordSys);
	return m_pCoordSys->ConvertToStd(afCoord[0], afCoord[1], afCoord[2]);
}


///////////////////////////////////////////////////////////////////////
//  CCoordSys::ConvertToStd

Vec3 CCoordSys::ConvertToStd(st_float32 x, st_float32 y, st_float32 z)
{
	assert(m_pCoordSys);
	return m_pCoordSys->ConvertToStd(x, y, z);
}


///////////////////////////////////////////////////////////////////////
//  CCoordSys::ConvertFromStd

Vec3 CCoordSys::ConvertFromStd(const st_float32 afCoord[3])
{
	assert(m_pCoordSys);
	return m_pCoordSys->ConvertFromStd(afCoord[0], afCoord[1], afCoord[2]);
}


///////////////////////////////////////////////////////////////////////
//  CCoordSys::ConvertFromStd

Vec3 CCoordSys::ConvertFromStd(st_float32 x, st_float32 y, st_float32 z)
{
	assert(m_pCoordSys);
	return m_pCoordSys->ConvertFromStd(x, y, z);
}


///////////////////////////////////////////////////////////////////////
//  CCoordSys::OutAxis

const Vec3& CCoordSys::OutAxis(void)
{
	assert(m_pCoordSys);
	return m_pCoordSys->OutAxis( );
}


///////////////////////////////////////////////////////////////////////
//  CCoordSys::RightAxis

const Vec3& CCoordSys::RightAxis(void)
{
	assert(m_pCoordSys);
	return m_pCoordSys->RightAxis( );
}


///////////////////////////////////////////////////////////////////////
//  CCoordSys::UpAxis

const Vec3& CCoordSys::UpAxis(void)
{
	assert(m_pCoordSys);
	return m_pCoordSys->UpAxis( );
}


///////////////////////////////////////////////////////////////////////
//  CCoordSys::OutComponent

st_float32 CCoordSys::OutComponent(const st_float32 afCoord[3])
{
	assert(m_pCoordSys);
	return m_pCoordSys->OutComponent(afCoord[0], afCoord[1], afCoord[2]);
}


///////////////////////////////////////////////////////////////////////
//  CCoordSys::RightComponent

st_float32 CCoordSys::RightComponent(const st_float32 afCoord[3])
{
	assert(m_pCoordSys);
	return m_pCoordSys->RightComponent(afCoord[0], afCoord[1], afCoord[2]);
}


///////////////////////////////////////////////////////////////////////
//  CCoordSys::UpComponent

st_float32 CCoordSys::UpComponent(const st_float32 afCoord[3])
{
	assert(m_pCoordSys);
	return m_pCoordSys->UpComponent(afCoord[0], afCoord[1], afCoord[2]);
}


///////////////////////////////////////////////////////////////////////
//  CCoordSys::RotateUpAxis

void CCoordSys::RotateUpAxis(Mat3x3& mMatrix, st_float32 fRadians)
{
	assert(m_pCoordSys);
	m_pCoordSys->RotateUpAxis(mMatrix, fRadians);
}


///////////////////////////////////////////////////////////////////////
//  CCoordSys::RotateUpAxis

void CCoordSys::RotateUpAxis(Mat4x4& mMatrix, st_float32 fRadians)
{
	assert(m_pCoordSys);
	m_pCoordSys->RotateUpAxis(mMatrix, fRadians);
}


///////////////////////////////////////////////////////////////////////
//  CCoordSys::RotateOutAxis

void CCoordSys::RotateOutAxis(Mat3x3& mMatrix, st_float32 fRadians)
{
	assert(m_pCoordSys);
	m_pCoordSys->RotateOutAxis(mMatrix, fRadians);
}


///////////////////////////////////////////////////////////////////////
//  CCoordSys::RotateUpAxis

void CCoordSys::RotateOutAxis(Mat4x4& mMatrix, st_float32 fRadians)
{
	assert(m_pCoordSys);
	m_pCoordSys->RotateOutAxis(mMatrix, fRadians);
}


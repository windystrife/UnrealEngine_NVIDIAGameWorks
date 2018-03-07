// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11BaseRHIPrivate.h: Private D3D RHI definitions for Windows.
=============================================================================*/

#pragma once

#include "WindowsHWrapper.h"

// Assume D3DX is available
#ifndef WITH_D3DX_LIBS
#define WITH_D3DX_LIBS	1
#endif

// Disable macro redefinition warning for compatibility with Windows SDK 8+
#pragma warning(push)
#pragma warning(disable : 4005)	// macro redefinition

// D3D headers.
#if PLATFORM_64BITS
#pragma pack(push,16)
#else
#pragma pack(push,8)
#endif
#define D3D_OVERLOADS 1
#include "AllowWindowsPlatformTypes.h"
#include <D3D11.h>
#include "HideWindowsPlatformTypes.h"

#undef DrawText

#pragma pack(pop)
#pragma warning(pop)

typedef ID3D11DeviceContext FD3D11DeviceContext;
typedef ID3D11Device FD3D11Device;

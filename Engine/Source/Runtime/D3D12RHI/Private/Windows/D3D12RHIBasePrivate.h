// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12BaseRHIPrivate.h: Private D3D RHI definitions for Windows.
=============================================================================*/

#pragma once

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
#include <d3d12.h>
#include "d3dx12.h"
#include <d3d12sdklayers.h>
#include "HideWindowsPlatformTypes.h"

#undef DrawText

#pragma pack(pop)
#pragma warning(pop)

#define D3D12RHI_RESOURCE_FLAG_ALLOW_INDIRECT_BUFFER D3D12_RESOURCE_FLAG_NONE
#define D3D12RHI_HEAP_FLAG_ALLOW_INDIRECT_BUFFERS		D3D12_HEAP_FLAG_NONE

#include "../Public/D3D12Util.h"
#include "WindowsD3D12DiskCache.h"
#include "WindowsD3D12PipelineState.h"

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EditorPhysXSupport.h: Editor version of the engine's PhysXSupport.h
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"

#if WITH_PHYSX

#pragma warning( push )
#pragma warning( disable : 4946 ) // reinterpret_cast used between related classes

PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS

#if USING_CODE_ANALYSIS
	#pragma warning( push )
	#pragma warning( disable : ALL_CODE_ANALYSIS_WARNINGS )
#endif	// USING_CODE_ANALYSIS

// different packing for Intel 32-bit on Linux
#if (PLATFORM_LINUX && PLATFORM_CPU_X86_FAMILY && !PLATFORM_64BITS)
	#pragma pack(push,16)
#else
	#pragma pack(push,8)
#endif
THIRD_PARTY_INCLUDES_START

#include "Px.h"
#include "PxPhysicsAPI.h"
#include "PxRenderBuffer.h"
#include "PxExtensionsAPI.h"
#include "PxPvd.h"
//#include "PxDefaultCpuDispatcher.h"

// vehicle related header files
//#include "PxVehicleSDK.h"
//#include "PxVehicleUtils.h"

// utils
#include "PxGeometryQuery.h"
#include "PxMeshQuery.h"
#include "PxTriangle.h"

// APEX
#if WITH_APEX
// Framework
#include "Apex.h"

// Modules
#include "ModuleClothing.h"

// Assets
#include "ClothingAsset.h"

// Actors
#include "ClothingActor.h"

// Utilities
#include "NvParamUtils.h"

#endif // #if WITH_APEX

THIRD_PARTY_INCLUDES_END
#pragma pack(pop)

#if USING_CODE_ANALYSIS
	#pragma warning( pop )
#endif	// USING_CODE_ANALYSIS

#pragma warning( pop )

PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS

using namespace physx;

#endif // WITH_PHYSX

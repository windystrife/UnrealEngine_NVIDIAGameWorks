// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"

#ifdef NV_CLOTH_API
#undef NV_CLOTH_API	//Our build system treats PhysX as a module and automatically defines this. PhysX has its own way of defining this.
#endif

#if WITH_NVCLOTH

PUSH_MACRO(check)
#undef check

#if PLATFORM_XBOXONE
	#pragma pack(push,16)
#elif PLATFORM_WINDOWS
	#if PLATFORM_64BITS
		#pragma pack(push,16)
	#elif PLATFORM_32BITS
		#pragma pack(push,8)
	#endif
#endif

#include "PxErrors.h"
#include "PxErrorCallback.h"

#include "NvCloth/PhaseConfig.h"
#include "NvCloth/Callbacks.h"
#include "NvCloth/Factory.h"
#include "NvCloth/Solver.h"
#include "NvCloth/Cloth.h"

#include "NvClothExt/ClothMeshDesc.h"
#include "NvClothExt/ClothFabricCooker.h"
#include "NvClothExt/ClothMeshQuadifier.h"

#if PLATFORM_XBOXONE
	#pragma pack(pop)
#elif PLATFORM_WINDOWS
	#if PLATFORM_64BITS
		#pragma pack(pop)
	#elif PLATFORM_32BITS
		#pragma pack(pop)
	#endif
#endif

POP_MACRO(check)

#endif

// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_NVCLOTH

#include "NvClothIncludes.h"

#include "LogVerbosity.h"

#include "UnrealString.h"
#include "Array.h"

namespace NvClothSupport
{
	ELogVerbosity::Type PxErrorToLogVerbosity(physx::PxErrorCode::Enum InEnum);
	FString PxErrorToString(physx::PxErrorCode::Enum InEnum);

	// Helper incase we ever want a new allocator for NvCloth
	class physx::PxAllocatorCallback* GetAllocator();

	void InitializeNvClothingSystem();
	void ShutdownNvClothingSystem();

	struct ClothTri
	{
		ClothTri()
		{}

		ClothTri(uint32 A, uint32 B, uint32 C)
		{
			T[0] = A;
			T[1] = B;
			T[2] = C;
		}

		uint32 T[3];
	};

	struct ClothQuad
	{
		ClothQuad()
		{}

		ClothQuad(uint32 A, uint32 B, uint32 C, uint32 D)
		{
			Q[0] = A;
			Q[1] = B;
			Q[2] = C;
			Q[3] = D;
		}

		uint32 Q[4];
	};

	template<typename ContainerType>
	nv::cloth::Range<ContainerType> CreateRange(TArray<ContainerType>& InArray, int32 InBeginOffset = 0)
	{
		return {InArray.GetData() + InBeginOffset, (InArray.GetData() + InBeginOffset) + InArray.Num()};
	}

	template<typename ContainerType>
	nv::cloth::Range<const ContainerType> CreateRange(const TArray<ContainerType>& InArray, int32 InBeginOffset = 0)
	{
		return{InArray.GetData() + InBeginOffset, (InArray.GetData() + InBeginOffset) + InArray.Num()};
	}

	struct ClothParticleScopeLock
	{
		ClothParticleScopeLock();
		ClothParticleScopeLock(nv::cloth::Cloth* InCloth);

		~ClothParticleScopeLock();

	private:
		nv::cloth::Cloth* LockedCloth;
	};
}

#endif

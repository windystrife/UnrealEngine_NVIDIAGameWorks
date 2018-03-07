// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "RenderResource.h"
#include "Components.generated.h"

/*=============================================================================
	Components.h: Forward declarations of object components of actors
=============================================================================*/

// Constants.
enum { MAX_TEXCOORDS = 4, MAX_STATIC_TEXCOORDS = 8 };

/** The information used to build a static-mesh vertex. */
struct FStaticMeshBuildVertex
{
	FVector Position;

	FVector TangentX;
	FVector TangentY;
	FVector TangentZ;

	FVector2D UVs[MAX_STATIC_TEXCOORDS];
	FColor Color;
};



/** The world size for each texcoord mapping. Used by the texture streaming. */
USTRUCT(BlueprintType)
struct FMeshUVChannelInfo
{
	GENERATED_USTRUCT_BODY()

	/** Default constructor (no initialization). */
	FORCEINLINE FMeshUVChannelInfo() { FMemory::Memzero(*this); }

	/** Constructor which initializes all components to zero. */
	FMeshUVChannelInfo(ENoInit) { }

	UPROPERTY()
	bool bInitialized;

	/** Whether this values was set manually or is auto generated. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Basic)
	bool bOverrideDensities;

	/**
	 * The UV density in the mesh, before any transform scaling, in world unit per UV.
	 * This value represents the length taken to cover a full UV unit.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Basic, meta = (EditCondition = "bOverrideDensities"))
	float LocalUVDensities[MAX_TEXCOORDS];

	friend FArchive& operator<<(FArchive& Ar, FMeshUVChannelInfo& Info);
};

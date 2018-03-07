// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "LandscapeGrassType.generated.h"

class UStaticMesh;
struct FPropertyChangedEvent;

UENUM()
enum class EGrassScaling : uint8
{
	/** Grass instances will have uniform X, Y and Z scales. */
	Uniform,
	/** Grass instances will have random X, Y and Z scales. */
	Free,
	/** X and Y will be the same random scale, Z will be another */
	LockXY,
};

USTRUCT()
struct FGrassVariety
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Grass)
	UStaticMesh* GrassMesh;

	/* Instances per 10 square meters. */
	UPROPERTY(EditAnywhere, Category=Grass)
	float GrassDensity;

	/* If true, use a jittered grid sequence for placement, otherwise use a halton sequence. */
	UPROPERTY(EditAnywhere, Category=Grass)
	bool bUseGrid;

	UPROPERTY(EditAnywhere, Category=Grass, meta = (EditCondition = "bUseGrid", UIMin = 0, ClampMin = 0, UIMax = 1, ClampMax = 1))
	float PlacementJitter;

	/* The distance where instances will begin to fade out if using a PerInstanceFadeAmount material node. 0 disables. */
	UPROPERTY(EditAnywhere, Category=Grass)
	int32 StartCullDistance;

	/**
	 * The distance where instances will have completely faded out when using a PerInstanceFadeAmount material node. 0 disables. 
	 * When the entire cluster is beyond this distance, the cluster is completely culled and not rendered at all.
	 */
	UPROPERTY(EditAnywhere, Category = Grass)
	int32 EndCullDistance;

	/** 
	 * Specifies the smallest LOD that will be used for this component.
	 * If -1 (default), the MinLOD of the static mesh asset will be used instead.
	 */
	UPROPERTY(EditAnywhere, Category = Grass)
	int32 MinLOD;

	/** Specifies grass instance scaling type */
	UPROPERTY(EditAnywhere, Category=Grass)
	EGrassScaling Scaling;

	/** Specifies the range of scale, from minimum to maximum, to apply to a grass instance's X Scale property */
	UPROPERTY(EditAnywhere, Category=Grass)
	FFloatInterval ScaleX;

	/** Specifies the range of scale, from minimum to maximum, to apply to a grass instance's Y Scale property */
	UPROPERTY(EditAnywhere, Category=Grass)
	FFloatInterval ScaleY;

	/** Specifies the range of scale, from minimum to maximum, to apply to a grass instance's Z Scale property */
	UPROPERTY(EditAnywhere, Category=Grass)
	FFloatInterval ScaleZ;

	/** Whether the grass instances should be placed at random rotation (true) or all at the same rotation (false) */
	UPROPERTY(EditAnywhere, Category = Grass)
	bool RandomRotation;

	/** Whether the grass instances should be tilted to the normal of the landscape (true), or always vertical (false) */
	UPROPERTY(EditAnywhere, Category = Grass)
	bool AlignToSurface;

	/* Whether to use the landscape's lightmap when rendering the grass. */
	UPROPERTY(EditAnywhere, Category = Grass)
	bool bUseLandscapeLightmap;

	/**
	 * Lighting channels that the grass will be assigned. Lights with matching channels will affect the grass.
	 * These channels only apply to opaque materials, direct lighting, and dynamic lighting and shadowing.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Grass)
	FLightingChannels LightingChannels;

	/** Whether the grass instances should receive decals. */
	UPROPERTY(EditAnywhere, Category = Grass)
	bool bReceivesDecals;

	FGrassVariety()
		: GrassMesh(nullptr)
		, GrassDensity(400)
		, bUseGrid(true)
		, PlacementJitter(1.0f)
		, StartCullDistance(10000.0f)
		, EndCullDistance(10000.0f)
		, MinLOD(-1)
		, Scaling(EGrassScaling::Uniform)
		, ScaleX(1.0f, 1.0f)
		, ScaleY(1.0f, 1.0f)
		, ScaleZ(1.0f, 1.0f)
		, RandomRotation(true)
		, AlignToSurface(true)
		, bUseLandscapeLightmap(false)
		, bReceivesDecals(true)
	{
	}
};

UCLASS(MinimalAPI)
class ULandscapeGrassType : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Grass)
	TArray<FGrassVariety> GrassVarieties;

	UPROPERTY()
	UStaticMesh* GrassMesh_DEPRECATED;
	UPROPERTY()
	float GrassDensity_DEPRECATED;
	UPROPERTY()
	float PlacementJitter_DEPRECATED;
	UPROPERTY()
	int32 StartCullDistance_DEPRECATED;
	UPROPERTY()
	int32 EndCullDistance_DEPRECATED;
	UPROPERTY()
	bool RandomRotation_DEPRECATED;
	UPROPERTY()
	bool AlignToSurface_DEPRECATED;

	//~ Begin UObject Interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface
};




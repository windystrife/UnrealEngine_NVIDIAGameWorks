// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.h"
#include "SpriteDrawCall.h"
#include "PaperTerrainComponent.generated.h"

class FPrimitiveSceneProxy;
class UBodySetup;
class UMaterialInterface;
struct FPaperTerrainMaterialRule;

struct FPaperTerrainSpriteGeometry
{
	TArray<FSpriteDrawCallRecord> Records;
	UMaterialInterface* Material;
	int32 DrawOrder;
};

struct FTerrainSpriteStamp
{
	const UPaperSprite* Sprite;
	float NominalWidth;
	float Time;
	float Scale;
	bool bCanStretch;

	FTerrainSpriteStamp(const UPaperSprite* InSprite, float InTime, bool bIsEndCap);
};

struct FTerrainSegment
{
	float StartTime;
	float EndTime;
	const FPaperTerrainMaterialRule* Rule;
	TArray<FTerrainSpriteStamp> Stamps;

	FTerrainSegment();
	void RepositionStampsToFillSpace();
};

/**
 * The terrain visualization component for an associated spline component.
 * This takes a 2D terrain material and instances sprite geometry along the spline path.
 */

UCLASS(BlueprintType, Experimental)
class PAPER2D_API UPaperTerrainComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

public:
	/** The terrain material to apply to this component (set of rules for which sprites are used on different surfaces or the interior) */
	UPROPERTY(Category=Sprite, EditAnywhere, BlueprintReadOnly)
	class UPaperTerrainMaterial* TerrainMaterial;

	UPROPERTY(Category = Sprite, EditAnywhere, BlueprintReadOnly)
	bool bClosedSpline;

	UPROPERTY(Category = Sprite, EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bClosedSpline"))
	bool bFilledSpline;

	UPROPERTY()
	class UPaperTerrainSplineComponent* AssociatedSpline;

	/** Random seed used for choosing which spline meshes to use. */
	UPROPERTY(Category=Sprite, EditAnywhere)
	int32 RandomSeed;

	/** The overlap amount between segments */
	UPROPERTY(Category=Sprite, EditAnywhere)
	float SegmentOverlapAmount;

protected:
	/** The color of the terrain (passed to the sprite material as a vertex color) */
	UPROPERTY(Category=Sprite, BlueprintReadOnly, Interp)
	FLinearColor TerrainColor;

	/** Number of steps per spline segment to place in the reparameterization table */
	UPROPERTY(Category=Sprite, EditAnywhere, meta=(ClampMin=4, UIMin=4), AdvancedDisplay)
	int32 ReparamStepsPerSegment;

	/** Collision domain (no collision, 2D (experimental), or 3D) */
	UPROPERTY(Category=Collision, EditAnywhere)
	TEnumAsByte<ESpriteCollisionMode::Type> SpriteCollisionDomain;

	/** The extrusion thickness of collision geometry when using a 3D collision domain */
	UPROPERTY(Category=Collision, EditAnywhere)
	float CollisionThickness;

public:
	// UObject interface
	virtual const UObject* AdditionalStatObject() const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// End of UObject interface

	// UActorComponent interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	// End of UActorComponent interface

	// UPrimitiveComponent interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual UBodySetup* GetBodySetup() override;
	// End of UPrimitiveComponent interface

	// Set color of the terrain
	UFUNCTION(BlueprintCallable, Category="Sprite")
	void SetTerrainColor(FLinearColor NewColor);

protected:
	void SpawnSegments(const TArray<FTerrainSegment>& TerrainSegments, bool bGenerateSegmentColliders);
	
	void GenerateFillRenderDataFromPolygon(const class UPaperSprite* NewSprite, FSpriteDrawCallRecord& FillDrawCall, const FVector2D& TextureSize, const TArray<FVector2D>& TriangulatedPolygonVertices);
	void GenerateCollisionDataFromPolygon(const TArray<FVector2D>& SplinePolyVertices2D, const TArray<float>& TerrainOffsets, const TArray<FVector2D>& TriangulatedPolygonVertices);
	void InsertConvexCollisionDataFromPolygon(const TArray<FVector2D>& ClosedPolyVertices2D);
	void ConstrainSplinePointsToXZ();

	void OnSplineEdited();

	/** Description of collision */
	UPROPERTY(Transient, DuplicateTransient)
	class UBodySetup* CachedBodySetup;

	TArray<FPaperTerrainSpriteGeometry> GeneratedSpriteGeometry;
	
	FTransform GetTransformAtDistance(float InDistance) const;
};


// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "Serialization/BulkData.h"
#include "AI/Navigation/NavigationTypes.h"
#include "NavCollision.generated.h"

class FPrimitiveDrawInterface;
struct FCompositeNavModifier;

USTRUCT()
struct FNavCollisionCylinder
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Cylinder)
	FVector Offset;

	UPROPERTY(EditAnywhere, Category=Cylinder)
	float Radius;

	UPROPERTY(EditAnywhere, Category=Cylinder)
	float Height;
};

USTRUCT()
struct FNavCollisionBox
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Box)
	FVector Offset;

	UPROPERTY(EditAnywhere, Category=Box)
	FVector Extent;
};

struct FNavCollisionConvex
{
	TNavStatArray<FVector> VertexBuffer;
	TNavStatArray<int32> IndexBuffer;
};

UCLASS(config=Engine)
class ENGINE_API UNavCollision : public UObject
{
	GENERATED_UCLASS_BODY()

	FNavCollisionConvex TriMeshCollision;
	FNavCollisionConvex ConvexCollision;
	TNavStatArray<int32> ConvexShapeIndices;

	/** list of nav collision cylinders */
	UPROPERTY(EditAnywhere, Category=Navigation)
	TArray<FNavCollisionCylinder> CylinderCollision;

	/** list of nav collision boxes */
	UPROPERTY(EditAnywhere, Category=Navigation)
	TArray<FNavCollisionBox> BoxCollision;

	/** navigation area type (empty = default obstacle) */
	UPROPERTY(EditAnywhere, Category=Navigation)
	TSubclassOf<class UNavArea> AreaClass;

	/** If set, mesh will be used as dynamic obstacle (don't create navmesh on top, much faster adding/removing) */
	UPROPERTY(EditAnywhere, Category=Navigation, config)
	uint32 bIsDynamicObstacle : 1;

	/** If set, convex collisions will be exported offline for faster runtime navmesh building (increases memory usage) */
	UPROPERTY(EditAnywhere, Category=Navigation, config)
	uint32 bGatherConvexGeometry : 1;

	/** convex collisions are ready to use */
	uint32 bHasConvexGeometry : 1;

	/** if set, convex geometry will be rebuild instead of using cooked data */
	uint32 bForceGeometryRebuild : 1;

	/** Guid of associated BodySetup */
	FGuid BodySetupGuid;

	/** Cooked data for each format */
	FFormatContainer CookedFormatData;

	//~ Begin UObject Interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	//~ End UObject Interface.

	FGuid GetGuid() const;

	/** Tries to read data from DDC, and if that fails gathers navigation
	 *	collision data, stores it and uploads to DDC */
	void Setup(class UBodySetup* BodySetup);

	/** copy user settings from other nav collision data */
	void CopyUserSettings(const UNavCollision& OtherData);

	/** show cylinder and box collisions */
	void DrawSimpleGeom(FPrimitiveDrawInterface* PDI, const FTransform& Transform, const FColor Color);

	/** Get data for dynamic obstacle */
	void GetNavigationModifier(FCompositeNavModifier& Modifier, const FTransform& LocalToWorld);

	/** Read collisions data */
	void GatherCollision();

protected:
	void ClearCollision();

	FORCEINLINE bool ShouldUseConvexCollision() const { return bGatherConvexGeometry || (CylinderCollision.Num() == 0 && BoxCollision.Num() == 0);  }

#if WITH_EDITOR
	void InvalidatePhysicsData();
#endif // WITH_EDITOR
	FByteBulkData* GetCookedData(FName Format);
};

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "LandscapeInfo.h"
#include "Components/PrimitiveComponent.h"
#include "LandscapeSplinesComponent.generated.h"

class ALandscapeProxy;
class FPrimitiveSceneProxy;
class UControlPointMeshComponent;
class ULandscapeSplineControlPoint;
class ULandscapeSplineSegment;
class UMeshComponent;
class USplineMeshComponent;
class UStaticMesh;
class UTexture2D;

// structs for ForeignWorldSplineDataMap
// these are editor-only, but we don't have the concept of an editor-only USTRUCT
USTRUCT()
struct FForeignControlPointData
{
	GENERATED_USTRUCT_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid ModificationKey;

	UPROPERTY()
	UControlPointMeshComponent* MeshComponent;

	UPROPERTY()
	TLazyObjectPtr<ULandscapeSplineControlPoint> Identifier;

	friend bool operator==(const FForeignControlPointData& LHS, const FForeignControlPointData& RHS)
	{
		return LHS.Identifier == RHS.Identifier;
	}
#endif
};

USTRUCT()
struct FForeignSplineSegmentData
{
	GENERATED_USTRUCT_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid ModificationKey;

	UPROPERTY()
	TArray<USplineMeshComponent*> MeshComponents;

	UPROPERTY()
	TLazyObjectPtr<ULandscapeSplineSegment> Identifier;

	friend bool operator==(const FForeignSplineSegmentData& LHS, const FForeignSplineSegmentData& RHS)
	{
		return LHS.Identifier == RHS.Identifier;
	}
#endif
};

USTRUCT()
struct FForeignWorldSplineData
{
	GENERATED_USTRUCT_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TMap<TLazyObjectPtr<ULandscapeSplineControlPoint>, FForeignControlPointData> ForeignControlPointDataMap_DEPRECATED;

	UPROPERTY()
	TArray<FForeignControlPointData> ForeignControlPointData;

	UPROPERTY()
	TMap<TLazyObjectPtr<ULandscapeSplineSegment>, FForeignSplineSegmentData> ForeignSplineSegmentDataMap_DEPRECATED;

	UPROPERTY()
	TArray<FForeignSplineSegmentData> ForeignSplineSegmentData;
#endif

#if WITH_EDITOR
	bool IsEmpty();

	FForeignControlPointData* FindControlPoint(ULandscapeSplineControlPoint* InIdentifer);
	FForeignSplineSegmentData* FindSegmentData(ULandscapeSplineSegment* InIdentifer);
#endif
};

//////////////////////////////////////////////////////////////////////////
// ULandscapeSplinesComponent
//////////////////////////////////////////////////////////////////////////

UCLASS(MinimalAPI)
class ULandscapeSplinesComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	/** Resolution of the spline, in distance per point */
	UPROPERTY()
	float SplineResolution;

	/** Color to use to draw the splines */
	UPROPERTY()
	FColor SplineColor;

	/** Sprite used to draw control points */
	UPROPERTY()
	UTexture2D* ControlPointSprite;

	/** Mesh used to draw splines that have no mesh */
	UPROPERTY()
	UStaticMesh* SplineEditorMesh;

	/** Whether we are in-editor and showing spline editor meshes */
	UPROPERTY(NonTransactional, Transient)
	uint32 bShowSplineEditorMesh:1;
#endif

protected:
	UPROPERTY(TextExportTransient)
	TArray<ULandscapeSplineControlPoint*> ControlPoints;

	UPROPERTY(TextExportTransient)
	TArray<ULandscapeSplineSegment*> Segments;

#if WITH_EDITORONLY_DATA
	// Serialized
	UPROPERTY(TextExportTransient)
	TMap<TSoftObjectPtr<UWorld>, FForeignWorldSplineData> ForeignWorldSplineDataMap;

	// Transient - rebuilt on load
	TMap<UMeshComponent*, UObject*> MeshComponentLocalOwnersMap;
	TMap<UMeshComponent*, TLazyObjectPtr<UObject>> MeshComponentForeignOwnersMap;
#endif

	// References to components owned by landscape splines in other levels
	// for cooked build (uncooked keeps references via ForeignWorldSplineDataMap)
	UPROPERTY(TextExportTransient)
	TArray<UMeshComponent*> CookedForeignMeshComponents;

public:
	void CheckSplinesValid();
	bool ModifySplines(bool bAlwaysMarkDirty = true);

#if WITH_EDITOR
	virtual void ShowSplineEditorMesh(bool bShow);

	// Rebuilds all spline points and meshes for all spline control points and segments in this splines component
	// @param bBuildCollision Building collision data is very slow, so during interactive changes pass false to improve user experience (make sure to call with true when done)
	virtual void RebuildAllSplines(bool bBuildCollision = true);

	// returns a suitable ULandscapeSplinesComponent to place streaming meshes into, given a location
	// falls back to "this" if it can't find another suitable, so never returns nullptr
	// @param bCreate whether to create a component if a suitable actor is found but it has no splines component yet
	ULandscapeSplinesComponent* GetStreamingSplinesComponentByLocation(const FVector& LocalLocation, bool bCreate = true);

	// returns the matching ULandscapeSplinesComponent for a given level, *can return null*
	// @param bCreate whether to create a component if a suitable actor is found but it has no splines component yet
	ULandscapeSplinesComponent* GetStreamingSplinesComponentForLevel(ULevel* Level, bool bCreate = true);

	// gathers and returns all currently existing 
	TArray<ULandscapeSplinesComponent*> GetAllStreamingSplinesComponents();

	virtual void UpdateModificationKey(ULandscapeSplineSegment* Owner);
	virtual void UpdateModificationKey(ULandscapeSplineControlPoint* Owner);
	virtual void AddForeignMeshComponent(ULandscapeSplineSegment* Owner, USplineMeshComponent* Component);
	virtual void RemoveForeignMeshComponent(ULandscapeSplineSegment* Owner, USplineMeshComponent* Component);
	virtual void RemoveAllForeignMeshComponents(ULandscapeSplineSegment* Owner);
	virtual void AddForeignMeshComponent(ULandscapeSplineControlPoint* Owner, UControlPointMeshComponent* Component);
	virtual void RemoveForeignMeshComponent(ULandscapeSplineControlPoint* Owner, UControlPointMeshComponent* Component);
	virtual void DestroyOrphanedForeignMeshComponents(UWorld* OwnerWorld);
	virtual UControlPointMeshComponent*   GetForeignMeshComponent(ULandscapeSplineControlPoint* Owner);
	virtual TArray<USplineMeshComponent*> GetForeignMeshComponents(ULandscapeSplineSegment* Owner);

	virtual UObject* GetOwnerForMeshComponent(const UMeshComponent* SplineMeshComponent);

	void AutoFixMeshComponentErrors(UWorld* OtherWorld);
#endif

	//~ Begin UObject Interface

	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif // WITH_EDITOR
	//~ End UObject Interface

	//~ Begin UActorComponent Interface
#if WITH_EDITOR
	virtual void CheckForErrors() override;
#endif
	virtual void OnRegister() override;
	//~ End UActorComponent Interface
	
	//~ Begin UPrimitiveComponent Interface.
#if WITH_EDITOR
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
#endif
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End UPrimitiveComponent Interface.

	// many friends
	friend class FLandscapeToolSplines;
	friend class FLandscapeSplinesSceneProxy;
	friend class ULandscapeSplineControlPoint;
	friend class ULandscapeSplineSegment;
#if WITH_EDITOR
	// TODO - move this out of ULandscapeInfo
	friend bool ULandscapeInfo::ApplySplinesInternal(bool bOnlySelected, ALandscapeProxy* Landscape);
#endif
};

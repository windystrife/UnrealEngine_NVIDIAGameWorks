// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "Components/SplineMeshComponent.h"
#include "LandscapeSplinesComponent.h"
#include "LandscapeSplineSegment.generated.h"

class ULandscapeSplineControlPoint;
class UStaticMesh;

//Forward declarations
class ULandscapeSplineControlPoint;

USTRUCT()
struct FLandscapeSplineInterpPoint
{
	GENERATED_USTRUCT_BODY()

	/** Center Point */
	UPROPERTY()
	FVector Center;

	/** Left Point */
	UPROPERTY()
	FVector Left;

	/** Right Point */
	UPROPERTY()
	FVector Right;

	/** Left Falloff Point */
	UPROPERTY()
	FVector FalloffLeft;

	/** Right FalloffPoint */
	UPROPERTY()
	FVector FalloffRight;

	/** Start/End Falloff fraction */
	UPROPERTY()
	float StartEndFalloff;

	FLandscapeSplineInterpPoint()
	{
	}

	FLandscapeSplineInterpPoint(FVector InCenter, FVector InLeft, FVector InRight, FVector InFalloffLeft, FVector InFalloffRight, float InStartEndFalloff) :
		Center(InCenter),
		Left(InLeft),
		Right(InRight),
		FalloffLeft(InFalloffLeft),
		FalloffRight(InFalloffRight),
		StartEndFalloff(InStartEndFalloff)
	{
	}
};

USTRUCT()
struct FLandscapeSplineSegmentConnection
{
	GENERATED_USTRUCT_BODY()

	// Control point connected to this end of the segment
	UPROPERTY()
	ULandscapeSplineControlPoint* ControlPoint;

	// Tangent length of the connection
	UPROPERTY(EditAnywhere, Category=LandscapeSplineSegmentConnection)
	float TangentLen;

	// Socket on the control point that we are connected to
	UPROPERTY(EditAnywhere, Category=LandscapeSplineSegmentConnection)
	FName SocketName;

	FLandscapeSplineSegmentConnection()
		: ControlPoint(nullptr)
		, TangentLen(0.0f)
		, SocketName(NAME_None)
	{
	}
};

// Deprecated
UENUM()
enum LandscapeSplineMeshOrientation
{
	LSMO_XUp,
	LSMO_YUp,
	LSMO_MAX,
};

USTRUCT()
struct FLandscapeSplineMeshEntry
{
	GENERATED_USTRUCT_BODY()

	/** Mesh to use on the spline */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshEntry)
	UStaticMesh* Mesh;

	/** Overrides mesh's materials */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshEntry, AdvancedDisplay)
	TArray<UMaterialInterface*> MaterialOverrides;

	/** Whether to automatically center the mesh horizontally on the spline */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshEntry, meta=(DisplayName="Center Horizontally"))
	uint32 bCenterH:1;

	/** Tweak to center the mesh correctly on the spline */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshEntry, AdvancedDisplay, meta=(DisplayName="Center Adjust"))
	FVector2D CenterAdjust;

	/** Whether to scale the mesh to fit the width of the spline */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshEntry)
	uint32 bScaleToWidth:1;

	/** Scale of the spline mesh, (Z=Forwards) */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshEntry)
	FVector Scale;

	/** Orientation of the spline mesh, X=Up or Y=Up */
	UPROPERTY()
	TEnumAsByte<LandscapeSplineMeshOrientation> Orientation_DEPRECATED;

	/** Chooses the forward axis for the spline mesh orientation */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshEntry)
	TEnumAsByte<ESplineMeshAxis::Type> ForwardAxis;

	/** Chooses the up axis for the spline mesh orientation */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshEntry)
	TEnumAsByte<ESplineMeshAxis::Type> UpAxis;

	FLandscapeSplineMeshEntry() :
		Mesh(nullptr),
		MaterialOverrides(),
		bCenterH(true),
		CenterAdjust(0, 0),
		bScaleToWidth(true),
		Scale(1,1,1),
		Orientation_DEPRECATED(LSMO_YUp),
		ForwardAxis(ESplineMeshAxis::X),
		UpAxis(ESplineMeshAxis::Z)
	{
	}

	bool IsValid() const;
};


UCLASS(Within=LandscapeSplinesComponent,autoExpandCategories=(LandscapeSplineSegment,LandscapeSplineMeshes),MinimalAPI)
class ULandscapeSplineSegment : public UObject
{
	GENERATED_UCLASS_BODY()

// Directly editable data:
	UPROPERTY(EditAnywhere, EditFixedSize, Category=LandscapeSplineSegment)
	FLandscapeSplineSegmentConnection Connections[2];

#if WITH_EDITORONLY_DATA
	/**
	 * Name of blend layer to paint when applying spline to landscape
	 * If "none", no layer is painted
	 */
	UPROPERTY(EditAnywhere, Category=LandscapeDeformation)
	FName LayerName;

	/** If the spline is above the terrain, whether to raise the terrain up to the level of the spline when applying it to the landscape. */
	UPROPERTY(EditAnywhere, Category=LandscapeDeformation)
	uint32 bRaiseTerrain:1;

	/** If the spline is below the terrain, whether to lower the terrain down to the level of the spline when applying it to the landscape. */
	UPROPERTY(EditAnywhere, Category=LandscapeDeformation)
	uint32 bLowerTerrain:1;

	/** Spline meshes from this list are used in random order along the spline. */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshes)
	TArray<FLandscapeSplineMeshEntry> SplineMeshes;

	/** Whether to generate collision for the Spline Meshes. */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshes)
	uint32 bEnableCollision:1;

	/** Whether the Spline Meshes should cast a shadow. */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshes)
	uint32 bCastShadow:1;

	/** Random seed used for choosing which order to use spline meshes. Ignored if only one mesh is set. */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshes, AdvancedDisplay)
	int32 RandomSeed;

	/**  Max draw distance for all the mesh pieces used in this spline */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshes, AdvancedDisplay, meta=(DisplayName="Max Draw Distance"))
	float LDMaxDrawDistance;

	/**
	 * Translucent objects with a lower sort priority draw behind objects with a higher priority.
	 * Translucent objects with the same priority are rendered from back-to-front based on their bounds origin.
	 *
	 * Ignored if the object is not translucent.  The default priority is zero.
	 * Warning: This should never be set to a non-default value unless you know what you are doing, as it will prevent the renderer from sorting correctly.
	 */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshes, AdvancedDisplay)
	int32 TranslucencySortPriority;

	/** Whether spline meshes should be placed in landscape proxy streaming levels (true) or the spline's level (false) */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshes, AdvancedDisplay)
	uint32 bPlaceSplineMeshesInStreamingLevels : 1;

protected:
	UPROPERTY(Transient)
	uint32 bSelected : 1;

	UPROPERTY(Transient)
	uint32 bNavDirty : 1;
#endif

// Procedural data:
protected:
	/** Actual data for spline. */
	UPROPERTY()
	FInterpCurveVector SplineInfo;

	/** Spline points */
	UPROPERTY()
	TArray<FLandscapeSplineInterpPoint> Points;

	/** Bounds of points */
	UPROPERTY()
	FBox Bounds;

	/** Spline meshes */
	UPROPERTY(TextExportTransient)
	TArray<USplineMeshComponent*> LocalMeshComponents;

#if WITH_EDITORONLY_DATA
	/** World references for mesh components stored in other streaming levels */
	UPROPERTY(TextExportTransient, NonPIEDuplicateTransient)
	TArray<TSoftObjectPtr<UWorld>> ForeignWorlds;

	/** Key for tracking whether this segment has been modified relative to the mesh components stored in other streaming levels */
	UPROPERTY(TextExportTransient, NonPIEDuplicateTransient)
	FGuid ModificationKey;
#endif

public:
	const FBox& GetBounds() const { return Bounds; }
	const TArray<FLandscapeSplineInterpPoint>& GetPoints() const { return Points; }

#if WITH_EDITOR
	bool IsSplineSelected() const { return bSelected; }
	virtual void SetSplineSelected(bool bInSelected);

	virtual void AutoFlipTangents();

	TMap<ULandscapeSplinesComponent*, TArray<USplineMeshComponent*>> GetForeignMeshComponents();

	virtual void UpdateSplinePoints(bool bUpdateCollision = true);

	void UpdateSplineEditorMesh();
	virtual void DeleteSplinePoints();

	const TArray<TSoftObjectPtr<UWorld>>& GetForeignWorlds() const { return ForeignWorlds; }
	FGuid GetModificationKey() const { return ModificationKey; }
#endif

	virtual void FindNearest(const FVector& InLocation, float& t, FVector& OutLocation, FVector& OutTangent);

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostEditImport() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
protected:
	virtual void PostInitProperties() override;
public:
	//~ End UObject Interface

	friend class FLandscapeToolSplines;
};

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "VIBaseTransformGizmo.h"
#include "VIGizmoHandle.h"
#include "VIPivotTransformGizmo.generated.h"

/**
 * A transform gizmo on the pivot that allows you to interact with selected objects by moving, scaling and rotating.
 */
UCLASS()
class APivotTransformGizmo : public ABaseTransformGizmo
{
	GENERATED_BODY()

public:

	/** Default constructor that sets up CDO properties */
	APivotTransformGizmo();
	
	/** Called by the world interaction system after we've been spawned into the world, to allow
	    us to create components and set everything up nicely for the selected objects that we'll be
		used to manipulate */
	virtual void UpdateGizmo(const EGizmoHandleTypes InGizmoType, const ECoordSystem InGizmoCoordinateSpace, const FTransform& InLocalToWorld, const FBox& InLocalBounds,
		const FVector& InViewLocation, const float InScaleMultiplier, bool bInAllHandlesVisible, const bool bInAllowRotationAndScaleHandles, class UActorComponent* DraggingHandle,
		const TArray<UActorComponent*>& InHoveringOverHandles, const float InGizmoHoverScale, const float InGizmoHoverAnimationDuration) override;

private:
	/** Uniform scale handle group component */
	UPROPERTY()
	class UUniformScaleGizmoHandleGroup* UniformScaleGizmoHandleGroup;

	/** Translation handle group component */
	UPROPERTY()
	class UPivotTranslationGizmoHandleGroup* TranslationGizmoHandleGroup;
	
	/** Scale handle group component */
	UPROPERTY()
	class UPivotScaleGizmoHandleGroup* ScaleGizmoHandleGroup;
	
	/** Plane translation handle group component */
	UPROPERTY()
	class UPivotPlaneTranslationGizmoHandleGroup* PlaneTranslationGizmoHandleGroup;

	/** Rotation handle group component */
	UPROPERTY()
	class UPivotRotationGizmoHandleGroup* RotationGizmoHandleGroup;

	/** Stretch handle group component */
	UPROPERTY()
	class UStretchGizmoHandleGroup* StretchGizmoHandleGroup;

	/** The alpha for gizmo animation when aiming at it with a laser */
	float AimingAtGizmoScaleAlpha;

	/** Handle from previous tick that was dragged */
	UPROPERTY()
	UActorComponent* LastDraggingHandle;
};

/**
 * Axis Gizmo handle for translating
 */
UCLASS()
class UPivotTranslationGizmoHandleGroup : public UAxisGizmoHandleGroup
{
	GENERATED_BODY()

public:

	/** Default constructor that sets up CDO properties */
	UPivotTranslationGizmoHandleGroup();

	/** Updates the gizmo handles */
	virtual void UpdateGizmoHandleGroup( const FTransform& LocalToWorld, const FBox& LocalBounds, const FVector ViewLocation, const bool bAllHandlesVisible, class UActorComponent* DraggingHandle, const TArray< UActorComponent* >& HoveringOverHandles, 
		float AnimationAlpha, float GizmoScale, const float GizmoHoverScale, const float GizmoHoverAnimationDuration, bool& bOutIsHoveringOrDraggingThisHandleGroup ) override;

	/** Gets the GizmoType for this Gizmo handle */
	virtual EGizmoHandleTypes GetHandleType() const override;
};

/**
 * Axis Gizmo handle for scaling
 */
UCLASS()
class UPivotScaleGizmoHandleGroup : public UAxisGizmoHandleGroup
{
	GENERATED_BODY()

public:

	/** Default constructor that sets up CDO properties */
	UPivotScaleGizmoHandleGroup();

	/** Updates the gizmo handles */
	virtual void UpdateGizmoHandleGroup( const FTransform& LocalToWorld, const FBox& LocalBounds,  const FVector ViewLocation, const bool bAllHandlesVisible, class UActorComponent* DraggingHandle, const TArray< UActorComponent* >& HoveringOverHandles, 
		float AnimationAlpha, float GizmoScale, const float GizmoHoverScale, const float GizmoHoverAnimationDuration, bool& bOutIsHoveringOrDraggingThisHandleGroup ) override;

	/** Gets the GizmoType for this Gizmo handle */
	virtual EGizmoHandleTypes GetHandleType() const override;

	/** Returns true if this type of handle is allowed with world space gizmos */
	virtual bool SupportsWorldCoordinateSpace() const override;
};

/**
 * Axis Gizmo handle for plane translation
 */
UCLASS()
class UPivotPlaneTranslationGizmoHandleGroup : public UAxisGizmoHandleGroup
{
	GENERATED_BODY()

public:

	/** Default constructor that sets up CDO properties */
	UPivotPlaneTranslationGizmoHandleGroup();

	/** Updates the gizmo handles */
	virtual void UpdateGizmoHandleGroup( const FTransform& LocalToWorld, const FBox& LocalBounds, const FVector ViewLocation, const bool bAllHandlesVisible, class UActorComponent* DraggingHandle, const TArray< UActorComponent* >& HoveringOverHandles, 
		float AnimationAlpha, float GizmoScale, const float GizmoHoverScale, const float GizmoHoverAnimationDuration, bool& bOutIsHoveringOrDraggingThisHandleGroup ) override;

	/** Gets the GizmoType for this Gizmo handle */
	virtual EGizmoHandleTypes GetHandleType() const override;
};


/**
 * Axis Gizmo handle for rotation
 */
UCLASS()
class UPivotRotationGizmoHandleGroup : public UAxisGizmoHandleGroup
{
	GENERATED_BODY()

public:

	/** Default constructor that sets up CDO properties */
	UPivotRotationGizmoHandleGroup();

	/** Updates the gizmo handles */
	virtual void UpdateGizmoHandleGroup(const FTransform& LocalToWorld, const FBox& LocalBounds, const FVector ViewLocation, const bool bAllHandlesVisible, class UActorComponent* DraggingHandle, const TArray< UActorComponent* >& HoveringOverHandles, 
		float AnimationAlpha, float GizmoScale, const float GizmoHoverScale, const float GizmoHoverAnimationDuration, bool& bOutIsHoveringOrDraggingThisHandleGroup ) override;

	/** Gets the GizmoType for this Gizmo handle */
	virtual EGizmoHandleTypes GetHandleType() const override;

private:

	/** Updates the root of an indicator to rotate the indicator itself */
	void UpdateIndicator(USceneComponent* IndicatorRoot, const FVector& Direction, const uint32 FacingAxisIndex);

	/** Make the components visible when dragging rotation */
	void ShowRotationVisuals(const bool bInShow);

	void SetupIndicator(USceneComponent* RootComponent, UGizmoHandleMeshComponent* IndicatorMeshComponent, UStaticMesh* Mesh);
	
	void SetIndicatorColor(UStaticMeshComponent* InMeshComponent, const FLinearColor& InHandleColor);

	/** Root component of all the mesh components that are used to visualize the rotation when dragging */
	UPROPERTY()
	USceneComponent* RootFullRotationHandleComponent;

	/** When dragging a rotation handle the full rotation circle appears */
	UPROPERTY()
	class UGizmoHandleMeshComponent* FullRotationHandleMeshComponent;

	/** The mesh that indicated the start rotation */
	UPROPERTY()
	class UGizmoHandleMeshComponent* StartRotationIndicatorMeshComponent;

	/** The root component of the start rotation indicator */
	UPROPERTY()
	USceneComponent* RootStartRotationIdicatorComponent;

	/** The mesh that indicated the delta rotation */
	UPROPERTY()
	class UGizmoHandleMeshComponent* DeltaRotationIndicatorMeshComponent;

	/** The root component of the delta rotation indicator */
	UPROPERTY()
	USceneComponent* RootDeltaRotationIndicatorComponent;

	/** The rotation when starting to drag the gizmo */
	TOptional<FQuat> StartDragRotation;
};

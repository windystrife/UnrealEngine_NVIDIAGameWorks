// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "Components/ActorComponent.h"
#include "ViewportInteractionTypes.h"
#include "UnrealWidget.h"
#include "ViewportDragOperation.generated.h"

/**
 * Base class for interactable drag calculations
 */
UCLASS()
class VIEWPORTINTERACTION_API UViewportDragOperation : public UObject
{
	GENERATED_BODY()

public:

	/** 
	 * Execute dragging 
	 *
	 * @param UViewportInteractor - The interactor causing the dragging
	 * @param IViewportInteractableInterface - The interactable owning this drag operation
	 */
	virtual void ExecuteDrag(class UViewportInteractor* Interactor, class IViewportInteractableInterface* Interactable){};

	/** 
	 * Execute dragging 
	 *
	 * @param UViewportInteractor - The interactor causing the dragging
	 * @param IViewportInteractableInterface - The interactable owning this drag operation
	 */
	virtual void ExecuteDrag(struct FDraggingTransformableData& DraggingData){};

	/** @todo ViewportInteraction: Temporary way to check if a drag operation needs to constrain the drag delta on a plane of a gizmo axis. */
	bool bPlaneConstraint;
};


/**
 * Container component for UViewportDragOperation that can be used by objects in the world that are draggable and implement the ViewportInteractableInterface
 */
UCLASS()
class VIEWPORTINTERACTION_API UViewportDragOperationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	
	/** Default constructor. */
	UViewportDragOperationComponent();

	/** Get the actual dragging operation */
	UViewportDragOperation* GetDragOperation();

	/** Sets the drag operation class that will be used next time starting dragging */
	void SetDragOperationClass( const TSubclassOf<UViewportDragOperation> InDragOperation );

	/** Starts new dragging operation */
	void StartDragOperation();

	/* Destroys the dragoperation */
	void ClearDragOperation();

	/** If this operation is currently active. */
	bool IsDragging() const;

private:

	/** The actual dragging implementation */
	UPROPERTY()
	UViewportDragOperation* DragOperation;

	/** The next class that will be used as drag operation */
	UPROPERTY()
	TSubclassOf<UViewportDragOperation> DragOperationSubclass;
};

/**
 * Data structure that holds all arguments that can be used while dragging a transformable.
 */
USTRUCT()
struct VIEWPORTINTERACTION_API FDraggingTransformableData
{
	GENERATED_BODY()

	FDraggingTransformableData() :
		Interactor(nullptr),
		WorldInteraction(nullptr),
		PassDraggedTo(FVector::ZeroVector),
		OptionalHandlePlacement(),
		DragDelta(FVector::ZeroVector),
		ConstrainedDragDelta(FVector::ZeroVector),
		OtherHandDragDelta(FVector::ZeroVector),
		DraggedTo(FVector::ZeroVector),
		OtherHandDraggedTo(FVector::ZeroVector),
		DragDeltaFromStart(FVector::ZeroVector),
		OtherHandDragDeltaFromStart(FVector::ZeroVector),
		LaserPointerStart(FVector::ZeroVector),
		LaserPointerDirection(FVector::ZeroVector),
		GizmoStartTransform(FTransform::Identity),
		GizmoLastTransform(FTransform::Identity),
		OutGizmoUnsnappedTargetTransform(FTransform::Identity),
		GizmoStartLocalBounds(EForceInit::ForceInitToZero),
		GizmoCoordinateSpace(ECoordSystem::COORD_World),
		bOutMovedTransformGizmo(false),
		bOutShouldApplyVelocitiesFromDrag(false),
		OutUnsnappedDraggedTo(FVector::ZeroVector),
		bOutTranslated(false),
		bOutRotated(false),
		bOutScaled(false),
		bAllowSnap(true)
	{}

	class UViewportInteractor* Interactor;
	class UViewportWorldInteraction* WorldInteraction;

	FVector PassDraggedTo;
	TOptional<FTransformGizmoHandlePlacement> OptionalHandlePlacement;
	FVector DragDelta;
	FVector ConstrainedDragDelta;
	FVector OtherHandDragDelta;
	FVector DraggedTo;
	FVector OtherHandDraggedTo;
	FVector DragDeltaFromStart;
	FVector OtherHandDragDeltaFromStart;
	FVector LaserPointerStart;
	FVector LaserPointerDirection;
	FTransform GizmoStartTransform;
	FTransform GizmoLastTransform;
	FTransform OutGizmoUnsnappedTargetTransform;
	FBox GizmoStartLocalBounds;
	ECoordSystem GizmoCoordinateSpace;

	bool bOutMovedTransformGizmo;
	bool bOutShouldApplyVelocitiesFromDrag;
	FVector OutUnsnappedDraggedTo;
	bool bOutTranslated;
	bool bOutRotated;
	bool bOutScaled;
	bool bAllowSnap;
};

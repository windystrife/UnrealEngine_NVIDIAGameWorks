// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ViewportDragOperation.h"	
#include "ViewportInteractionDragOperations.generated.h"

/**
 * Gizmo translation on one axis.
 */
UCLASS()
class VIEWPORTINTERACTION_API UTranslationDragOperation : public UViewportDragOperation
{
	GENERATED_BODY()

public:
	virtual void ExecuteDrag(struct FDraggingTransformableData& DraggingData) override;
};

/**
 * Gizmo translation on two axes.
 */
UCLASS()
class VIEWPORTINTERACTION_API UPlaneTranslationDragOperation: public UViewportDragOperation
{
	GENERATED_BODY()

public:
	UPlaneTranslationDragOperation();
	virtual void ExecuteDrag(struct FDraggingTransformableData& DraggingData) override;
};

/**
 * Rotation around one axis based on input angle.
 */
UCLASS()
class VIEWPORTINTERACTION_API URotateOnAngleDragOperation: public UViewportDragOperation
{
	GENERATED_BODY()

public:
	URotateOnAngleDragOperation();
	
	virtual void ExecuteDrag(struct FDraggingTransformableData& DraggingData) override;

	/** When RotateOnAngle we intersect on a plane to rotate the transform gizmo. This is the local point from the transform gizmo location of that intersect */
	FVector GetLocalIntersectPointOnRotationGizmo() const;

private:

	/** Starting angle when rotating an object with  ETransformGizmoInteractionType::RotateOnAngle */
	TOptional<float> StartDragAngleOnRotation;

	/** The direction of where the rotation handle is facing when starting rotation */
	TOptional<FVector> DraggingRotationHandleDirection;

	/** Where the laser intersected on the gizmo rotation aligned plane. */
	FVector LocalIntersectPointOnRotationGizmo;
};

/**
 * Scale on one axis.
 */
UCLASS()
class VIEWPORTINTERACTION_API UScaleDragOperation : public UViewportDragOperation
{
	GENERATED_BODY()

public:
	virtual void ExecuteDrag(struct FDraggingTransformableData& DraggingData) override;
};

/**
 * Scale on all axes.
 */
UCLASS()
class  VIEWPORTINTERACTION_API UUniformScaleDragOperation : public UViewportDragOperation
{
	GENERATED_BODY()

public:
	virtual void ExecuteDrag(struct FDraggingTransformableData& DraggingData) override;
};

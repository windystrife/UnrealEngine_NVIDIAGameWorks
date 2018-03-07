// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "VIGizmoHandle.h"
#include "ViewportDragOperation.h"
#include "VIStretchGizmoHandle.generated.h"

enum class EGizmoHandleTypes : uint8;

/**
 * Gizmo handle for stretching/scaling
 */
UCLASS()
class VIEWPORTINTERACTION_API UStretchGizmoHandleGroup : public UGizmoHandleGroup
{
	GENERATED_BODY()

public:

	/** Default constructor that sets up CDO properties */
	UStretchGizmoHandleGroup();
	
	/** Updates the Gizmo handles */
	virtual void UpdateGizmoHandleGroup( const FTransform& LocalToWorld, const FBox& LocalBounds, const FVector ViewLocation, const bool bAllHandlesVisible, class UActorComponent* DraggingHandle, const TArray< UActorComponent* >& HoveringOverHandles, 
		float AnimationAlpha, float GizmoScale, const float GizmoHoverScale, const float GizmoHoverAnimationDuration, bool& bOutIsHoveringOrDraggingThisHandleGroup ) override;

	/** Gets the GizmoType for this Gizmo handle */
	virtual EGizmoHandleTypes GetHandleType() const override;

	/** Returns true if this type of handle is allowed with world space gizmos */
	virtual bool SupportsWorldCoordinateSpace() const override;
};

UCLASS()
class UStretchGizmoHandleDragOperation : public UViewportDragOperation
{
	GENERATED_BODY()

public:

	// IViewportDragOperation
	virtual void ExecuteDrag(struct FDraggingTransformableData& DraggingData) override;
};

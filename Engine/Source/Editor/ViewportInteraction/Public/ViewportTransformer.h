// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ViewportInteractor.h"
#include "ViewportWorldInteraction.h"
#include "ViewportTransformer.generated.h"

UCLASS( abstract )
class VIEWPORTINTERACTION_API UViewportTransformer : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION()
	virtual void Init( UViewportWorldInteraction* InitViewportWorldInteraction );

	UFUNCTION()
	virtual void Shutdown();

	/** @return If this transformer can be used to align its transformable to actors in the scene */
	UFUNCTION()
	virtual bool CanAlignToActors() const
	{
		return false;
	};

	/** @return True if the transform gizmo should be aligned to the center of the bounds of all selected objects with more than one is selected.  Otherwise it will be at the pivot of the last transformable, similar to legacl editor actor selection */
	UFUNCTION()
	virtual bool ShouldCenterTransformGizmoPivot() const
	{
		return false;
	}

	/** When starting to drag */
	UFUNCTION()
	virtual void OnStartDragging(UViewportInteractor* Interactor) {};

	/** When ending drag */
	UFUNCTION()
	virtual void OnStopDragging(UViewportInteractor* Interactor) {};

protected:

	/** The viewport world interaction object we're registered with */
	UPROPERTY()
	UViewportWorldInteraction* ViewportWorldInteraction;

};


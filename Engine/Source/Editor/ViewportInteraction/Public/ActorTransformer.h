// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewportTransformer.h"
#include "ActorTransformer.generated.h"


UCLASS()
class VIEWPORTINTERACTION_API UActorTransformer : public UViewportTransformer
{
	GENERATED_BODY()

public:

	// UViewportTransformer overrides
	virtual void Init( class UViewportWorldInteraction* InitViewportWorldInteraction ) override;
	virtual void Shutdown() override;
	virtual bool CanAlignToActors() const override
	{
		return true;
	}
	virtual void OnStartDragging(class UViewportInteractor* Interactor) override;
	virtual void OnStopDragging(class UViewportInteractor* Interactor) override;

protected:

	/** Called when selection changes in the level */
	void OnActorSelectionChanged( UObject* ChangedObject );

	/** Creates a list of transformables with the current selected actors */
	void UpdateTransformables();

};


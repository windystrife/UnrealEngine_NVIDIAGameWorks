// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ViewportInteractor.h"
#include "MouseCursorInteractor.generated.h"

/**
 * Viewport interactor for a mouse cursor
 */
UCLASS()
class VIEWPORTINTERACTION_API UMouseCursorInteractor : public UViewportInteractor
{
	GENERATED_BODY()

public:

	/** Default constructor. */
	UMouseCursorInteractor();

	/** Initialize default values */
	void Init();

	// ViewportInteractor overrides
	virtual void PollInput() override;
	virtual bool IsModifierPressed() const override;

protected:

	virtual bool AllowLaserSmoothing() const;

private:

	/** Whether the control key was pressed the last time input was polled */
	bool bIsControlKeyPressed;

};

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "EdMode.h"
#include "ActorPickerMode.h"

class FEditorViewportClient;
class FViewport;

namespace EPickState
{
	enum Type
	{
		NotOverViewport,
		OverViewport,
		OverIncompatibleActor,
		OverActor,
	};
}

/**
 * Editor mode used to interactively pick actors in the level editor viewports.
 */
class FEdModeActorPicker : public FEdMode
{
public:
	FEdModeActorPicker();

	/** Begin FEdMode interface */
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
	virtual bool MouseEnter(FEditorViewportClient* ViewportClient, FViewport* Viewport,int32 x, int32 y) override;
	virtual bool MouseLeave(FEditorViewportClient* ViewportClient, FViewport* Viewport) override;
	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) override;
	virtual bool LostFocus(FEditorViewportClient* ViewportClient, FViewport* Viewport) override;
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	virtual bool GetCursor(EMouseCursor::Type& OutCursor) const override;
	virtual bool UsesToolkits() const override;
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override;
	virtual void Initialize() override;
	/** End FEdMode interface */

	/** End the mode */
	virtual void Exit() override;

	/** Delegate used to display information about picking near the cursor */
	FText GetCursorDecoratorText() const;

	bool IsActorValid(const AActor *const Actor) const;

	/** Flag for display state */
	TWeakObjectPtr<AActor> HoveredActor;

	/** Flag for display state */
	EPickState::Type PickState;

	/** The window that owns the decorator widget */
	TSharedPtr<SWindow> CursorDecoratorWindow;

	/** Delegates used to pick actors */
	FOnActorSelected OnActorSelected;
	FOnGetAllowedClasses OnGetAllowedClasses;
	FOnShouldFilterActor OnShouldFilterActor;
};

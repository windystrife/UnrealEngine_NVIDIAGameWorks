// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "InputCoreTypes.h"
#include "EditorModes.h"
#include "EdMode.h"
#include "EditorViewportClient.h"

class FGameplayDebuggerEdMode : public FEdMode
{
public:
	FGameplayDebuggerEdMode() : FocusedViewport(nullptr) {}

	// FEdMode interface
	virtual bool UsesToolkits() const override { return true; }
	virtual void Enter() override;
	virtual void Exit() override;
	virtual bool ReceivedFocus(FEditorViewportClient* ViewportClient, FViewport* Viewport) override;
	virtual bool LostFocus(FEditorViewportClient* ViewportClient, FViewport* Viewport) override;
	virtual bool InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent) override;
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime);
	// End of FEdMode interface

	static void SafeCloseMode();

	static const FName EM_GameplayDebugger;

private:
	FEditorViewportClient* FocusedViewport;
	bool bPrevGAreScreenMessagesEnabled;

	void EnableViewportClientFlags(FEditorViewportClient* ViewportClient, bool bEnable);
};

#endif // WITH_EDITOR

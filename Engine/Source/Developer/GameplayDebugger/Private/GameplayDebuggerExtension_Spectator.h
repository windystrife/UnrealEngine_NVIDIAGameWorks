// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayDebuggerExtension.h"

class ADebugCameraController;

class FGameplayDebuggerExtension_Spectator : public FGameplayDebuggerExtension
{
public:

	FGameplayDebuggerExtension_Spectator();

	virtual void OnDeactivated() override;
	virtual FString GetDescription() const override;

	static TSharedRef<FGameplayDebuggerExtension> MakeInstance();

protected:

	void ToggleSpectatorMode();

	uint32 bHasInputBinding : 1;
	mutable uint32 bIsCachedDescriptionValid : 1;
	mutable FString CachedDescription;
	TWeakObjectPtr<ADebugCameraController> SpectatorController;
};

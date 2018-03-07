// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Blueprint for Blutility editor utilities
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Blueprint.h"
#include "EditorUtilityBlueprint.generated.h"

UCLASS()
class UEditorUtilityBlueprint : public UBlueprint
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	// UBlueprint interface
	virtual bool SupportedByDefaultBlueprintFactory() const override
	{
		return false;
	}
	// End of UBlueprint interface
#endif
};

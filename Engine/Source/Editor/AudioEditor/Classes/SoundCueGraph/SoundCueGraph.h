// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraph.h"
#include "SoundCueGraph.generated.h"

UCLASS(MinimalAPI)
class USoundCueGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()

	/** Returns the SoundCue that contains this graph */
	AUDIOEDITOR_API class USoundCue* GetSoundCue() const;
};


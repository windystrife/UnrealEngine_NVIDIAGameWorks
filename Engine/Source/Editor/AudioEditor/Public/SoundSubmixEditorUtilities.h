// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UEdGraph;
class UEdGraphPin;

class AUDIOEDITOR_API FSoundSubmixEditorUtilities
{
public:

	/**
	 * Create a new SoundSubmix
	 *
	 * @param	Graph		Graph representing sound classes
	 * @param	FromPin		Pin that was dragged to create sound class
	 * @param	Location	Location for new sound class
	 */
	static void CreateSoundSubmix(const UEdGraph* Graph, UEdGraphPin* FromPin, FVector2D Location, const FString& Name);

private:
	FSoundSubmixEditorUtilities() {}
};

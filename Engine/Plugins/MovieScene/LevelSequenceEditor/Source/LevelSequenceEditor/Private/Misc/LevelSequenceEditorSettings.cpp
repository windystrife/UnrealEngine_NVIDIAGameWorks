// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/LevelSequenceEditorSettings.h"

ULevelSequenceMasterSequenceSettings::ULevelSequenceMasterSequenceSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MasterSequenceName(TEXT("Sequence"))
	, MasterSequenceSuffix(TEXT("Master"))
	, MasterSequenceNumShots(5)
{
	MasterSequenceBasePath.Path = TEXT("/Game/Cinematics/Sequences");
}

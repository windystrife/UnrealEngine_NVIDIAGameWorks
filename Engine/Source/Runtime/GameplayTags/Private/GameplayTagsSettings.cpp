// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsSettings.h"
#include "GameplayTagsModule.h"

UGameplayTagsList::UGameplayTagsList(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// No config filename, needs to be set at creation time
}

void UGameplayTagsList::SortTags()
{
	GameplayTagList.Sort();
}

UGameplayTagsSettings::UGameplayTagsSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ConfigFileName = GetDefaultConfigFilename();
	ImportTagsFromConfig = false;
	WarnOnInvalidTags = true;
	FastReplication = false;
	NumBitsForContainerSize = 6;
	NetIndexFirstBitSegment = 16;
}

#if WITH_EDITOR
void UGameplayTagsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property)
	{
		IGameplayTagsModule::OnTagSettingsChanged.Broadcast();
	}
}
#endif

// ---------------------------------

UGameplayTagsDeveloperSettings::UGameplayTagsDeveloperSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	
}

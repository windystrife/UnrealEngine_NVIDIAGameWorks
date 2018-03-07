// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ActorRecordingSettings.h"
#include "IMovieSceneSectionRecorderFactory.h"
#include "Features/IModularFeatures.h"

FActorRecordingSettings::FActorRecordingSettings()
{
	TArray<IMovieSceneSectionRecorderFactory*> ModularFeatures = IModularFeatures::Get().GetModularFeatureImplementations<IMovieSceneSectionRecorderFactory>("MovieSceneSectionRecorderFactory");
	for (IMovieSceneSectionRecorderFactory* Factory : ModularFeatures)
	{
		UObject* SettingsObject = Factory->CreateSettingsObject();
		if (SettingsObject)
		{
			Settings.Add(SettingsObject);
		}
	}
}

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PlatformFeatures.h"
#include "SaveGameSystem.h"
#include "DVRStreaming.h"
#include "VideoRecordingSystem.h"


ISaveGameSystem* IPlatformFeaturesModule::GetSaveGameSystem()
{
	static FGenericSaveGameSystem GenericSaveGame;
	return &GenericSaveGame;
}


IDVRStreamingSystem* IPlatformFeaturesModule::GetStreamingSystem()
{
	static FGenericDVRStreamingSystem GenericStreamingSystem;
	return &GenericStreamingSystem;
}

TSharedPtr<const class FJsonObject> IPlatformFeaturesModule::GetTitleSettings()
{
	return nullptr;
}

FString IPlatformFeaturesModule::GetUniqueAppId()
{
	return FString();
}

IVideoRecordingSystem* IPlatformFeaturesModule::GetVideoRecordingSystem()
{
	static FGenericVideoRecordingSystem GenericVideoRecordingSystem;
	return &GenericVideoRecordingSystem;
}

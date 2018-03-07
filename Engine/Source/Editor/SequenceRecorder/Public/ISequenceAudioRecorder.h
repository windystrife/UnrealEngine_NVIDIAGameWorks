// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

class USoundWave;

struct FAudioRecorderSettings
{
	/** Directory to create the asset within (empty for transient package) */
	FDirectoryPath Directory;
	FString AssetName;
	float RecordingDurationSec;
	float GainDB;
	int32 InputBufferSize;

	FAudioRecorderSettings()
		: RecordingDurationSec(-1.0f)
		, GainDB(0.0f)
		, InputBufferSize(256)
	{}
};

class ISequenceAudioRecorder
{
public:

	/** Virtual destructor */
	virtual ~ISequenceAudioRecorder() {}
	
	/** Start recording audio data */
	virtual void Start(const FAudioRecorderSettings& Settings) = 0;

	/** Stop recording audio data */
	virtual USoundWave* Stop() = 0;
};

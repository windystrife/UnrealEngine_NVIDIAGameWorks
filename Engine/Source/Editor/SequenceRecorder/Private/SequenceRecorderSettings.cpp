// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SequenceRecorderSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/LightComponent.h"
#include "SequenceRecorder.h"
#include "CineCameraComponent.h"

USequenceRecorderSettings::USequenceRecorderSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateLevelSequence = true;
	bImmersiveMode = false;
	SequenceLength = FAnimationRecordingSettings::DefaultMaximumLength;
	RecordingDelay = 4.0f;
	SequenceName = TEXT("RecordedSequence");
	AnimationSubDirectory = TEXT("Animations");
	AudioSubDirectory = TEXT("Audio");
	AudioGain = 0.0f;
	AudioInputBufferSize = 4048;
	SequenceRecordingBasePath.Path = TEXT("/Game/Cinematics/Sequences");
	bRecordNearbySpawnedActors = true;
	NearbyActorRecordingProximity = 5000.0f;
	bRecordWorldSettingsActor = true;
	bReduceKeys = true;

	ClassesAndPropertiesToRecord.Add(FPropertiesToRecordForClass(USkeletalMeshComponent::StaticClass()));
	ClassesAndPropertiesToRecord.Add(FPropertiesToRecordForClass(UStaticMeshComponent::StaticClass()));
	ClassesAndPropertiesToRecord.Add(FPropertiesToRecordForClass(UParticleSystemComponent::StaticClass()));
	ClassesAndPropertiesToRecord.Add(FPropertiesToRecordForClass(ULightComponent::StaticClass()));
	ClassesAndPropertiesToRecord.Add(FPropertiesToRecordForClass(UCameraComponent::StaticClass()));
	ClassesAndPropertiesToRecord.Add(FPropertiesToRecordForClass(UCineCameraComponent::StaticClass()));
}

void USequenceRecorderSettings::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	SaveConfig();

	if (PropertyChangedEvent.Property)
	{
		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(USequenceRecorderSettings, SequenceName) ||
			PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(USequenceRecorderSettings, SequenceRecordingBasePath))
		{
			FSequenceRecorder::Get().RefreshNextSequence();
		}
	}
}

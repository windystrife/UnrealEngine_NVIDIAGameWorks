// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Sound/SoundEffectPreset.h"
#include "Sound/SoundEffectSource.h"
#include "Engine/Engine.h"
#include "AudioDeviceManager.h"

USoundEffectPreset::USoundEffectPreset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bInitialized(false)
{
	
}

void USoundEffectPreset::EffectCommand(TFunction<void()> Command)
{
	for (int32 i = 0; i < Instances.Num(); ++i)
	{
		Instances[i]->EffectCommand(Command);
	}
}

void USoundEffectPreset::Update()
{
	for (int32 i = 0; i < Instances.Num(); ++i)
	{
		Instances[i]->SetPreset(this);
	}
}

void USoundEffectPreset::AddEffectInstance(FSoundEffectBase* InSource)
{
	if (!bInitialized)
	{
		bInitialized = true;
		Init();

		// Call the optional virtual function which subclasses can implement if they need initialization
		OnInit();
	}

	Instances.AddUnique(InSource);
}

void USoundEffectPreset::RemoveEffectInstance(FSoundEffectBase* InSource)
{
	Instances.Remove(InSource);
}

#if WITH_EDITORONLY_DATA
void USoundEffectPreset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Copy the settings to the thread safe version
	Init();
	Update();
}
#endif

#if WITH_EDITORONLY_DATA
void USoundEffectSourcePresetChain::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (GEngine)
	{
		FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
		AudioDeviceManager->UpdateSourceEffectChain(GetUniqueID(), Chain, bPlayEffectChainTails);
	}
}
#endif


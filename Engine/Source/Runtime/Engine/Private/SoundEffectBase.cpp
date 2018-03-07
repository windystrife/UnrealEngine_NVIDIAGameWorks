// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Sound/SoundEffectBase.h"


FSoundEffectBase::FSoundEffectBase()
	: bChanged(false)
	, Preset(nullptr)
	, ParentPreset(nullptr)
	, bIsRunning(false)
	, bIsActive(false)
{}

FSoundEffectBase::~FSoundEffectBase()
{
}

bool FSoundEffectBase::IsActive() const
{ 
	return bIsActive; 
}

void FSoundEffectBase::SetEnabled(const bool bInIsEnabled)
{ 
	bIsActive = bInIsEnabled; 
}

void FSoundEffectBase::SetPreset(USoundEffectPreset* Inpreset)
{
	Preset = Inpreset;
	bChanged = true;
}

void FSoundEffectBase::Update()
{
	PumpPendingMessages();

	if (bChanged && Preset)
	{
		OnPresetChanged();
		bChanged = false;
	}
}

void FSoundEffectBase::RegisterWithPreset(USoundEffectPreset* InParentPreset)
{
	ParentPreset = InParentPreset;
	ParentPreset->AddEffectInstance(this);
}

void FSoundEffectBase::UnregisterWithPreset()
{
	ParentPreset->RemoveEffectInstance(this);
}

bool FSoundEffectBase::IsParentPreset(USoundEffectPreset* InPreset) const
{
	return ParentPreset == InPreset;
}

void FSoundEffectBase::EffectCommand(TFunction<void()> Command)
{
	CommandQueue.Enqueue(MoveTemp(Command));
}
void FSoundEffectBase::PumpPendingMessages()
{
	// Pumps the commadn queue
	TFunction<void()> Command;
	while (CommandQueue.Dequeue(Command))
	{
		Command();
	}
}

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Sound/ISlateSoundDevice.h"

struct FSlateSound;

class FSlateSoundDevice : public ISlateSoundDevice
{
public:
	virtual ~FSlateSoundDevice(){}

private:
	virtual void PlaySound(const FSlateSound& Sound, int32 UserIndex = 0) const override;	
	virtual float GetSoundDuration(const FSlateSound& Sound) const override;
};

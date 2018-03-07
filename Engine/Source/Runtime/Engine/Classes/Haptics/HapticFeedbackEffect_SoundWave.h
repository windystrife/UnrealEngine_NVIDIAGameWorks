// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Haptics/HapticFeedbackEffect_Base.h"
#include "GenericPlatform/IInputInterface.h"
#include "HapticFeedbackEffect_SoundWave.generated.h"

class USoundWave;

UCLASS(MinimalAPI, BlueprintType)
class UHapticFeedbackEffect_SoundWave : public UHapticFeedbackEffect_Base
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = "HapticFeedbackEffect_SoundWave")
	USoundWave *SoundWave;

	~UHapticFeedbackEffect_SoundWave();

	void Initialize() override;

	void GetValues(const float EvalTime, FHapticFeedbackValues& Values) override;

	float GetDuration() const override;

private:
	void PrepareSoundWaveBuffer();
	bool bPrepared;

	FHapticFeedbackBuffer HapticBuffer;
};

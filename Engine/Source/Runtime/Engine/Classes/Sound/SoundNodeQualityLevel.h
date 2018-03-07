// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

 
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Sound/SoundNode.h"
#include "SoundNodeQualityLevel.generated.h"

class FAudioDevice;
struct FActiveSound;
struct FSoundParseParameters;
struct FWaveInstance;

/**
 * This SoundNode uses GameUserSettings AudioQualityLevel (or the editor override) to choose which branch to play
 * and at runtime will only load in to memory sound waves connected to the branch that will be selected
 */
UCLASS(hidecategories=Object, editinlinenew, MinimalAPI, meta=( DisplayName="Quality Level" ))
class USoundNodeQualityLevel : public USoundNode
{
	GENERATED_BODY()

public:

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface
#endif

	//~ Begin USoundNode Interface.
	virtual void ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances ) override;
	virtual int32 GetMaxChildNodes() const override;
	virtual int32 GetMinChildNodes() const override;
#if WITH_EDITOR
	virtual FText GetInputPinName(int32 PinIndex) const override;
#endif
	//~ End USoundNode Interface.

#if WITH_EDITOR
	void ReconcileNode(bool bReconstructNode);
#endif
};


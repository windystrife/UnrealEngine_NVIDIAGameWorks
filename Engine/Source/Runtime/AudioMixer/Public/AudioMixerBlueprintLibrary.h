// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Classes/SubmixEffects/AudioMixerSubmixEffectDynamicsProcessor.h"
#include "Sound/SoundEffectSource.h"
#include "AudioMixerBlueprintLibrary.generated.h"

UCLASS()
class AUDIOMIXER_API UAudioMixerBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Adds a submix effect preset to the master submix. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Effects", meta=(WorldContext="WorldContextObject"))
	static void AddMasterSubmixEffect(const UObject* WorldContextObject, USoundEffectSubmixPreset* SubmixEffectPreset);

	/** Removes a submix effect preset from the master submix. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Effects", meta=(WorldContext="WorldContextObject"))
	static void RemoveMasterSubmixEffect(const UObject* WorldContextObject, USoundEffectSubmixPreset* SubmixEffectPreset);

	/** Clears all master submix effects. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Effects", meta = (WorldContext = "WorldContextObject"))
	static void ClearMasterSubmixEffects(const UObject* WorldContextObject);

	/** Adds source effect entry to preset chain. Only effects the instance of the preset chain */
	UFUNCTION(BlueprintCallable, Category = "Audio|Effects", meta = (WorldContext = "WorldContextObject"))
	static void AddSourceEffectToPresetChain(const UObject* WorldContextObject, USoundEffectSourcePresetChain* PresetChain, FSourceEffectChainEntry Entry);

	/** Adds source effect entry to preset chain. Only affects the instance of preset chain. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Effects", meta = (WorldContext = "WorldContextObject"))
	static void RemoveSourceEffectFromPresetChain(const UObject* WorldContextObject, USoundEffectSourcePresetChain* PresetChain, int32 EntryIndex);

	/** Set whether or not to bypass the effect at the source effect chain index. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Effects", meta = (WorldContext = "WorldContextObject"))
	static void SetBypassSourceEffectChainEntry(const UObject* WorldContextObject, USoundEffectSourcePresetChain* PresetChain, int32 EntryIndex, bool bBypassed);

	/** Returns the number of effect chain entries in the given source effect chain. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Effects", meta = (WorldContext = "WorldContextObject"))
	static int32 GetNumberOfEntriesInSourceEffectChain(const UObject* WorldContextObject, USoundEffectSourcePresetChain* PresetChain);


};

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "SoundMix.generated.h"

class USoundClass;
struct FPropertyChangedEvent;

USTRUCT()
struct FAudioEQEffect
{
	GENERATED_USTRUCT_BODY()

	/* Start time of effect */
	double RootTime;

	/** Center frequency in Hz for band 0 */
	UPROPERTY(EditAnywhere, Category = Band0, meta = (ClampMin = "0.0", ClampMax = "20000.0", UIMin = "0.0", UIMax = "20000.0"))
	float FrequencyCenter0;
	
	/** Boost/cut of band 0 */
	UPROPERTY(EditAnywhere, Category = Band0, meta = (ClampMin = "0.0", ClampMax = "10.0", UIMin = "0.0", UIMax = "10.0"))
	float Gain0;

	/** Bandwidth of band 0. Region is center frequency +/- Bandwidth /2 */
	UPROPERTY(EditAnywhere, Category = Band0, meta = (ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "2.0"))
	float Bandwidth0;

	/** Center frequency in Hz for band 1 */
	UPROPERTY(EditAnywhere, Category = Band0, meta = (ClampMin = "0.0", ClampMax = "20000.0", UIMin = "0.0", UIMax = "20000.0"))
	float FrequencyCenter1;

	/** Boost/cut of band 1 */
	UPROPERTY(EditAnywhere, Category = Band0, meta = (ClampMin = "0.0", ClampMax = "10.0", UIMin = "0.0", UIMax = "10.0"))
	float Gain1;

	/** Bandwidth of band 1. Region is center frequency +/- Bandwidth /2 */
	UPROPERTY(EditAnywhere, Category = Band0, meta = (ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "2.0"))
	float Bandwidth1;

	/** Center frequency in Hz for band 2 */
	UPROPERTY(EditAnywhere, Category = Band0, meta = (ClampMin = "0.0", ClampMax = "20000.0", UIMin = "0.0", UIMax = "20000.0"))
	float FrequencyCenter2;

	/** Boost/cut of band 2 */
	UPROPERTY(EditAnywhere, Category = Band0, meta = (ClampMin = "0.0", ClampMax = "10.0", UIMin = "0.0", UIMax = "10.0"))
	float Gain2;

	/** Bandwidth of band 2. Region is center frequency +/- Bandwidth /2 */
	UPROPERTY(EditAnywhere, Category = Band0, meta = (ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "2.0"))
	float Bandwidth2;

	/** Center frequency in Hz for band 3 */
	UPROPERTY(EditAnywhere, Category = Band0, meta = (ClampMin = "0.0", ClampMax = "20000.0", UIMin = "0.0", UIMax = "20000.0"))
	float FrequencyCenter3;

	/** Boost/cut of band 3 */
	UPROPERTY(EditAnywhere, Category = Band0, meta = (ClampMin = "0.0", ClampMax = "10.0", UIMin = "0.0", UIMax = "10.0"))
	float Gain3;

	/** Bandwidth of band 3. Region is center frequency +/- Bandwidth /2 */
	UPROPERTY(EditAnywhere, Category = Band0, meta = (ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "2.0"))
	float Bandwidth3;

	FAudioEQEffect()
		: RootTime(0.0f)
		, FrequencyCenter0(600.0f)
		, Gain0(1.0f)
		, Bandwidth0(1.0f)
		, FrequencyCenter1(1000.0f)
		, Gain1(1.0f)
		, Bandwidth1(1.0f)
		, FrequencyCenter2(2000.0f)
		, Gain2(1.0f)
		, Bandwidth2(1.0f)
		, FrequencyCenter3(10000.0f)
		, Gain3(1.0f)
		, Bandwidth3(1.0f)
	{}

	/** 
	* Interpolate EQ settings based on time
	*/
	void Interpolate( float InterpValue, const FAudioEQEffect& Start, const FAudioEQEffect& End );
		
	/** 
	* Clamp all settings in range
	*/
	void ClampValues();

};

/**
 * Elements of data for sound group volume control
 */
USTRUCT()
struct FSoundClassAdjuster
{
	GENERATED_USTRUCT_BODY()

	/* The sound class this adjuster affects. */
	UPROPERTY(EditAnywhere, Category=SoundClassAdjuster, DisplayName = "Sound Class" )
	USoundClass* SoundClassObject;

	/* A multiplier applied to the volume. */
	UPROPERTY(EditAnywhere, Category=SoundClassAdjuster )
	float VolumeAdjuster;

	/* A multiplier applied to the pitch. */
	UPROPERTY(EditAnywhere, Category=SoundClassAdjuster )
	float PitchAdjuster;

	/* Set to true to apply this adjuster to all children of the sound class. */
	UPROPERTY(EditAnywhere, Category=SoundClassAdjuster )
	uint32 bApplyToChildren:1;

	/* A multiplier applied to VoiceCenterChannelVolume. */
	UPROPERTY(EditAnywhere, Category=SoundClassAdjuster )
	float VoiceCenterChannelVolumeAdjuster;

	FSoundClassAdjuster()
		: SoundClassObject(NULL)
		, VolumeAdjuster(1)
		, PitchAdjuster(1)
		, bApplyToChildren(false)
		, VoiceCenterChannelVolumeAdjuster(1)
		{
		}
	
};

UCLASS(BlueprintType, hidecategories=object, MinimalAPI)
class USoundMix : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Whether to apply the EQ effect */
	UPROPERTY(EditAnywhere, Category=EQ, AssetRegistrySearchable )
	uint32 bApplyEQ:1;

	UPROPERTY(EditAnywhere, Category=EQ)
	float EQPriority;

	UPROPERTY(EditAnywhere, Category=EQ)
	struct FAudioEQEffect EQSettings;

	/* Array of changes to be applied to groups. */
	UPROPERTY(EditAnywhere, Category=SoundClasses)
	TArray<struct FSoundClassAdjuster> SoundClassEffects;

	/* Initial delay in seconds before the the mix is applied. */
	UPROPERTY(EditAnywhere, Category=SoundMix )
	float InitialDelay;

	/* Time taken in seconds for the mix to fade in. */
	UPROPERTY(EditAnywhere, Category=SoundMix )
	float FadeInTime;

	/* Duration of mix, negative means it will be applied until another mix is set. */
	UPROPERTY(EditAnywhere, Category=SoundMix )
	float Duration;

	/* Time taken in seconds for the mix to fade out. */
	UPROPERTY(EditAnywhere, Category=SoundMix )
	float FadeOutTime;

#if WITH_EDITORONLY_DATA
	/** Transient property used to trigger real-time updates of the active EQ filter for editor previewing */
	UPROPERTY(transient)
	uint32 bChanged:1;
#endif

#if WITH_EDITOR
	bool CausesPassiveDependencyLoop(TArray<USoundClass*>& ProblemClasses) const;
#endif // WITH_EDITOR

protected:
	//~ Begin UObject Interface.
	virtual FString GetDesc( void ) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void BeginDestroy() override;
	//~ End UObject Interface.

#if WITH_EDITOR
	bool CheckForDependencyLoop(USoundClass* SoundClass, TArray<USoundClass*>& ProblemClasses, bool CheckChildren) const;
#endif // WITH_EDITOR
};




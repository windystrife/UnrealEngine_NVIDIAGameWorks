// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *
 * Used to affect audio settings in the game and editor.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "GameFramework/Volume.h"
#include "Audio.h"
#include "AudioVolume.generated.h"

class AAudioVolume;
struct FBodyInstance;

/**
 * DEPRECATED: Exists for backwards compatibility
 * Indicates a reverb preset to use.
 *
 */
UENUM()
enum ReverbPreset
{
	REVERB_Default,
	REVERB_Bathroom,
	REVERB_StoneRoom,
	REVERB_Auditorium,
	REVERB_ConcertHall,
	REVERB_Cave,
	REVERB_Hallway,
	REVERB_StoneCorridor,
	REVERB_Alley,
	REVERB_Forest,
	REVERB_City,
	REVERB_Mountains,
	REVERB_Quarry,
	REVERB_Plain,
	REVERB_ParkingLot,
	REVERB_SewerPipe,
	REVERB_Underwater,
	REVERB_SmallRoom,
	REVERB_MediumRoom,
	REVERB_LargeRoom,
	REVERB_MediumHall,
	REVERB_LargeHall,
	REVERB_Plate,
	REVERB_MAX,
};

/** Struct encapsulating settings for reverb effects. */
USTRUCT(BlueprintType)
struct FReverbSettings
{
	GENERATED_USTRUCT_BODY()

	/* Whether to apply the reverb settings below. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ReverbSettings )
	uint32 bApplyReverb:1;

	/** The reverb preset to employ. */
	UPROPERTY()
	TEnumAsByte<enum ReverbPreset> ReverbType_DEPRECATED;

	/** The reverb asset to employ. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ReverbSettings)
	class UReverbEffect* ReverbEffect;

	/** Volume level of the reverb affect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ReverbSettings)
	float Volume;

	/** Time to fade from the current reverb settings into this setting, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ReverbSettings)
	float FadeTime;

	FReverbSettings()
		: bApplyReverb(true)
		, ReverbType_DEPRECATED(REVERB_Default)
		, ReverbEffect(nullptr)
		, Volume(0.5f)
		, FadeTime(2.0f)
	{
	}

	bool operator==(const FReverbSettings& Other) const;
	bool operator!=(const FReverbSettings& Other) const { return !(*this == Other); }

	void PostSerialize(const FArchive& Ar);
};

template<>
struct TStructOpsTypeTraits<FReverbSettings> : public TStructOpsTypeTraitsBase2<FReverbSettings>
{
	enum 
	{
		WithPostSerialize = true,
	};
};

/** Struct encapsulating settings for interior areas. */
USTRUCT(BlueprintType)
struct FInteriorSettings
{
	GENERATED_USTRUCT_BODY()

	// Whether these interior settings are the default values for the world
	UPROPERTY()
	uint32 bIsWorldSettings:1;

	// The desired volume of sounds outside the volume when the player is inside the volume
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InteriorSettings)
	float ExteriorVolume;

	// The time over which to interpolate from the current volume to the desired volume of sounds outside the volume when the player enters the volume
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InteriorSettings)
	float ExteriorTime;

	// The desired LPF frequency cutoff in hertz of sounds outside the volume when the player is inside the volume
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteriorSettings)
	float ExteriorLPF;

	// The time over which to interpolate from the current LPF to the desired LPF of sounds outside the volume when the player enters the volume
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InteriorSettings)
	float ExteriorLPFTime;

	// The desired volume of sounds inside the volume when the player is outside the volume
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InteriorSettings)
	float InteriorVolume;

	// The time over which to interpolate from the current volume to the desired volume of sounds inside the volume when the player enters the volume
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InteriorSettings)
	float InteriorTime;

	// The desired LPF frequency cutoff in hertz of sounds inside  the volume when the player is outside the volume
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteriorSettings)
	float InteriorLPF;

	// The time over which to interpolate from the current LPF to the desired LPF of sounds inside the volume when the player enters the volume
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InteriorSettings)
	float InteriorLPFTime;

	FInteriorSettings();

	bool operator==(const FInteriorSettings& Other) const;
	bool operator!=(const FInteriorSettings& Other) const;

	void PostSerialize(const FArchive& Ar);
};

template<>
struct TStructOpsTypeTraits<FInteriorSettings> : public TStructOpsTypeTraitsBase2<FInteriorSettings>
{
	enum
	{
		WithPostSerialize = true,
	};
};

struct FAudioVolumeProxy
{
	FAudioVolumeProxy()
		: AudioVolumeID(0)
		, WorldID(0)
		, Priority(0.f)
		, BodyInstance(nullptr)
	{
	}

	FAudioVolumeProxy(const AAudioVolume* AudioVolume);

	uint32 AudioVolumeID;
	uint32 WorldID;
	float Priority;
	FReverbSettings ReverbSettings;
	FInteriorSettings InteriorSettings;
	FBodyInstance* BodyInstance; // This is scary
};

UCLASS(hidecategories=(Advanced, Attachment, Collision, Volume))
class ENGINE_API AAudioVolume : public AVolume
{
	GENERATED_UCLASS_BODY()

private:
	/**
	 * Priority of this volume. In the case of overlapping volumes the one with the highest priority
	 * is chosen. The order is undefined if two or more overlapping volumes have the same priority.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AudioVolume, meta=(AllowPrivateAccess="true"))
	float Priority;

	/** whether this volume is currently enabled and able to affect sounds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_bEnabled, Category=AudioVolume, meta=(AllowPrivateAccess="true"))
	uint32 bEnabled:1;

	/** Reverb settings to use for this volume. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Reverb, meta=(AllowPrivateAccess="true"))
	FReverbSettings Settings;

	/** Interior settings used for this volume */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AmbientZone, meta=(AllowPrivateAccess="true"))
	FInteriorSettings AmbientZoneSettings;

public:

	float GetPriority() const { return Priority; }
	
	UFUNCTION(BlueprintCallable, Category=AudioVolume)
	void SetPriority(float NewPriority);

	bool GetEnabled() const { return bEnabled; }
	
	UFUNCTION(BlueprintCallable, Category=AudioVolume)
	void SetEnabled(bool bNewEnabled);

	const FReverbSettings& GetReverbSettings() const { return Settings; }
	
	UFUNCTION(BlueprintCallable, Category=AudioVolume)
	void SetReverbSettings(const FReverbSettings& NewReverbSettings);

	const FInteriorSettings& GetInteriorSettings() const { return AmbientZoneSettings; }

	UFUNCTION(BlueprintCallable, Category=AudioVolume)
	void SetInteriorSettings(const FInteriorSettings& NewInteriorSettings);

private:

	UFUNCTION()
	virtual void OnRep_bEnabled();

	void TransformUpdated(USceneComponent* RootComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport);

	void AddProxy() const;
	void RemoveProxy() const;
	void UpdateProxy() const;

public:

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UObject Interface

	//~ Begin AActor Interface
	virtual void PostUnregisterAllComponents() override;
	virtual void PostRegisterAllComponents() override;
	//~ End AActor Interface
};




// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AudioVolume.cpp: Used to affect audio settings in the game and editor.
=============================================================================*/

#include "Sound/AudioVolume.h"
#include "Engine/CollisionProfile.h"
#include "AudioThread.h"
#include "Sound/ReverbEffect.h"
#include "AudioDevice.h"
#include "Components/BrushComponent.h"
#include "Net/UnrealNetwork.h"

FInteriorSettings::FInteriorSettings()
	: bIsWorldSettings(false)
	, ExteriorVolume(1.0f)
	, ExteriorTime(0.5f)
	, ExteriorLPF(MAX_FILTER_FREQUENCY)
	, ExteriorLPFTime(0.5f)
	, InteriorVolume(1.0f)
	, InteriorTime(0.5f)
	, InteriorLPF(MAX_FILTER_FREQUENCY)
	, InteriorLPFTime(0.5f)
{
}


void FInteriorSettings::PostSerialize(const FArchive& Ar)
{
	if (Ar.UE4Ver() < VER_UE4_USE_LOW_PASS_FILTER_FREQ)
	{
		if (InteriorLPF > 0.0f && InteriorLPF < 1.0f)
		{
			float FilterConstant = 2.0f * FMath::Sin(PI * 6000.0f * InteriorLPF / 48000);
			InteriorLPF = FilterConstant * MAX_FILTER_FREQUENCY;
		}

		if (ExteriorLPF > 0.0f && ExteriorLPF < 1.0f)
		{
			float FilterConstant = 2.0f * FMath::Sin(PI * 6000.0f * ExteriorLPF / 48000);
			ExteriorLPF = FilterConstant * MAX_FILTER_FREQUENCY;
		}
	}
}


bool FReverbSettings::operator==(const FReverbSettings& Other) const
{
	return (bApplyReverb == Other.bApplyReverb
			&& ReverbEffect == Other.ReverbEffect
			&& Volume == Other.Volume
			&& FadeTime == Other.FadeTime);
}

void FReverbSettings::PostSerialize(const FArchive& Ar)
{
	if( Ar.UE4Ver() < VER_UE4_REVERB_EFFECT_ASSET_TYPE )
	{
		FString ReverbAssetName;
		switch(ReverbType_DEPRECATED)
		{
			case REVERB_Default:
				// No replacement asset for default reverb type
				return;

			case REVERB_Bathroom:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Bathroom.Bathroom");
				break;

			case REVERB_StoneRoom:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/StoneRoom.StoneRoom");
				break;

			case REVERB_Auditorium:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Auditorium.Auditorium");
				break;

			case REVERB_ConcertHall:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/ConcertHall.ConcertHall");
				break;

			case REVERB_Cave:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Cave.Cave");
				break;

			case REVERB_Hallway:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Hallway.Hallway");
				break;

			case REVERB_StoneCorridor:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/StoneCorridor.StoneCorridor");
				break;

			case REVERB_Alley:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Alley.Alley");
				break;

			case REVERB_Forest:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Forest.Forest");
				break;

			case REVERB_City:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/City.City");
				break;

			case REVERB_Mountains:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Mountains.Mountains");
				break;

			case REVERB_Quarry:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Quarry.Quarry");
				break;

			case REVERB_Plain:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Plain.Plain");
				break;

			case REVERB_ParkingLot:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/ParkingLot.ParkingLot");
				break;

			case REVERB_SewerPipe:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/SewerPipe.SewerPipe");
				break;

			case REVERB_Underwater:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Underwater.Underwater");
				break;

			case REVERB_SmallRoom:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/SmallRoom.SmallRoom");
				break;

			case REVERB_MediumRoom:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/MediumRoom.MediumRoom");
				break;

			case REVERB_LargeRoom:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/LargeRoom.LargeRoom");
				break;

			case REVERB_MediumHall:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/MediumHall.MediumHall");
				break;

			case REVERB_LargeHall:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/LargeHall.LargeHall");
				break;

			case REVERB_Plate:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Plate.Plate");
				break;

			default:
				// This should cover every type of reverb preset
				checkNoEntry();
				break;
		}

		ReverbEffect = LoadObject<UReverbEffect>(NULL, *ReverbAssetName);
		check( ReverbEffect );
	}
}

bool FInteriorSettings::operator==(const FInteriorSettings& Other) const
{
	return (Other.bIsWorldSettings == bIsWorldSettings)
		&& (Other.ExteriorVolume == ExteriorVolume) && (Other.ExteriorTime == ExteriorTime)
		&& (Other.ExteriorLPF == ExteriorLPF) && (Other.ExteriorLPFTime == ExteriorLPFTime)
		&& (Other.InteriorVolume == InteriorVolume) && (Other.InteriorTime == InteriorTime)
		&& (Other.InteriorLPF == InteriorLPF) && (Other.InteriorLPFTime == InteriorLPFTime);
}

bool FInteriorSettings::operator!=(const FInteriorSettings& Other) const
{
	return !(*this == Other);
}

AAudioVolume::AAudioVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetBrushComponent()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	GetBrushComponent()->bAlwaysCreatePhysicsState = true;

	bColored = true;
	BrushColor = FColor(255, 255, 0, 255);

	bEnabled = true;
}

void AAudioVolume::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAudioVolume, bEnabled);
}

FAudioVolumeProxy::FAudioVolumeProxy(const AAudioVolume* AudioVolume)
	: AudioVolumeID(AudioVolume->GetUniqueID())
	, WorldID(AudioVolume->GetWorld()->GetUniqueID())
	, Priority(AudioVolume->GetPriority())
	, ReverbSettings(AudioVolume->GetReverbSettings())
	, InteriorSettings(AudioVolume->GetInteriorSettings())
	, BodyInstance(AudioVolume->GetBrushComponent()->GetBodyInstance())
{
}

void AAudioVolume::AddProxy() const
{
	UWorld* World = GetWorld();

	if (FAudioDevice* AudioDevice = World->GetAudioDevice())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.AddAudioVolumeProxy"), STAT_AudioAddAudioVolumeProxy, STATGROUP_TaskGraphTasks);

		FAudioVolumeProxy Proxy(this);

		FAudioThread::RunCommandOnAudioThread([AudioDevice, Proxy]()
		{
			AudioDevice->AddAudioVolumeProxy(Proxy);
		}, GET_STATID(STAT_AudioAddAudioVolumeProxy));
	}
}

void FAudioDevice::AddAudioVolumeProxy(const FAudioVolumeProxy& Proxy)
{
	check(IsInAudioThread());

	AudioVolumeProxies.Add(Proxy.AudioVolumeID, Proxy);
	AudioVolumeProxies.ValueSort([](const FAudioVolumeProxy& A, const FAudioVolumeProxy& B) { return A.Priority > B.Priority; });

	InvalidateCachedInteriorVolumes();
}


void AAudioVolume::RemoveProxy() const
{
	// World will be NULL during exit purge.
	UWorld* World = GetWorld();
	if (World)
	{
		if (FAudioDevice* AudioDevice = World->GetAudioDevice())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.RemoveAudioVolumeProxy"), STAT_AudioRemoveAudioVolumeProxy, STATGROUP_TaskGraphTasks);

			const uint32 AudioVolumeID = GetUniqueID();
			FAudioThread::RunCommandOnAudioThread([AudioDevice, AudioVolumeID]()
			{
				AudioDevice->RemoveAudioVolumeProxy(AudioVolumeID);
			}, GET_STATID(STAT_AudioRemoveAudioVolumeProxy));
		}
	}
}

void FAudioDevice::RemoveAudioVolumeProxy(const uint32 AudioVolumeID)
{
	check(IsInAudioThread());

	AudioVolumeProxies.Remove(AudioVolumeID);

	InvalidateCachedInteriorVolumes();
}

void AAudioVolume::UpdateProxy() const
{
	UWorld* World = GetWorld();

	if (FAudioDevice* AudioDevice = World->GetAudioDevice())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.UpdateAudioVolumeProxy"), STAT_AudioUpdateAudioVolumeProxy, STATGROUP_TaskGraphTasks);

		FAudioVolumeProxy Proxy(this);

		FAudioThread::RunCommandOnAudioThread([AudioDevice, Proxy]()
		{
			AudioDevice->UpdateAudioVolumeProxy(Proxy);
		}, GET_STATID(STAT_AudioUpdateAudioVolumeProxy));
	}
}

void FAudioDevice::UpdateAudioVolumeProxy(const FAudioVolumeProxy& NewProxy)
{
	check(IsInAudioThread());

	if (FAudioVolumeProxy* CurrentProxy = AudioVolumeProxies.Find(NewProxy.AudioVolumeID))
	{
		const float CurrentPriority = CurrentProxy->Priority;

		*CurrentProxy = NewProxy;

		if (CurrentPriority != NewProxy.Priority)
		{
			AudioVolumeProxies.ValueSort([](const FAudioVolumeProxy& A, const FAudioVolumeProxy& B) { return A.Priority > B.Priority; });
		}
	}
}

void AAudioVolume::PostUnregisterAllComponents()
{
	// Route clear to super first.
	Super::PostUnregisterAllComponents();

	// Component can be nulled due to GC at this point
	if (GetRootComponent())
	{
		GetRootComponent()->TransformUpdated.RemoveAll(this);
	}
	RemoveProxy();

	if (UWorld* World = GetWorld())
	{
		World->AudioVolumes.Remove(this);
	}
}

void AAudioVolume::PostRegisterAllComponents()
{
	// Route update to super first.
	Super::PostRegisterAllComponents();

	GetRootComponent()->TransformUpdated.AddUObject(this, &AAudioVolume::TransformUpdated);
	AddProxy();

	UWorld* World = GetWorld();
	World->AudioVolumes.Add(this);
	World->AudioVolumes.Sort([](const AAudioVolume& A, const AAudioVolume& B) { return (A.GetPriority() > B.GetPriority()); });
}

void AAudioVolume::TransformUpdated(USceneComponent* InRootComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	UpdateProxy();
}

void AAudioVolume::SetEnabled(const bool bNewEnabled)
{
	if (bNewEnabled != bEnabled)
	{
		bEnabled = bNewEnabled;
		if (bEnabled)
		{
			AddProxy();
		}
		else
		{
			RemoveProxy();
		}
	}
}

void AAudioVolume::OnRep_bEnabled()
{
	if (bEnabled)
	{
		AddProxy();
	}
	else
	{
		RemoveProxy();
	}
}

void AAudioVolume::SetPriority(const float NewPriority)
{
	if (NewPriority != Priority)
	{
		Priority = NewPriority;
		GetWorld()->AudioVolumes.Sort([](const AAudioVolume& A, const AAudioVolume& B) { return (A.GetPriority() > B.GetPriority()); });
		if (bEnabled)
		{
			UpdateProxy();
		}
	}
}

void AAudioVolume::SetInteriorSettings(const FInteriorSettings& NewInteriorSettings)
{
	if (NewInteriorSettings != AmbientZoneSettings)
	{
		AmbientZoneSettings = NewInteriorSettings;
		if (bEnabled)
		{
			UpdateProxy();
		}
	}
}

void AAudioVolume::SetReverbSettings(const FReverbSettings& NewReverbSettings)
{
	if (NewReverbSettings != Settings)
	{
		Settings = NewReverbSettings;
		if (bEnabled)
		{
			UpdateProxy();
		}
	}
}


#if WITH_EDITOR
void AAudioVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	Settings.Volume = FMath::Clamp<float>( Settings.Volume, 0.0f, 1.0f );
	AmbientZoneSettings.InteriorTime = FMath::Max<float>( 0.01f, AmbientZoneSettings.InteriorTime );
	AmbientZoneSettings.InteriorLPFTime = FMath::Max<float>( 0.01f, AmbientZoneSettings.InteriorLPFTime );
	AmbientZoneSettings.ExteriorTime = FMath::Max<float>( 0.01f, AmbientZoneSettings.ExteriorTime );
	AmbientZoneSettings.ExteriorLPFTime = FMath::Max<float>( 0.01f, AmbientZoneSettings.ExteriorLPFTime );

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AAudioVolume, Priority))
	{
		GetWorld()->AudioVolumes.Sort([](const AAudioVolume& A, const AAudioVolume& B) { return (A.GetPriority() > B.GetPriority()); });
	}

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AAudioVolume, bEnabled))
	{
		if (bEnabled)
		{
			AddProxy();
		}
		else
		{
			RemoveProxy();
		}
	}
	else if (bEnabled)
	{
		UpdateProxy();
	}

}
#endif // WITH_EDITOR

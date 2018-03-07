// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

/////////////////////////////////////////////////////
// UAnimNotify_PlaySound

UAnimNotify_PlaySound::UAnimNotify_PlaySound()
	: Super()
{
	VolumeMultiplier = 1.f;
	PitchMultiplier = 1.f;

#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(196, 142, 255, 255);
#endif // WITH_EDITORONLY_DATA
}

void UAnimNotify_PlaySound::Notify(class USkeletalMeshComponent* MeshComp, class UAnimSequenceBase* Animation)
{
	// Don't call super to avoid call back in to blueprints
	if (Sound)
	{
		if (bFollow)
		{
			UGameplayStatics::SpawnSoundAttached(Sound, MeshComp, AttachName, FVector(ForceInit), EAttachLocation::KeepRelativeOffset, false, VolumeMultiplier, PitchMultiplier);
		}
		else
		{
			UGameplayStatics::PlaySoundAtLocation(MeshComp->GetWorld(), Sound, MeshComp->GetComponentLocation(), VolumeMultiplier, PitchMultiplier);
		}
	}
}

FString UAnimNotify_PlaySound::GetNotifyName_Implementation() const
{
	if (Sound)
	{
		return Sound->GetName();
	}
	else
	{
		return Super::GetNotifyName_Implementation();
	}
}

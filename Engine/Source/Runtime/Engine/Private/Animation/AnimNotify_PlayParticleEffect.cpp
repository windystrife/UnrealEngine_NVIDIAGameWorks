// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifies/AnimNotify_PlayParticleEffect.h"
#include "Particles/ParticleSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "ParticleHelper.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimSequenceBase.h"

/////////////////////////////////////////////////////
// UAnimNotify_PlayParticleEffect

UAnimNotify_PlayParticleEffect::UAnimNotify_PlayParticleEffect()
	: Super()
{
	Attached = true;
	Scale = FVector(1.f);

#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(192, 255, 99, 255);
#endif // WITH_EDITORONLY_DATA
}

void UAnimNotify_PlayParticleEffect::PostLoad()
{
	Super::PostLoad();

	RotationOffsetQuat = FQuat(RotationOffset);
}

#if WITH_EDITOR
void UAnimNotify_PlayParticleEffect::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimNotify_PlayParticleEffect, RotationOffset))
	{
		RotationOffsetQuat = FQuat(RotationOffset);
	}
}
#endif

void UAnimNotify_PlayParticleEffect::Notify(class USkeletalMeshComponent* MeshComp, class UAnimSequenceBase* Animation)
{
	// Don't call super to avoid unnecessary call in to blueprints
	if (PSTemplate)
	{
		if (PSTemplate->IsImmortal())
		{
			UE_LOG(LogParticles, Warning, TEXT("Particle Notify: Anim '%s' tried to spawn infinitely looping particle system '%s'. Spawning suppressed."), *GetNameSafe(Animation), *GetNameSafe(PSTemplate));
			return;
		}

		if (Attached)
		{
			UGameplayStatics::SpawnEmitterAttached(PSTemplate, MeshComp, SocketName, LocationOffset, RotationOffset, Scale);
		}
		else
		{
			const FTransform MeshTransform = MeshComp->GetSocketTransform(SocketName);
			FTransform SpawnTransform;
			SpawnTransform.SetLocation(MeshTransform.TransformPosition(LocationOffset));
			SpawnTransform.SetRotation(MeshTransform.GetRotation() * RotationOffsetQuat);
			SpawnTransform.SetScale3D(Scale);
			UGameplayStatics::SpawnEmitterAtLocation(MeshComp->GetWorld(), PSTemplate, SpawnTransform);
		}
	}
	else
	{
		UE_LOG(LogParticles, Warning, TEXT("Particle Notify: Particle system is null for particle notify '%s' in anim: '%s'"), *GetNotifyName(), *GetPathNameSafe(Animation));
	}
}

FString UAnimNotify_PlayParticleEffect::GetNotifyName_Implementation() const
{
	if (PSTemplate)
	{
		return PSTemplate->GetName();
	}
	else
	{
		return Super::GetNotifyName_Implementation();
	}
}

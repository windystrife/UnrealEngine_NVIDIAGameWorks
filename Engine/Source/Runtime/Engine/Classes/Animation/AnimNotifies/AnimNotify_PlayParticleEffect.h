// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_PlayParticleEffect.generated.h"

class UAnimSequenceBase;
class UParticleSystem;
class USkeletalMeshComponent;

UCLASS(const, hidecategories=Object, collapsecategories, meta=(DisplayName="Play Particle Effect"))
class ENGINE_API UAnimNotify_PlayParticleEffect : public UAnimNotify
{
	GENERATED_BODY()

public:

	UAnimNotify_PlayParticleEffect();

	// Begin UObject interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// End UObject interface

	// Begin UAnimNotify interface
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
	// End UAnimNotify interface

	// Particle System to Spawn
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AnimNotify", meta=(DisplayName="Particle System"))
	UParticleSystem* PSTemplate;

	// Location offset from the socket
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AnimNotify")
	FVector LocationOffset;

	// Rotation offset from socket
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AnimNotify")
	FRotator RotationOffset;

	// Scale to spawn the particle system at
	UPROPERTY(EditAnywhere, Category="AnimNotify")
	FVector Scale;

private:
	// Cached version of the Rotation Offset already in Quat form
	FQuat RotationOffsetQuat;

public:

	// Should attach to the bone/socket
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AnimNotify")
	uint32 Attached:1; 	//~ Does not follow coding standard due to redirection from BP

	// SocketName to attach to
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	FName SocketName;
};




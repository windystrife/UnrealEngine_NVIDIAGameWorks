// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelCapture.h"
#include "UObject/LazyObjectPtr.h"
#include "Engine/World.h"

ULevelCapture::ULevelCapture(const FObjectInitializer& Init)
	: Super(Init)
{
	bAutoStartCapture = true;
}

void ULevelCapture::SetPrerequisiteActor(AActor* InPrerequisiteActor)
{
	PrerequisiteActorId = TLazyObjectPtr<>(InPrerequisiteActor).GetUniqueID().GetGuid();
}

void ULevelCapture::Initialize(TSharedPtr<FSceneViewport> InViewport, int32 PIEInstance)
{
	CaptureStrategy = MakeShareable(new FFixedTimeStepCaptureStrategy(Settings.FrameRate));
	Super::Initialize(InViewport, PIEInstance);

	PIECaptureInstance = PIEInstance;

	if (bAutoStartCapture)
	{
		StartCapture();
	}
}

void ULevelCapture::Tick(float DeltaSeconds)
{
	AActor* Actor = PrerequisiteActor.Get();
	if (!Actor)
	{
		// We need to look the actor up
		TLazyObjectPtr<AActor> LazyActor;
		if (PIECaptureInstance != -1)
		{
			LazyActor = FUniqueObjectGuid(PrerequisiteActorId).FixupForPIE(PIECaptureInstance);
		}
		else
		{
			LazyActor = FUniqueObjectGuid(PrerequisiteActorId);
		}

		PrerequisiteActor = Actor = LazyActor.Get();
	}

	if (Actor && Actor->GetWorld() && !Actor->GetWorld()->HasBegunPlay())
	{
		return;
	}

	if (!PrerequisiteActorId.IsValid() || (Actor && Actor->HasActorBegunPlay()))
	{
		CaptureThisFrame(DeltaSeconds);
	}
}

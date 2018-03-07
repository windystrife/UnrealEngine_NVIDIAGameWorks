// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CineCameraActor.h"
#include "DrawDebugHelpers.h"
#include "CineCameraComponent.h"

#define LOCTEXT_NAMESPACE "CineCameraActor"

//////////////////////////////////////////////////////////////////////////
// ACameraActor

ACineCameraActor::ACineCameraActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer
			.SetDefaultSubobjectClass<UCineCameraComponent>(TEXT("CameraComponent"))
	)
{
	CineCameraComponent = Cast<UCineCameraComponent>(GetCameraComponent());

	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(true);
}

void ACineCameraActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	LookatTrackingSettings.LastLookatTrackingRotation = GetActorRotation();
}

bool ACineCameraActor::ShouldTickIfViewportsOnly() const
{
	return true;
}

FVector ACineCameraActor::GetLookatLocation() const
{
	FVector FinalLookat;
	if (LookatTrackingSettings.ActorToTrack)
	{
		FTransform const BaseTransform = LookatTrackingSettings.ActorToTrack->GetActorTransform();
		FinalLookat = BaseTransform.TransformPosition(LookatTrackingSettings.RelativeOffset);
	}
	else
	{
		FinalLookat = LookatTrackingSettings.RelativeOffset;
	}

	return FinalLookat;
}

static const FColor DebugLookatTrackingPointSolidColor(200, 200, 32, 128);		// yellow
static const FColor DebugLookatTrackingPointOutlineColor = FColor::Black;

static const FColor DebugFocusPointSolidColor(102, 26, 204, 153);		// purple
static const FColor DebugFocusPointOutlineColor = FColor::Black;

void ACineCameraActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (GetCameraComponent() && ShouldTickForTracking())
	{
		if (LookatTrackingSettings.bEnableLookAtTracking)
		{
			// do the lookat tracking
			// #note this will turn the whole actor, which assumes the cameracomponent's transform is the same as the root component
			// more complex component hierarchies will require different handling here
			FVector const LookatLoc = GetLookatLocation();
			FVector const ToLookat = LookatLoc - GetActorLocation();
			FRotator FinalRot = 
				bResetInterplation
				? ToLookat.Rotation()
				: FMath::RInterpTo(LookatTrackingSettings.LastLookatTrackingRotation, ToLookat.Rotation(), DeltaTime, LookatTrackingSettings.LookAtTrackingInterpSpeed);

			if (LookatTrackingSettings.bAllowRoll)
			{
				FinalRot.Roll = GetActorRotation().Roll;
			}

			SetActorRotation(FinalRot);

			// we store this ourselves in case other systems try to change our rotation, and end fighting the interpolation
			LookatTrackingSettings.LastLookatTrackingRotation = FinalRot;

#if ENABLE_DRAW_DEBUG
			if (LookatTrackingSettings.bDrawDebugLookAtTrackingPosition)
			{
				::DrawDebugSolidBox(GetWorld(), LookatLoc, FVector(12.f), DebugLookatTrackingPointSolidColor);
				::DrawDebugBox(GetWorld(), LookatLoc, FVector(12.f), DebugLookatTrackingPointOutlineColor);
			}
#endif // ENABLE_DRAW_DEBUG
		}

#if ENABLE_DRAW_DEBUG
		if (CineCameraComponent->FocusSettings.TrackingFocusSettings.bDrawDebugTrackingFocusPoint)
		{
			AActor const* const TrackedActor = CineCameraComponent->FocusSettings.TrackingFocusSettings.ActorToTrack;

			FVector FocusPoint;
			if (TrackedActor)
			{
				FTransform const BaseTransform = TrackedActor->GetActorTransform();
				FocusPoint = BaseTransform.TransformPosition(CineCameraComponent->FocusSettings.TrackingFocusSettings.RelativeOffset);
			}
			else
			{
				FocusPoint = CineCameraComponent->FocusSettings.TrackingFocusSettings.RelativeOffset;
			}
			
			::DrawDebugSolidBox(GetWorld(), FocusPoint, FVector(12.f), DebugFocusPointSolidColor);
			::DrawDebugBox(GetWorld(), FocusPoint, FVector(12.f), DebugFocusPointOutlineColor);
		}
#endif // ENABLE_DRAW_DEBUG

#if WITH_EDITORONLY_DATA
		if (CineCameraComponent->FocusSettings.bDrawDebugFocusPlane)
		{
			CineCameraComponent->UpdateDebugFocusPlane();
		}
#endif // WITH_EDITORONLY_DATA
	}

	bResetInterplation = false;
}


void ACineCameraActor::NotifyCameraCut()
{
	Super::NotifyCameraCut();
	
	bResetInterplation = true;
}

bool ACineCameraActor::ShouldTickForTracking() const
{
	bool bShouldTickForTracking = 
		LookatTrackingSettings.bEnableLookAtTracking || 
		CineCameraComponent->FocusSettings.TrackingFocusSettings.bDrawDebugTrackingFocusPoint;

#if WITH_EDITORONLY_DATA
	if (CineCameraComponent->FocusSettings.bDrawDebugFocusPlane)
	{
		bShouldTickForTracking = true;
	}
#endif // WITH_EDITORONLY_DATA

	return bShouldTickForTracking;
}


#undef LOCTEXT_NAMESPACE

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
//
#include "MotionControllerComponent.h"
#include "GameFramework/Pawn.h"
#include "PrimitiveSceneProxy.h"
#include "Misc/ScopeLock.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Features/IModularFeatures.h"
#include "IMotionController.h"
#include "PrimitiveSceneInfo.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

namespace {
	/** This is to prevent destruction of motion controller components while they are
		in the middle of being accessed by the render thread */
	FCriticalSection CritSect;

	/** Console variable for specifying whether motion controller late update is used */
	TAutoConsoleVariable<int32> CVarEnableMotionControllerLateUpdate(
		TEXT("vr.EnableMotionControllerLateUpdate"),
		1,
		TEXT("This command allows you to specify whether the motion controller late update is applied.\n")
		TEXT(" 0: don't use late update\n")
		TEXT(" 1: use late update (default)"),
		ECVF_Cheat);
} // anonymous namespace

//=============================================================================
UMotionControllerComponent::UMotionControllerComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer),
RenderThreadComponentScale(1.0f,1.0f,1.0f)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bTickEvenWhenPaused = true;

	PlayerIndex = 0;
	Hand = EControllerHand::Left;
	bDisableLowLatencyUpdate = false;
	bHasAuthority = false;
	bAutoActivate = true;
}

//=============================================================================
UMotionControllerComponent::~UMotionControllerComponent()
{
	if (ViewExtension.IsValid())
	{
		{
			// This component could be getting accessed from the render thread so it needs to wait
			// before clearing MotionControllerComponent and allowing the destructor to continue
			FScopeLock ScopeLock(&CritSect);
			ViewExtension->MotionControllerComponent = NULL;
		}

		ViewExtension.Reset();
	}	
}

void UMotionControllerComponent::SendRenderTransform_Concurrent()
{
	RenderThreadRelativeTransform = GetRelativeTransform();
	RenderThreadComponentScale = GetComponentScale();
	Super::SendRenderTransform_Concurrent();
}

//=============================================================================
void UMotionControllerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsActive)
	{
		FVector Position;
		FRotator Orientation;
		float WorldToMeters = GetWorld() ? GetWorld()->GetWorldSettings()->WorldToMeters : 100.0f;
		bTracked = PollControllerState(Position, Orientation, WorldToMeters);
		if (bTracked)
		{
			SetRelativeLocationAndRotation(Position, Orientation);
		}

		if (!ViewExtension.IsValid() && GEngine)
		{
			ViewExtension = FSceneViewExtensions::NewExtension<FViewExtension>(this);
		}
	}
}

//=============================================================================
bool UMotionControllerComponent::PollControllerState(FVector& Position, FRotator& Orientation, float WorldToMetersScale)
{
	if (IsInGameThread())
	{
		// Cache state from the game thread for use on the render thread
		const AActor* MyOwner = GetOwner();
		const APawn* MyPawn = Cast<APawn>(MyOwner);
		bHasAuthority = MyPawn ? MyPawn->IsLocallyControlled() : (MyOwner->Role == ENetRole::ROLE_Authority);
	}

	if ((PlayerIndex != INDEX_NONE) && bHasAuthority)
	{
		TArray<IMotionController*> MotionControllers = IModularFeatures::Get().GetModularFeatureImplementations<IMotionController>( IMotionController::GetModularFeatureName() );
		for( auto MotionController : MotionControllers )
		{
			if (MotionController == nullptr)
			{
				continue;
			}

			EControllerHand QueryHand = (Hand == EControllerHand::AnyHand) ? EControllerHand::Left : Hand;
			if (MotionController->GetControllerOrientationAndPosition(PlayerIndex, QueryHand, Orientation, Position, WorldToMetersScale))
			{
				CurrentTrackingStatus = MotionController->GetControllerTrackingStatus(PlayerIndex, Hand);
				return true;
			}
			
			// If we've made it here, we need to see if there's a right hand controller that reports back the position
			if (Hand == EControllerHand::AnyHand && MotionController->GetControllerOrientationAndPosition(PlayerIndex, EControllerHand::Right, Orientation, Position, WorldToMetersScale))
			{
				CurrentTrackingStatus = MotionController->GetControllerTrackingStatus(PlayerIndex, EControllerHand::Right);
				return true;
			}
		}
	}
	return false;
}

//=============================================================================
void UMotionControllerComponent::FViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	if (!MotionControllerComponent)
	{
		return;
	}

	// Set up the late update state for the controller component
	LateUpdate.Setup(MotionControllerComponent->CalcNewComponentToWorld(FTransform()), MotionControllerComponent);
}

//=============================================================================
void UMotionControllerComponent::FViewExtension::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	if (!MotionControllerComponent)
	{
		return;
	}

	FTransform OldTransform;
	FTransform NewTransform;
	{
		FScopeLock ScopeLock(&CritSect);
		if (!MotionControllerComponent)
		{
			return;
		}

		// Find a view that is associated with this player.
		float WorldToMetersScale = -1.0f;
		for (const FSceneView* SceneView : InViewFamily.Views)
		{
			if (SceneView && SceneView->PlayerIndex == MotionControllerComponent->PlayerIndex)
			{
				WorldToMetersScale = SceneView->WorldToMetersScale;
				break;
			}
		}
		// If there are no views associated with this player use view 0.
		if (WorldToMetersScale < 0.0f)
		{
			check(InViewFamily.Views.Num() > 0);
			WorldToMetersScale = InViewFamily.Views[0]->WorldToMetersScale;
		}

		// Poll state for the most recent controller transform
		FVector Position;
		FRotator Orientation;
		if (!MotionControllerComponent->PollControllerState(Position, Orientation, WorldToMetersScale))
		{
			return;
		}

		OldTransform = MotionControllerComponent->RenderThreadRelativeTransform;
		NewTransform = FTransform(Orientation, Position, MotionControllerComponent->RenderThreadComponentScale);
	} // Release the lock on the MotionControllerComponent

	// Tell the late update manager to apply the offset to the scene components
	LateUpdate.Apply_RenderThread(InViewFamily.Scene, OldTransform, NewTransform);
}

bool UMotionControllerComponent::FViewExtension::IsActiveThisFrame(class FViewport* InViewport) const
{
	check(IsInGameThread());
	return MotionControllerComponent && !MotionControllerComponent->bDisableLowLatencyUpdate && CVarEnableMotionControllerLateUpdate.GetValueOnGameThread();
}


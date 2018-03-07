// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Camera/CameraActor.h"
#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "AudioDevice.h"
#include "Particles/EmitterCameraLensEffectBase.h"
#include "Camera/CameraAnim.h"
#include "Camera/CameraAnimInst.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraModifier.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/CameraPhotography.h"
#include "GameFramework/PlayerState.h"
#include "IXRTrackingSystem.h" // for IsHeadTrackingAllowed()

DEFINE_LOG_CATEGORY_STATIC(LogPlayerCameraManager, Log, All);

DECLARE_CYCLE_STAT(TEXT("ServerUpdateCamera"), STAT_ServerUpdateCamera, STATGROUP_Game);


//////////////////////////////////////////////////////////////////////////
// APlayerCameraManager

APlayerCameraManager::APlayerCameraManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static FName NAME_Default(TEXT("Default"));

	DefaultFOV = 90.0f;
	DefaultAspectRatio = 1.33333f;
	bDefaultConstrainAspectRatio = false;
	DefaultOrthoWidth = 512.0f;
	bHidden = true;
	bReplicates = false;
	FreeCamDistance = 256.0f;
	bDebugClientSideCamera = false;
	ViewPitchMin = -89.9f;
	ViewPitchMax = 89.9f;
	ViewYawMin = 0.f;
	ViewYawMax = 359.999f;
	ViewRollMin = -89.9f;
	ViewRollMax = 89.9f;
	bUseClientSideCameraUpdates = true;
	CameraStyle = NAME_Default;
	bCanBeDamaged = false;

	// create dummy transform component
	TransformComponent = CreateDefaultSubobject<USceneComponent>(TEXT("TransformComponent0"));
	RootComponent = TransformComponent;

	// support camerashakes by default
	DefaultModifiers.Add(UCameraModifier_CameraShake::StaticClass());
}

/// @cond DOXYGEN_WARNINGS

void APlayerCameraManager::PhotographyCameraModify_Implementation(const FVector NewCameraLocation, const FVector PreviousCameraLocation, const FVector OriginalCameraLocation, FVector& OutCameraLocation)
{	// let proposed camera through unmodified by default
	OutCameraLocation = NewCameraLocation;
}

void APlayerCameraManager::OnPhotographySessionStart_Implementation()
{	// do nothing by default
}

void APlayerCameraManager::OnPhotographySessionEnd_Implementation()
{	// do nothing by default
}

void APlayerCameraManager::OnPhotographyMultiPartCaptureStart_Implementation()
{	// do nothing by default
}

void APlayerCameraManager::OnPhotographyMultiPartCaptureEnd_Implementation()
{	// do nothing by default
}

/// @endcond


APlayerController* APlayerCameraManager::GetOwningPlayerController() const
{
	return PCOwner;
}


void APlayerCameraManager::SetViewTarget(class AActor* NewTarget, struct FViewTargetTransitionParams TransitionParams)
{
	// Make sure view target is valid
	if( NewTarget == NULL )
	{
		NewTarget = PCOwner;
	}

	// Update current ViewTargets
	ViewTarget.CheckViewTarget(PCOwner);
	if( PendingViewTarget.Target )
	{
		PendingViewTarget.CheckViewTarget(PCOwner);
	}

	// If we're already transitioning to this new target, don't interrupt.
	if( PendingViewTarget.Target != NULL && NewTarget == PendingViewTarget.Target )
	{
		return;
	}

	// if viewtarget different then new one or we're transitioning from the same target with locked outgoing, then assign it
	if((NewTarget != ViewTarget.Target) || (PendingViewTarget.Target && BlendParams.bLockOutgoing))
	{
		// if a transition time is specified, then set pending view target accordingly
		if( TransitionParams.BlendTime > 0 )
		{
			// band-aid fix so that EndViewTarget() gets called properly in this case
			if (PendingViewTarget.Target == NULL)
			{
				PendingViewTarget.Target = ViewTarget.Target;
			}

			// use last frame's POV
			ViewTarget.POV = LastFrameCameraCache.POV;
			BlendParams = TransitionParams;
			BlendTimeToGo = TransitionParams.BlendTime;
			
			AssignViewTarget(NewTarget, PendingViewTarget, TransitionParams);
			PendingViewTarget.CheckViewTarget(PCOwner);
		}
		else
		{
			// otherwise, assign new viewtarget instantly
			AssignViewTarget(NewTarget, ViewTarget);
			ViewTarget.CheckViewTarget(PCOwner);
			// remove old pending ViewTarget so we don't still try to switch to it
			PendingViewTarget.Target = NULL;
		}
	}
	else
	{
		// we're setting the viewtarget to the viewtarget we were transitioning away from,
		// just abort the transition.
		// @fixme, investigate if we want this case to go through the above code, so AssignViewTarget et al
		// get called
		if (PendingViewTarget.Target != NULL)
		{
			if (!PCOwner->IsPendingKillPending() && !PCOwner->IsLocalPlayerController() && GetNetMode() != NM_Client)
			{
				PCOwner->ClientSetViewTarget(NewTarget, TransitionParams);
			}
		}
		PendingViewTarget.Target = NULL;
	}
}


void APlayerCameraManager::AssignViewTarget(AActor* NewTarget, FTViewTarget& VT, struct FViewTargetTransitionParams TransitionParams)
{
	if( !NewTarget || (NewTarget == VT.Target) )
	{
		return;
	}

// 	UE_LOG(LogPlayerCameraManager, Log, TEXT("%f AssignViewTarget OldTarget: %s, NewTarget: %s, BlendTime: %f"), GetWorld()->TimeSeconds, VT.Target ? *VT.Target->GetFName().ToString() : TEXT("NULL"),
// 		NewTarget ? *NewTarget->GetFName().ToString() : TEXT("NULL"),
// 		TransitionParams.BlendTime);

	AActor* OldViewTarget = VT.Target;
	VT.Target = NewTarget;

	// Use default FOV and aspect ratio.
	VT.POV.AspectRatio = DefaultAspectRatio;
	VT.POV.bConstrainAspectRatio = bDefaultConstrainAspectRatio;
	VT.POV.FOV = DefaultFOV;

	if (OldViewTarget)
	{
		OldViewTarget->EndViewTarget(PCOwner);
	}

	VT.Target->BecomeViewTarget(PCOwner);

	if (!PCOwner->IsLocalPlayerController() && (GetNetMode() != NM_Client))
	{
		PCOwner->ClientSetViewTarget(VT.Target, TransitionParams);
	}
}

AActor* APlayerCameraManager::GetViewTarget() const
{
	// to handle verification/caching behavior while preserving constness upstream
	APlayerCameraManager* const NonConstThis = const_cast<APlayerCameraManager*>(this);

	// if blending to another view target, return this one first
	if( PendingViewTarget.Target )
	{
		NonConstThis->PendingViewTarget.CheckViewTarget(NonConstThis->PCOwner);
		if( PendingViewTarget.Target )
		{
			return PendingViewTarget.Target;
		}
	}

	NonConstThis->ViewTarget.CheckViewTarget(NonConstThis->PCOwner);
	return ViewTarget.Target;
}

APawn* APlayerCameraManager::GetViewTargetPawn() const
{
	// to handle verification/caching behavior while preserving constness upstream
	APlayerCameraManager* const NonConstThis = const_cast<APlayerCameraManager*>(this);

	// if blending to another view target, return this one first
	if (PendingViewTarget.Target)
	{
		NonConstThis->PendingViewTarget.CheckViewTarget(NonConstThis->PCOwner);
		if (PendingViewTarget.Target)
		{
			return PendingViewTarget.GetTargetPawn();
		}
	}

	NonConstThis->ViewTarget.CheckViewTarget(NonConstThis->PCOwner);
	return ViewTarget.GetTargetPawn();
}

bool APlayerCameraManager::ShouldTickIfViewportsOnly() const
{
	return (PCOwner != NULL);
}

void APlayerCameraManager::ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	ClearCachedPPBlends();

	// Loop through each camera modifier
	for (int32 ModifierIdx = 0; ModifierIdx < ModifierList.Num(); ++ModifierIdx)
	{
		// Apply camera modification and output into DesiredCameraOffset/DesiredCameraRotation
		if ((ModifierList[ModifierIdx] != NULL) && !ModifierList[ModifierIdx]->IsDisabled())
		{
			// If ModifyCamera returns true, exit loop
			// Allows high priority things to dictate if they are
			// the last modifier to be applied
			if (ModifierList[ModifierIdx]->ModifyCamera(DeltaTime, InOutPOV))
			{
				break;
			}
		}
	}

	// Now apply CameraAnims
	// these essentially behave as the highest-pri modifier.
	for (int32 Idx = 0; Idx < ActiveAnims.Num(); ++Idx)
	{
		UCameraAnimInst* const AnimInst = ActiveAnims[Idx];

		if (AnimCameraActor && !AnimInst->bFinished)
		{
			// clear out animated camera actor
			InitTempCameraActor(AnimCameraActor, AnimInst);

			// evaluate the animation at the new time
			AnimInst->AdvanceAnim(DeltaTime, false);

			// Add weighted properties to the accumulator actor
			if (AnimInst->CurrentBlendWeight > 0.f)
			{
				ApplyAnimToCamera(AnimCameraActor, AnimInst, InOutPOV);
			}
		}

		// changes to this are good for a single update, so reset this to 1.f after processing
		AnimInst->TransientScaleModifier = 1.f;

		// handle animations that have finished
		if (AnimInst->bFinished)
		{
			ReleaseCameraAnimInst(AnimInst);
			Idx--;		// we removed this from the ActiveAnims array
		}
	}

	// need to zero this when we are done with it.  playing another animation
	// will calc a new InitialTM for the move track instance based on these values.
	if (AnimCameraActor)
	{
		AnimCameraActor->TeleportTo(FVector::ZeroVector, FRotator::ZeroRotator);
	}
}

void APlayerCameraManager::AddCachedPPBlend(struct FPostProcessSettings& PPSettings, float BlendWeight)
{
	check(PostProcessBlendCache.Num() == PostProcessBlendCacheWeights.Num());
	PostProcessBlendCache.Add(PPSettings);
	PostProcessBlendCacheWeights.Add(BlendWeight);
}

void APlayerCameraManager::ClearCachedPPBlends()
{
	PostProcessBlendCache.Empty();
	PostProcessBlendCacheWeights.Empty();
}

void APlayerCameraManager::GetCachedPostProcessBlends(TArray<FPostProcessSettings> const*& OutPPSettings, TArray<float> const*& OutBlendWeigthts) const
{
	OutPPSettings = &PostProcessBlendCache;
	OutBlendWeigthts = &PostProcessBlendCacheWeights;
}

void APlayerCameraManager::ApplyAnimToCamera(ACameraActor const* AnimatedCamActor, UCameraAnimInst const* AnimInst, FMinimalViewInfo& InOutPOV)
{
	AnimInst->ApplyToView(InOutPOV);

	// postprocess
	if (AnimatedCamActor->GetCameraComponent()->PostProcessBlendWeight > 0.f)
	{
		AddCachedPPBlend(AnimatedCamActor->GetCameraComponent()->PostProcessSettings, AnimatedCamActor->GetCameraComponent()->PostProcessBlendWeight * AnimInst->CurrentBlendWeight);
	}
}

UCameraAnimInst* APlayerCameraManager::AllocCameraAnimInst()
{
	check(IsInGameThread());

	UCameraAnimInst* FreeAnim = (FreeAnims.Num() > 0) ? FreeAnims.Pop() : NULL;
	if (FreeAnim)
	{
		UCameraAnimInst const* const DefaultInst = GetDefault<UCameraAnimInst>();

		ActiveAnims.Push(FreeAnim);

		// reset some defaults
		if (DefaultInst)
		{
			FreeAnim->TransientScaleModifier = DefaultInst->TransientScaleModifier;
			FreeAnim->PlaySpace = DefaultInst->PlaySpace;
		}

		// make sure any previous anim has been terminated correctly
		check( (FreeAnim->MoveTrack == NULL) && (FreeAnim->MoveInst == NULL) );
	}

	return FreeAnim;
}


void APlayerCameraManager::ReleaseCameraAnimInst(UCameraAnimInst* Inst)
{	
	ActiveAnims.Remove(Inst);
	FreeAnims.Push(Inst);
}


UCameraAnimInst* APlayerCameraManager::FindInstanceOfCameraAnim(UCameraAnim const* Anim) const
{
	int32 const NumActiveAnims = ActiveAnims.Num();
	for (int32 Idx=0; Idx<NumActiveAnims; Idx++)
	{
		if (ActiveAnims[Idx]->CamAnim == Anim)
		{
			return ActiveAnims[Idx];
		}
	}

	return NULL;
}

UCameraAnimInst* APlayerCameraManager::PlayCameraAnim(UCameraAnim* Anim, float Rate, float Scale, float BlendInTime, float BlendOutTime, bool bLoop, bool bRandomStartTime, float Duration, ECameraAnimPlaySpace::Type PlaySpace, FRotator UserPlaySpaceRot)
{
	// get a new instance and play it
	if (AnimCameraActor != NULL)
	{
		UCameraAnimInst* const Inst = AllocCameraAnimInst();
		if (Inst)
		{
			if (!Anim->bRelativeToInitialFOV)
			{
				Inst->InitialFOV = ViewTarget.POV.FOV;
			}
			Inst->LastCameraLoc = FVector::ZeroVector;		// clear LastCameraLoc
			Inst->Play(Anim, AnimCameraActor, Rate, Scale, BlendInTime, BlendOutTime, bLoop, bRandomStartTime, Duration);
			Inst->SetPlaySpace(PlaySpace, UserPlaySpaceRot);
			return Inst;
		}
	}

	return NULL;
}

void APlayerCameraManager::StopAllInstancesOfCameraAnim(UCameraAnim* Anim, bool bImmediate)
{
	// find cameraaniminst for this.
	for (int32 Idx=0; Idx<ActiveAnims.Num(); ++Idx)
	{
		if (ActiveAnims[Idx]->CamAnim == Anim)
		{
			ActiveAnims[Idx]->Stop(bImmediate);
		}
	}
}

void APlayerCameraManager::StopAllCameraAnims(bool bImmediate)
{
	for (int32 Idx=0; Idx<ActiveAnims.Num(); ++Idx)
	{
		ActiveAnims[Idx]->Stop(bImmediate);
	}
}

void APlayerCameraManager::StopCameraAnimInst(class UCameraAnimInst* AnimInst, bool bImmediate)
{
	if (AnimInst != NULL)
	{
		AnimInst->Stop(bImmediate);
	}
}


void APlayerCameraManager::InitTempCameraActor(ACameraActor* CamActor, UCameraAnimInst const* AnimInstToInitFor) const
{
	if (CamActor)
	{
		CamActor->TeleportTo(FVector::ZeroVector, FRotator::ZeroRotator);

		if (AnimInstToInitFor)
		{
			ACameraActor const* const DefaultCamActor = GetDefault<ACameraActor>();
			if (DefaultCamActor)
			{
				CamActor->GetCameraComponent()->AspectRatio = DefaultCamActor->GetCameraComponent()->AspectRatio;
				CamActor->GetCameraComponent()->FieldOfView = AnimInstToInitFor->CamAnim->BaseFOV;
				CamActor->GetCameraComponent()->PostProcessSettings = AnimInstToInitFor->CamAnim->BasePostProcessSettings;
				CamActor->GetCameraComponent()->PostProcessBlendWeight = AnimInstToInitFor->CamAnim->BasePostProcessBlendWeight;
			}
		}
	}
}

void APlayerCameraManager::UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime)
{
	if (OutVT.Target)
	{
		const bool bK2Camera = BlueprintUpdateCamera(OutVT.Target, OutVT.POV.Location, OutVT.POV.Rotation, OutVT.POV.FOV);
		if (bK2Camera == false)
		{
			OutVT.Target->CalcCamera(DeltaTime, OutVT.POV);
		}
	}
}

void APlayerCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	// Don't update outgoing viewtarget during an interpolation 
	if ((PendingViewTarget.Target != NULL) && BlendParams.bLockOutgoing && OutVT.Equal(ViewTarget))
	{
		return;
	}

	// store previous POV, in case we need it later
	FMinimalViewInfo OrigPOV = OutVT.POV;

	//@TODO: CAMERA: Should probably reset the view target POV fully here
	OutVT.POV.FOV = DefaultFOV;
	OutVT.POV.OrthoWidth = DefaultOrthoWidth;
	OutVT.POV.AspectRatio = DefaultAspectRatio;
	OutVT.POV.bConstrainAspectRatio = bDefaultConstrainAspectRatio;
	OutVT.POV.bUseFieldOfViewForLOD = true;
	OutVT.POV.ProjectionMode = bIsOrthographic ? ECameraProjectionMode::Orthographic : ECameraProjectionMode::Perspective;
	OutVT.POV.PostProcessSettings.SetBaseValues();
	OutVT.POV.PostProcessBlendWeight = 1.0f;


	bool bDoNotApplyModifiers = false;

	if (ACameraActor* CamActor = Cast<ACameraActor>(OutVT.Target))
	{
		// Viewing through a camera actor.
		CamActor->GetCameraComponent()->GetCameraView(DeltaTime, OutVT.POV);
	}
	else
	{

		static const FName NAME_Fixed = FName(TEXT("Fixed"));
		static const FName NAME_ThirdPerson = FName(TEXT("ThirdPerson"));
		static const FName NAME_FreeCam = FName(TEXT("FreeCam"));
		static const FName NAME_FreeCam_Default = FName(TEXT("FreeCam_Default"));
		static const FName NAME_FirstPerson = FName(TEXT("FirstPerson"));

		if (CameraStyle == NAME_Fixed)
		{
			// do not update, keep previous camera position by restoring
			// saved POV, in case CalcCamera changes it but still returns false
			OutVT.POV = OrigPOV;

			// don't apply modifiers when using this debug camera mode
			bDoNotApplyModifiers = true;
		}
		else if (CameraStyle == NAME_ThirdPerson || CameraStyle == NAME_FreeCam || CameraStyle == NAME_FreeCam_Default)
		{
			// Simple third person view implementation
			FVector Loc = OutVT.Target->GetActorLocation();
			FRotator Rotator = OutVT.Target->GetActorRotation();

			if (OutVT.Target == PCOwner)
			{
				Loc = PCOwner->GetFocalLocation();
			}

			// Take into account Mesh Translation so it takes into account the PostProcessing we do there.
			// @fixme, can crash in certain BP cases where default mesh is null
//			APawn* TPawn = Cast<APawn>(OutVT.Target);
// 			if ((TPawn != NULL) && (TPawn->Mesh != NULL))
// 			{
// 				Loc += FQuatRotationMatrix(OutVT.Target->GetActorQuat()).TransformVector(TPawn->Mesh->RelativeLocation - GetDefault<APawn>(TPawn->GetClass())->Mesh->RelativeLocation);
// 			}

			//OutVT.Target.GetActorEyesViewPoint(Loc, Rot);
			if( CameraStyle == NAME_FreeCam || CameraStyle == NAME_FreeCam_Default )
			{
				Rotator = PCOwner->GetControlRotation();
			}

			FVector Pos = Loc + ViewTargetOffset + FRotationMatrix(Rotator).TransformVector(FreeCamOffset) - Rotator.Vector() * FreeCamDistance;
			FCollisionQueryParams BoxParams(SCENE_QUERY_STAT(FreeCam), false, this);
			BoxParams.AddIgnoredActor(OutVT.Target);
			FHitResult Result;

			GetWorld()->SweepSingleByChannel(Result, Loc, Pos, FQuat::Identity, ECC_Camera, FCollisionShape::MakeBox(FVector(12.f)), BoxParams);
			OutVT.POV.Location = !Result.bBlockingHit ? Pos : Result.Location;
			OutVT.POV.Rotation = Rotator;

			// don't apply modifiers when using this debug camera mode
			bDoNotApplyModifiers = true;
		}
		else if (CameraStyle == NAME_FirstPerson)
		{
			// Simple first person, view through viewtarget's 'eyes'
			OutVT.Target->GetActorEyesViewPoint(OutVT.POV.Location, OutVT.POV.Rotation);
	
			// don't apply modifiers when using this debug camera mode
			bDoNotApplyModifiers = true;
		}
		else
		{
			UpdateViewTargetInternal(OutVT, DeltaTime);
		}
	}

	if (!bDoNotApplyModifiers || bAlwaysApplyModifiers)
	{
		// Apply camera modifiers at the end (view shakes for example)
		ApplyCameraModifiers(DeltaTime, OutVT.POV);
	}

	// Synchronize the actor with the view target results
	SetActorLocationAndRotation(OutVT.POV.Location, OutVT.POV.Rotation, false);

	UpdateCameraLensEffects(OutVT);
}


void APlayerCameraManager::UpdateCameraLensEffects(const FTViewTarget& OutVT)
{
	for (int32 Idx=0; Idx<CameraLensEffects.Num(); ++Idx)
	{
		if (CameraLensEffects[Idx] != NULL)
		{
			CameraLensEffects[Idx]->UpdateLocation(OutVT.POV.Location, OutVT.POV.Rotation, OutVT.POV.FOV);
		}
	}
}

void APlayerCameraManager::ApplyAudioFade()
{
	if (GEngine)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			if (FAudioDevice* AudioDevice = World->GetAudioDevice())
			{
				AudioDevice->SetTransientMasterVolume(1.0f - FadeAmount);
			}
		}
	}
}

void APlayerCameraManager::StopAudioFade()
{
	if (GEngine)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			if (FAudioDevice* AudioDevice = World->GetAudioDevice())
			{
				AudioDevice->SetTransientMasterVolume(1.0f);
			}
		}
	}
}

UCameraModifier* APlayerCameraManager::AddNewCameraModifier(TSubclassOf<UCameraModifier> ModifierClass)
{
	UCameraModifier* const NewMod = NewObject<UCameraModifier>(this, ModifierClass);
	if (NewMod)
	{
		if (AddCameraModifierToList(NewMod) == true)
		{
			return NewMod;
		}
	}
	
	return nullptr;
}

UCameraModifier* APlayerCameraManager::FindCameraModifierByClass(TSubclassOf<UCameraModifier> ModifierClass)
{
	for (UCameraModifier* Mod : ModifierList)
	{
		if (Mod->GetClass() == ModifierClass)
		{
			return Mod;
		}
	}

	return nullptr;
}

bool APlayerCameraManager::AddCameraModifierToList(UCameraModifier* NewModifier)
{
	if (NewModifier)
	{
		// Look through current modifier list and find slot for this priority
		int32 BestIdx = ModifierList.Num();
		for (int32 ModifierIdx = 0; ModifierIdx < ModifierList.Num(); ModifierIdx++)
		{
			UCameraModifier* const M = ModifierList[ModifierIdx];
			if (M)
			{
				if (M == NewModifier)
				{
					// already in list, just bail
					return false;
				}

				// If priority of current index has passed or equaled ours - we have the insert location
				if (NewModifier->Priority <= M->Priority)
				{
					// Disallow addition of exclusive modifier if priority is already occupied
					if (NewModifier->bExclusive && NewModifier->Priority == M->Priority)
					{
						return false;
					}

					// Update best index
					BestIdx = ModifierIdx;

					break;
				}
			}
		}

		// Insert self into best index
		ModifierList.InsertUninitialized(BestIdx, 1);
		ModifierList[BestIdx] = NewModifier;

		// Save camera
		NewModifier->AddedToCamera(this);
		return true;
	}

	return false;
}

bool APlayerCameraManager::RemoveCameraModifier(UCameraModifier* ModifierToRemove)
{
	if (ModifierToRemove)
	{
		// Loop through each modifier in camera
		for (int32 ModifierIdx = 0; ModifierIdx < ModifierList.Num(); ModifierIdx++)
		{
			// If we found ourselves, remove ourselves from the list and return
			if (ModifierList[ModifierIdx] == ModifierToRemove)
			{
				ModifierList.RemoveAt(ModifierIdx, 1);
				return true;
			}
		}
	}

	// Didn't find it in the list, nothing removed
	return false;
}


void APlayerCameraManager::PostInitializeComponents()
{
	Super::PostInitializeComponents();

 	// Setup default camera modifiers
	if (DefaultModifiers.Num() > 0)
	{
		for (auto ModifierClass : DefaultModifiers)
		{
			// empty entries are not valid here, do work only for actual classes
			if (ModifierClass)
			{
				UCameraModifier* const NewMod = AddNewCameraModifier(ModifierClass);

				// cache ref to camera shake if this is it
				UCameraModifier_CameraShake* const ShakeMod = Cast<UCameraModifier_CameraShake>(NewMod);
				if (ShakeMod)
				{
					CachedCameraShakeMod = ShakeMod;
				}
			}
		}
	}

 	// create CameraAnimInsts in pool
	for (int32 Idx=0; Idx<MAX_ACTIVE_CAMERA_ANIMS; ++Idx)
	{
		AnimInstPool[Idx] = NewObject<UCameraAnimInst>(this);

		// add everything to the free list initially
		FreeAnims.Add(AnimInstPool[Idx]);
	}

	// spawn the temp CameraActor used for updating CameraAnims
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Owner = this;
	SpawnInfo.Instigator = Instigator;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.ObjectFlags |= RF_Transient;	// We never want to save these temp actors into a map
	AnimCameraActor = GetWorld()->SpawnActor<ACameraActor>(SpawnInfo);
}

void APlayerCameraManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// clean up the temp camera actor
	if (AnimCameraActor)
	{
		if (EndPlayReason == EEndPlayReason::Destroyed)
		{
			AnimCameraActor->Destroy();
		}
		AnimCameraActor = NULL;
	}
	Super::EndPlay(EndPlayReason);
}

void APlayerCameraManager::InitializeFor(APlayerController* PC)
{
	CameraCache.POV.FOV = DefaultFOV;
	PCOwner = PC;

	SetViewTarget(PC);

	// set the level default scale
	SetDesiredColorScale(GetWorldSettings()->DefaultColorScale, 5.f);

	// Force camera update so it doesn't sit at (0,0,0) for a full tick.
	// This can have side effects with streaming.
	UpdateCamera(0.f);
}


float APlayerCameraManager::GetFOVAngle() const
{
	return (LockedFOV > 0.f) ? LockedFOV : CameraCache.POV.FOV;
}

void APlayerCameraManager::SetFOV(float NewFOV)
{
	LockedFOV = NewFOV;
}

void APlayerCameraManager::UnlockFOV()
{
	LockedFOV = 0.f;
}

bool APlayerCameraManager::IsOrthographic() const
{
	return bIsOrthographic;
}

float APlayerCameraManager::GetOrthoWidth() const
{
	return (LockedOrthoWidth > 0.f) ? LockedOrthoWidth : DefaultOrthoWidth;
}

void APlayerCameraManager::SetOrthoWidth(float OrthoWidth)
{
	LockedOrthoWidth = OrthoWidth;
}

void APlayerCameraManager::UnlockOrthoWidth()
{
	LockedOrthoWidth = 0.f;
}

void APlayerCameraManager::GetCameraViewPoint(FVector& OutCamLoc, FRotator& OutCamRot) const
{
	OutCamLoc = CameraCache.POV.Location;
	OutCamRot = CameraCache.POV.Rotation;
}

FRotator APlayerCameraManager::GetCameraRotation() const
{
	return CameraCache.POV.Rotation;
}

FVector APlayerCameraManager::GetCameraLocation() const
{
	return CameraCache.POV.Location;
}

void APlayerCameraManager::SetDesiredColorScale(FVector NewColorScale, float InterpTime)
{
	// if color scaling is not enabled
	if (!bEnableColorScaling)
	{
		// set the default color scale
		bEnableColorScaling = true;
		ColorScale.X = 1.f;
		ColorScale.Y = 1.f;
		ColorScale.Z = 1.f;
	}

	// Don't bother interpolating if we're already scaling at the desired color
	if( NewColorScale != ColorScale )
	{
		// save the current as original
		OriginalColorScale = ColorScale;
		// set the new desired scale
		DesiredColorScale = NewColorScale;
		// set the interpolation duration/time
		ColorScaleInterpStartTime = GetWorld()->TimeSeconds;
		ColorScaleInterpDuration = InterpTime;
		// and enable color scale interpolation
		bEnableColorScaleInterp = true;
	}
}


void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
	check(PCOwner != nullptr);

	if ((PCOwner->Player && PCOwner->IsLocalPlayerController()) || !bUseClientSideCameraUpdates || bDebugClientSideCamera)
	{
		DoUpdateCamera(DeltaTime);

		if (bShouldSendClientSideCameraUpdate && IsNetMode(NM_Client))
		{
			SCOPE_CYCLE_COUNTER(STAT_ServerUpdateCamera);

			// compress the rotation down to 4 bytes
			int32 const ShortYaw = FRotator::CompressAxisToShort(CameraCache.POV.Rotation.Yaw);
			int32 const ShortPitch = FRotator::CompressAxisToShort(CameraCache.POV.Rotation.Pitch);
			int32 const CompressedRotation = (ShortYaw << 16) | ShortPitch;

			FVector ClientCameraPosition = FRepMovement::RebaseOntoZeroOrigin(CameraCache.POV.Location, this);
			PCOwner->ServerUpdateCamera(ClientCameraPosition, CompressedRotation);
			bShouldSendClientSideCameraUpdate = false;
		}
	}
}

bool APlayerCameraManager::AllowPhotographyMode() const
{
	return true;
}

void APlayerCameraManager::DoUpdateCamera(float DeltaTime)
{
	FMinimalViewInfo NewPOV = ViewTarget.POV;

	// update color scale interpolation
	if (bEnableColorScaleInterp)
	{
		float BlendPct = FMath::Clamp((GetWorld()->TimeSeconds - ColorScaleInterpStartTime) / ColorScaleInterpDuration, 0.f, 1.0f);
		ColorScale = FMath::Lerp(OriginalColorScale, DesiredColorScale, BlendPct);
		// if we've maxed
		if (BlendPct == 1.0f)
		{
			// disable further interpolation
			bEnableColorScaleInterp = false;
		}
	}

	// Don't update outgoing viewtarget during an interpolation when bLockOutgoing is set.
	if ((PendingViewTarget.Target == NULL) || !BlendParams.bLockOutgoing)
	{
		// Update current view target
		ViewTarget.CheckViewTarget(PCOwner);
		UpdateViewTarget(ViewTarget, DeltaTime);
	}

	// our camera is now viewing there
	NewPOV = ViewTarget.POV;

	// if we have a pending view target, perform transition from one to another.
	if (PendingViewTarget.Target != NULL)
	{
		BlendTimeToGo -= DeltaTime;

		// Update pending view target
		PendingViewTarget.CheckViewTarget(PCOwner);
		UpdateViewTarget(PendingViewTarget, DeltaTime);

		// blend....
		if (BlendTimeToGo > 0)
		{
			float DurationPct = (BlendParams.BlendTime - BlendTimeToGo) / BlendParams.BlendTime;

			float BlendPct = 0.f;
			switch (BlendParams.BlendFunction)
			{
			case VTBlend_Linear:
				BlendPct = FMath::Lerp(0.f, 1.f, DurationPct);
				break;
			case VTBlend_Cubic:
				BlendPct = FMath::CubicInterp(0.f, 0.f, 1.f, 0.f, DurationPct);
				break;
			case VTBlend_EaseIn:
				BlendPct = FMath::Lerp(0.f, 1.f, FMath::Pow(DurationPct, BlendParams.BlendExp));
				break;
			case VTBlend_EaseOut:
				BlendPct = FMath::Lerp(0.f, 1.f, FMath::Pow(DurationPct, 1.f / BlendParams.BlendExp));
				break;
			case VTBlend_EaseInOut:
				BlendPct = FMath::InterpEaseInOut(0.f, 1.f, DurationPct, BlendParams.BlendExp);
				break;
			default:
				break;
			}

			// Update pending view target blend
			NewPOV = ViewTarget.POV;
			NewPOV.BlendViewInfo(PendingViewTarget.POV, BlendPct);//@TODO: CAMERA: Make sure the sense is correct!  BlendViewTargets(ViewTarget, PendingViewTarget, BlendPct);
		}
		else
		{
			// we're done blending, set new view target
			ViewTarget = PendingViewTarget;

			// clear pending view target
			PendingViewTarget.Target = NULL;

			BlendTimeToGo = 0;

			// our camera is now viewing there
			NewPOV = PendingViewTarget.POV;
		}
	}

	if (bEnableFading)
	{
		if (bAutoAnimateFade)
		{
			FadeTimeRemaining = FMath::Max(FadeTimeRemaining - DeltaTime, 0.0f);
			if (FadeTime > 0.0f)
			{
				FadeAmount = FadeAlpha.X + ((1.f - FadeTimeRemaining / FadeTime) * (FadeAlpha.Y - FadeAlpha.X));
			}

			if ((bHoldFadeWhenFinished == false) && (FadeTimeRemaining <= 0.f))
			{
				// done
				StopCameraFade();
			}
		}

		if (bFadeAudio)
		{
			ApplyAudioFade();
		}
	}

	if (AllowPhotographyMode())
	{
		const bool bPhotographyCausedCameraCut = UpdatePhotographyCamera(NewPOV);
		bGameCameraCutThisFrame = bGameCameraCutThisFrame || bPhotographyCausedCameraCut;
	}

	// Cache results
	FillCameraCache(NewPOV);
}

void APlayerCameraManager::UpdateCameraPhotographyOnly()
{
	FMinimalViewInfo NewPOV = ViewTarget.POV;

	// update photography camera, if any
	bGameCameraCutThisFrame = UpdatePhotographyCamera(NewPOV);

	// Cache results
	FillCameraCache(NewPOV);
}

//! Overridable
bool APlayerCameraManager::UpdatePhotographyCamera(FMinimalViewInfo& NewPOV)
{
	// update photography camera, if any
	return FCameraPhotographyManager::Get().UpdateCamera(NewPOV, this);
}

FPOV APlayerCameraManager::BlendViewTargets(const FTViewTarget& A,const FTViewTarget& B, float Alpha)
{
	FPOV POV;
	POV.Location = FMath::Lerp(A.POV.Location, B.POV.Location, Alpha);
	POV.FOV = (A.POV.FOV +  Alpha * ( B.POV.FOV - A.POV.FOV));

	FRotator DeltaAng = (B.POV.Rotation - A.POV.Rotation).GetNormalized();
	POV.Rotation = A.POV.Rotation + Alpha * DeltaAng;

	return POV;
}



void APlayerCameraManager::FillCameraCache(const FMinimalViewInfo& NewInfo)
{
	NewInfo.Location.DiagnosticCheckNaN(TEXT("APlayerCameraManager::FillCameraCache: NewInfo.Location"));
	NewInfo.Rotation.DiagnosticCheckNaN(TEXT("APlayerCameraManager::FillCameraCache: NewInfo.Rotation"));

	// Backup last frame results.
	if (CameraCache.TimeStamp != GetWorld()->TimeSeconds)
	{
		LastFrameCameraCache = CameraCache;
	}

	CameraCache.TimeStamp = GetWorld()->TimeSeconds;
	CameraCache.POV = NewInfo;
}


void APlayerCameraManager::ProcessViewRotation(float DeltaTime, FRotator& OutViewRotation, FRotator& OutDeltaRot)
{
	for( int32 ModifierIdx = 0; ModifierIdx < ModifierList.Num(); ModifierIdx++ )
	{
		if( ModifierList[ModifierIdx] != NULL && 
			!ModifierList[ModifierIdx]->IsDisabled() )
		{
			if( ModifierList[ModifierIdx]->ProcessViewRotation(ViewTarget.Target, DeltaTime, OutViewRotation, OutDeltaRot) )
			{
				break;
			}
		}
	}

	// Add Delta Rotation
	OutViewRotation += OutDeltaRot;
	OutDeltaRot = FRotator::ZeroRotator;

	if(GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed())
	{
		// With the HMD devices, we can't limit the view pitch, because it's bound to the player's head.  A simple normalization will suffice
		OutViewRotation.Normalize();
	}
	else
	{
		// Limit Player View Axes
		LimitViewPitch( OutViewRotation, ViewPitchMin, ViewPitchMax );
		LimitViewYaw( OutViewRotation, ViewYawMin, ViewYawMax );
		LimitViewRoll( OutViewRotation, ViewRollMin, ViewRollMax );
	}
}



void APlayerCameraManager::LimitViewPitch( FRotator& ViewRotation, float InViewPitchMin, float InViewPitchMax )
{
	ViewRotation.Pitch = FMath::ClampAngle(ViewRotation.Pitch, InViewPitchMin, InViewPitchMax);
	ViewRotation.Pitch = FRotator::ClampAxis(ViewRotation.Pitch);
}

void APlayerCameraManager::LimitViewRoll( FRotator&  ViewRotation, float InViewRollMin, float InViewRollMax)
{
	ViewRotation.Roll = FMath::ClampAngle(ViewRotation.Roll, InViewRollMin, InViewRollMax);
	ViewRotation.Roll = FRotator::ClampAxis(ViewRotation.Roll);
}

void APlayerCameraManager::LimitViewYaw(FRotator& ViewRotation, float InViewYawMin, float InViewYawMax)
{
	ViewRotation.Yaw = FMath::ClampAngle(ViewRotation.Yaw, InViewYawMin, InViewYawMax);
	ViewRotation.Yaw = FRotator::ClampAxis(ViewRotation.Yaw);
}

void APlayerCameraManager::DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
	DisplayDebugManager.SetDrawColor(FColor(255, 255, 255));
	DisplayDebugManager.DrawString(FString::Printf(TEXT("   Camera Style:%s main ViewTarget:%s"), *CameraStyle.ToString(), *ViewTarget.Target->GetName()));
	DisplayDebugManager.DrawString(FString::Printf(TEXT("   CamLoc:%s CamRot:%s FOV:%f"), *CameraCache.POV.Location.ToCompactString(), *CameraCache.POV.Rotation.ToCompactString(), CameraCache.POV.FOV));
	DisplayDebugManager.DrawString(FString::Printf(TEXT("   AspectRatio: %1.3f"), CameraCache.POV.AspectRatio));
}

void APlayerCameraManager::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);

	CameraCache.POV.Location+= InOffset;
	LastFrameCameraCache.POV.Location+= InOffset;
	
	ViewTarget.POV.Location+= InOffset;
	PendingViewTarget.POV.Location+= InOffset;

	CameraCache.POV.Location.DiagnosticCheckNaN(TEXT("APlayerCameraManager::ApplyWorldOffset: CameraCache.POV.Location"));
	LastFrameCameraCache.POV.Location.DiagnosticCheckNaN(TEXT("APlayerCameraManager::ApplyWorldOffset: LastFrameCameraCache.POV.Location"));
	ViewTarget.POV.Location.DiagnosticCheckNaN(TEXT("APlayerCameraManager::ApplyWorldOffset: ViewTarget.POV.Location"));
	PendingViewTarget.POV.Location.DiagnosticCheckNaN(TEXT("APlayerCameraManager::ApplyWorldOffset: PendingViewTarget.POV.Location"));
}

AEmitterCameraLensEffectBase* APlayerCameraManager::FindCameraLensEffect(TSubclassOf<AEmitterCameraLensEffectBase> LensEffectEmitterClass)
{
	for (int32 i = 0; i < CameraLensEffects.Num(); ++i)
	{
		AEmitterCameraLensEffectBase* LensEffect = CameraLensEffects[i];
		if ( !LensEffect->IsPendingKill() &&
			( (LensEffect->GetClass() == LensEffectEmitterClass) ||
			(LensEffect->EmittersToTreatAsSame.Find(LensEffectEmitterClass) != INDEX_NONE) ||
			(GetDefault<AEmitterCameraLensEffectBase>(LensEffectEmitterClass)->EmittersToTreatAsSame.Find(LensEffect->GetClass()) != INDEX_NONE ) ) )
		{
			return LensEffect;
		}
	}

	return NULL;
}


AEmitterCameraLensEffectBase* APlayerCameraManager::AddCameraLensEffect(TSubclassOf<AEmitterCameraLensEffectBase> LensEffectEmitterClass)
{
	if (LensEffectEmitterClass != NULL)
	{
		AEmitterCameraLensEffectBase* LensEffect = NULL;
		if (!GetDefault<AEmitterCameraLensEffectBase>(LensEffectEmitterClass)->bAllowMultipleInstances)
		{
			LensEffect = FindCameraLensEffect(LensEffectEmitterClass);

			if (LensEffect != NULL)
			{
				LensEffect->NotifyRetriggered();
			}
		}

		if (LensEffect == NULL)
		{
			// spawn with viewtarget as the owner so bOnlyOwnerSee works as intended
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.Owner = PCOwner->GetViewTarget();
			SpawnInfo.Instigator = Instigator;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnInfo.ObjectFlags |= RF_Transient;	// We never want to save these into a map
			
			AEmitterCameraLensEffectBase const* const EmitterCDO = LensEffectEmitterClass->GetDefaultObject<AEmitterCameraLensEffectBase>();
			FVector CamLoc;
			FRotator CamRot;
			GetCameraViewPoint(CamLoc, CamRot);
			FTransform SpawnTransform = AEmitterCameraLensEffectBase::GetAttachedEmitterTransform(EmitterCDO, CamLoc, CamRot, GetFOVAngle());
			
			LensEffect = GetWorld()->SpawnActor<AEmitterCameraLensEffectBase>(LensEffectEmitterClass, SpawnTransform, SpawnInfo);
			if (LensEffect != NULL)
			{
				LensEffect->RegisterCamera(this);
				CameraLensEffects.Add(LensEffect);
			}
		}
		
		return LensEffect;
	}

	return NULL;
}

void APlayerCameraManager::RemoveCameraLensEffect(AEmitterCameraLensEffectBase* Emitter)
{
	CameraLensEffects.Remove(Emitter);
}

void APlayerCameraManager::ClearCameraLensEffects()
{
	for (int32 i = 0; i < CameraLensEffects.Num(); ++i)
	{
		CameraLensEffects[i]->Destroy();
	}

	// empty the array.  unnecessary, since destruction will call RemoveCameraLensEffect,
	// but this gets it done in one fell swoop.
	CameraLensEffects.Empty();
}

/** ------------------------------------------------------------
 *  Camera Shakes
 *  ------------------------------------------------------------ */

UCameraShake* APlayerCameraManager::PlayCameraShake(TSubclassOf<UCameraShake> ShakeClass, float Scale, ECameraAnimPlaySpace::Type PlaySpace, FRotator UserPlaySpaceRot)
{
	if (ShakeClass && CachedCameraShakeMod && (Scale > 0.0f) )
	{
		return CachedCameraShakeMod->AddCameraShake(ShakeClass, Scale, PlaySpace, UserPlaySpaceRot);
	}

	return nullptr;
}


void APlayerCameraManager::StopCameraShake(UCameraShake* ShakeInst, bool bImmediately)
{
	if (ShakeInst && CachedCameraShakeMod)
	{
		CachedCameraShakeMod->RemoveCameraShake(ShakeInst, bImmediately);
	}
}

void APlayerCameraManager::StopAllInstancesOfCameraShake(TSubclassOf<class UCameraShake> ShakeClass, bool bImmediately)
{
	if (ShakeClass && CachedCameraShakeMod)
	{
		CachedCameraShakeMod->RemoveAllCameraShakesOfClass(ShakeClass, bImmediately);
	}
}

void APlayerCameraManager::StopAllCameraShakes(bool bImmediately)
{
	if (CachedCameraShakeMod)
	{
		CachedCameraShakeMod->RemoveAllCameraShakes(bImmediately);
	}
}

float APlayerCameraManager::CalcRadialShakeScale(APlayerCameraManager* Camera, FVector Epicenter, float InnerRadius, float OuterRadius, float Falloff)
{
	// using camera location so stuff like spectator cameras get shakes applied sensibly as well
	// need to ensure server has reasonably accurate camera position
	FVector POVLoc = Camera->GetCameraLocation();

	if (InnerRadius < OuterRadius)
	{
		float DistPct = ((Epicenter - POVLoc).Size() - InnerRadius) / (OuterRadius - InnerRadius);
		DistPct = 1.f - FMath::Clamp(DistPct, 0.f, 1.f);
		return FMath::Pow(DistPct, Falloff);
	}
	else
	{
		// ignore OuterRadius and do a cliff falloff at InnerRadius
		return ((Epicenter - POVLoc).SizeSquared() < FMath::Square(InnerRadius)) ? 1.f : 0.f;
	}
}


void APlayerCameraManager::PlayWorldCameraShake(UWorld* InWorld, TSubclassOf<class UCameraShake> Shake, FVector Epicenter, float InnerRadius, float OuterRadius, float Falloff, bool bOrientShakeTowardsEpicenter )
{
	for( FConstPlayerControllerIterator Iterator = InWorld->GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		APlayerController* PlayerController = Iterator->Get();
		if (PlayerController && PlayerController->PlayerCameraManager != NULL)
		{
			float ShakeScale = CalcRadialShakeScale(PlayerController->PlayerCameraManager, Epicenter, InnerRadius, OuterRadius, Falloff);

			if (bOrientShakeTowardsEpicenter && PlayerController->GetPawn() != NULL)
			{
				FVector CamLoc;
				FRotator CamRot;
				PlayerController->PlayerCameraManager->GetCameraViewPoint(CamLoc, CamRot);
				PlayerController->ClientPlayCameraShake(Shake, ShakeScale, ECameraAnimPlaySpace::UserDefined, (Epicenter - CamLoc).Rotation());
			}
			else
			{
				PlayerController->ClientPlayCameraShake(Shake, ShakeScale);
			}
		}
	}
}


/** ------------------------------------------------------------
 *  Camera fades
 *  ------------------------------------------------------------ */

void APlayerCameraManager::StartCameraFade(float FromAlpha, float ToAlpha, float InFadeTime, FLinearColor InFadeColor, bool bInFadeAudio, bool bInHoldWhenFinished)
{
	bEnableFading = true;

	FadeColor = InFadeColor;
	FadeAlpha = FVector2D(FromAlpha, ToAlpha);
	FadeTime = InFadeTime;
	FadeTimeRemaining = InFadeTime;
	bFadeAudio = bInFadeAudio;

	bAutoAnimateFade = true;
	bHoldFadeWhenFinished = bInHoldWhenFinished;
}

void APlayerCameraManager::StopCameraFade()
{
	if (bEnableFading == true)
	{
		// Make sure FadeAmount finishes at the desired value
		FadeAmount = FadeAlpha.Y;
		bEnableFading = false;
		StopAudioFade();
	}
}

void APlayerCameraManager::SetManualCameraFade(float InFadeAmount, FLinearColor Color, bool bInFadeAudio)
{
	bEnableFading = true;
	FadeColor = Color;
	FadeAmount = InFadeAmount;
	bFadeAudio = bInFadeAudio;

	bAutoAnimateFade = false;
	StopAudioFade();
	FadeTimeRemaining = 0.0f;
}


//////////////////////////////////////////////////////////////////////////
// FTViewTarget

void FTViewTarget::SetNewTarget(AActor* NewTarget)
{
	Target = NewTarget;
}

APawn* FTViewTarget::GetTargetPawn() const
{
	if (APawn* Pawn = Cast<APawn>(Target))
	{
		return Pawn;
	}
	else if (AController* Controller = Cast<AController>(Target))
	{
		return Controller->GetPawn();
	}
	else
	{
		return NULL;
	}
}

bool FTViewTarget::Equal(const FTViewTarget& OtherTarget) const
{
	//@TODO: Should I compare Controller too?
	return (Target == OtherTarget.Target) && (PlayerState == OtherTarget.PlayerState) && POV.Equals(OtherTarget.POV);
}


void FTViewTarget::CheckViewTarget(APlayerController* OwningController)
{
	check(OwningController);

	if (Target == NULL)
	{
		Target = OwningController;
	}

	// Update ViewTarget PlayerState (used to follow same player through pawn transitions, etc., when spectating)
	if (Target == OwningController) 
	{	
		PlayerState = NULL;
	}
	else if (AController* TargetAsController = Cast<AController>(Target))
	{
		PlayerState = TargetAsController->PlayerState;
	}
	else if (APawn* TargetAsPawn = Cast<APawn>(Target))
	{
		PlayerState = TargetAsPawn->PlayerState;
	}
	else if (APlayerState* TargetAsPlayerState = Cast<APlayerState>(Target))
	{
		PlayerState = TargetAsPlayerState;
	}
	else
	{
		PlayerState = NULL;
	}

	if ((PlayerState != NULL) && !PlayerState->IsPendingKill())
	{
		if ((Target == NULL) || Target->IsPendingKill() || !Cast<APawn>(Target) || (Cast<APawn>(Target)->PlayerState != PlayerState) )
		{
			Target = NULL;

			// not viewing pawn associated with VT.PlayerState, so look for one
			// Assuming on server, so PlayerState Owner is valid
			if (PlayerState->GetOwner() == NULL)
			{
				PlayerState = NULL;
			}
			else
			{
				if (AController* PlayerStateOwner = Cast<AController>(PlayerState->GetOwner()))
				{
					AActor* PlayerStateViewTarget = PlayerStateOwner->GetPawn();
					if( PlayerStateViewTarget && !PlayerStateViewTarget->IsPendingKill() )
					{
						OwningController->PlayerCameraManager->AssignViewTarget(PlayerStateViewTarget, *this);
					}
					else
					{
						Target = PlayerState; // this will cause it to update to the next Pawn possessed by the player being viewed
					}
				}
				else
				{
					PlayerState = NULL;
				}
			}
		}
	}

	if ((Target == NULL) || Target->IsPendingKill())
	{
		if (OwningController->GetPawn() && !OwningController->GetPawn()->IsPendingKillPending() )
		{
			OwningController->PlayerCameraManager->AssignViewTarget(OwningController->GetPawn(), *this);
		}
		else
		{
			OwningController->PlayerCameraManager->AssignViewTarget(OwningController, *this);
		}
	}
}

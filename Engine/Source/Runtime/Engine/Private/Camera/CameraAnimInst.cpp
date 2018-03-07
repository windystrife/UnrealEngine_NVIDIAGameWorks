// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Camera/CameraAnimInst.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraAnim.h"
#include "Matinee/InterpTrackMove.h"
#include "Matinee/InterpGroup.h"
#include "Matinee/InterpGroupInst.h"
#include "Matinee/InterpTrackInstMove.h"
#include "Matinee/InterpTrackFloatProp.h"

//////////////////////////////////////////////////////////////////////////
// UCameraAnimInst

UCameraAnimInst::UCameraAnimInst(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bFinished = true;
	bStopAutomatically = true;
	PlayRate = 1.0f;
	TransientScaleModifier = 1.0f;
	PlaySpace = ECameraAnimPlaySpace::CameraLocal;

	InterpGroupInst = CreateDefaultSubobject<UInterpGroupInst>(TEXT("InterpGroupInst0"));
}

void UCameraAnimInst::AdvanceAnim(float DeltaTime, bool bJump)
{
	// check to see if our animnodeseq has been deleted.  not a fan of 
	// polling for this, but we want to stop this immediately, not when
	// GC gets around to cleaning up.
	if ( (CamAnim != NULL) && !bFinished )
	{
		// will set to true if anim finishes this frame
		bool bAnimJustFinished = false;

		float const ScaledDeltaTime = DeltaTime * PlayRate;

		// find new times
		CurTime += ScaledDeltaTime;
		if (bBlendingIn)
		{
			CurBlendInTime += DeltaTime;
		}
		if (bBlendingOut)
		{
			CurBlendOutTime += DeltaTime;
		}

		// see if we've crossed any important time thresholds and deal appropriately
		if (bLooping)
		{
			if (CurTime > CamAnim->AnimLength)
			{
				// loop back to the beginning
				CurTime -= CamAnim->AnimLength;
			}
		}
		else
		{
			if (CurTime > CamAnim->AnimLength)
			{
				// done!!
				bAnimJustFinished = true;
			}
			else if (CurTime > (CamAnim->AnimLength - BlendOutTime))
			{
				// start blending out
				bBlendingOut = true;
				CurBlendOutTime = CurTime - (CamAnim->AnimLength - BlendOutTime);
			}
		}

		if (bBlendingIn)
		{
			if ((CurBlendInTime > BlendInTime) || (BlendInTime == 0.f))
			{
				// done blending in!
				bBlendingIn = false;
			}
		}
		if (bBlendingOut)
		{
			if (CurBlendOutTime > BlendOutTime)
			{
				// done!!
				CurBlendOutTime = BlendOutTime;
				bAnimJustFinished = true;
			}
		}
		
		// calculate blend weight. calculating separately and taking the minimum handles overlapping blends nicely.
		{
			float const BlendInWeight = (bBlendingIn) ? (CurBlendInTime / BlendInTime) : 1.f;
			float const BlendOutWeight = (bBlendingOut) ? (1.f - CurBlendOutTime / BlendOutTime) : 1.f;
			CurrentBlendWeight = FMath::Min(BlendInWeight, BlendOutWeight) * BasePlayScale * TransientScaleModifier;
			
			// this is intended to be applied only once
			TransientScaleModifier = 1.f;
		}

		// this will update tracks and apply the effects to the group actor (except move tracks)
		if (InterpGroupInst->Group)
		{
			InterpGroupInst->Group->UpdateGroup(CurTime, InterpGroupInst, false, bJump);
		}

		if (bStopAutomatically)
		{
			if (bAnimJustFinished)
			{
				// completely finished
				Stop(true);
			}
			else if (RemainingTime > 0.f)
			{
				// handle any specified duration
				RemainingTime -= DeltaTime;
				if (RemainingTime <= 0.f)
				{
					// stop with blend out
					Stop();
				}
			}
		}
	}
}

void UCameraAnimInst::SetCurrentTime(float NewTime)
{
	float const TimeDelta = NewTime - (CurTime / PlayRate);
	AdvanceAnim(TimeDelta, true);
}


void UCameraAnimInst::Update(float NewRate, float NewScale, float NewBlendInTime, float NewBlendOutTime, float NewDuration)
{
	if (bFinished == false)
	{
		if (bBlendingOut)
		{
			bBlendingOut = false;
			CurBlendOutTime = 0.f;

			// stop any blendout and reverse it to a blendin
			bBlendingIn = true;
			CurBlendInTime = NewBlendInTime * (1.f - CurBlendOutTime / BlendOutTime);
		}

		PlayRate = NewRate;
		BasePlayScale = NewScale;
		BlendInTime = NewBlendInTime;
		BlendOutTime = NewBlendOutTime;
		RemainingTime = (NewDuration > 0.f) ? (NewDuration - BlendOutTime) : 0.f;
	}
}

void UCameraAnimInst::SetDuration(float NewDuration)
{
	if (bFinished == false)
	{
		// setting a new duration will reset the play timer back to 0 but maintain current playback position
		// if blending out, stop it and blend back in to reverse it so it's smooth
		if (bBlendingOut)
		{
			bBlendingOut = false;
			CurBlendOutTime = 0.f;

			// stop any blendout and reverse it to a blendin
			bBlendingIn = true;
			CurBlendInTime = BlendInTime * (1.f - CurBlendOutTime / BlendOutTime);
		}

		RemainingTime = (NewDuration > 0.f) ? (NewDuration - BlendOutTime) : 0.f;
	}
	else
	{
		UE_LOG(LogCameraAnim, Warning, TEXT("SetDuration called for CameraAnim %s after it finished. Ignored."), *GetNameSafe(CamAnim));
	}
}

void UCameraAnimInst::SetScale(float NewScale)
{
	BasePlayScale = NewScale;
}
static const FName NAME_CameraComponentFieldOfViewPropertyName(TEXT("CameraComponent.FieldOfView"));

void UCameraAnimInst::Play(UCameraAnim* Anim, class AActor* CamActor, float InRate, float InScale, float InBlendInTime, float InBlendOutTime, bool bInLooping, bool bRandomStartTime, float Duration)
{
	check(IsInGameThread());

	if (Anim && Anim->CameraInterpGroup)
	{
		// make sure any previous anim has been terminated correctly
		Stop(true);

		CurTime = bRandomStartTime ? (FMath::FRand() * Anim->AnimLength) : 0.f;
		CurBlendInTime = 0.f;
		CurBlendOutTime = 0.f;
		bBlendingIn = true;
		bBlendingOut = false;
		bFinished = false;

		// copy properties
		CamAnim = Anim;
		PlayRate = InRate;
		BasePlayScale = InScale;
		BlendInTime = InBlendInTime;
		BlendOutTime = InBlendOutTime;
		bLooping = bInLooping;
		RemainingTime = (Duration > 0.f) ? (Duration - BlendOutTime) : 0.f;

		// init the interpgroup
		if ( CamActor && CamActor->IsA(ACameraActor::StaticClass()) )
		{
			// #fixme jf: I don't think this is necessary anymore
			// ensure CameraActor is zeroed, so RelativeToInitial anims get proper InitialTM
			CamActor->SetActorLocation(FVector::ZeroVector, false);
			CamActor->SetActorRotation(FRotator::ZeroRotator);
		}
		InterpGroupInst->InitGroupInst(CamAnim->CameraInterpGroup, CamActor);

		// cache move track refs
		for (int32 Idx = 0; Idx < InterpGroupInst->TrackInst.Num(); ++Idx)
		{
			MoveTrack = Cast<UInterpTrackMove>(CamAnim->CameraInterpGroup->InterpTracks[Idx]);
			if (MoveTrack != NULL)
			{
				MoveInst = CastChecked<UInterpTrackInstMove>(InterpGroupInst->TrackInst[Idx]);
				// only 1 move track per group, so we can bail here
				break;					
			}
		}

		// store initial transform so we can treat camera movements as offsets relative to the initial anim key
		if (MoveTrack && MoveInst)
		{
			FRotator OutRot;
			FVector OutLoc;
			MoveTrack->GetKeyTransformAtTime(MoveInst, 0.f, OutLoc, OutRot);
			InitialCamToWorld = FTransform(OutRot, OutLoc);		// @todo, store inverted since that's how we use it?

			// find FOV track if it exists, else just use the fov saved in the anim
			if (Anim->bRelativeToInitialFOV)
			{
				InitialFOV = Anim->BaseFOV;
			}
			for (int32 Idx = 0; Idx < InterpGroupInst->TrackInst.Num(); ++Idx)
			{
				UInterpTrackFloatProp* const FloatTrack = Cast<UInterpTrackFloatProp>(CamAnim->CameraInterpGroup->InterpTracks[Idx]);
				if (FloatTrack && (FloatTrack->PropertyName == NAME_CameraComponentFieldOfViewPropertyName))
				{
					if (Anim->bRelativeToInitialFOV)
					{
						InitialFOV = FloatTrack->EvalSub(0, 0.f);
					}
					bHasFOVTrack = true;
					break;
				}
			}
		}
		else
		{
			// make sure these are set in cases where there is no move track
			InitialCamToWorld = FTransform::Identity;
			InitialFOV = Anim->BaseFOV;
		}
	}
}

void UCameraAnimInst::Stop(bool bImmediate)
{
	check(IsInGameThread());

	if ( bImmediate || (BlendOutTime <= 0.f) )
	{
		if (InterpGroupInst->Group != nullptr)
		{
			InterpGroupInst->TermGroupInst(true);
			InterpGroupInst->Group = nullptr;
		}
		MoveTrack = nullptr;
		MoveInst = nullptr;
		bFinished = true;
 	}
	else if (bBlendingOut == false)
	{
		// start blend out if not already blending out
		bBlendingOut = true;
		CurBlendOutTime = 0.f;
	}
}

void UCameraAnimInst::ApplyTransientScaling(float Scalar)
{
	TransientScaleModifier *= Scalar;
}

void UCameraAnimInst::SetPlaySpace(ECameraAnimPlaySpace::Type NewSpace, FRotator UserPlaySpace)
{
	PlaySpace = NewSpace;
	UserPlaySpaceMatrix = (PlaySpace == ECameraAnimPlaySpace::UserDefined) ? FRotationMatrix(UserPlaySpace) : FMatrix::Identity;
}


void UCameraAnimInst::ApplyToView(FMinimalViewInfo& InOutPOV) const
{
	if (CurrentBlendWeight > 0.f)
	{
		ACameraActor const* AnimatedCamActor = dynamic_cast<ACameraActor*>(InterpGroupInst->GetGroupActor());
		if (AnimatedCamActor)
		{

			if (CamAnim->bRelativeToInitialTransform)
			{
				// move animated cam actor to initial-relative position
				FTransform const AnimatedCamToWorld = AnimatedCamActor->GetTransform();
				FTransform const AnimatedCamToInitialCam = AnimatedCamToWorld * InitialCamToWorld.Inverse();
				ACameraActor* const MutableCamActor = const_cast<ACameraActor*>(AnimatedCamActor);
				MutableCamActor->SetActorTransform(AnimatedCamToInitialCam);
			}

			float const Scale = CurrentBlendWeight;
			FRotationMatrix const CameraToWorld(InOutPOV.Rotation);

			if (PlaySpace == ECameraAnimPlaySpace::CameraLocal)
			{
				// the code in the else block will handle this just fine, but this path provides efficiency and simplicity for the most common case

				// loc
				FVector const LocalOffset = CameraToWorld.TransformVector(AnimatedCamActor->GetActorLocation()*Scale);
				InOutPOV.Location += LocalOffset;

				// rot
				FRotationMatrix const AnimRotMat(AnimatedCamActor->GetActorRotation()*Scale);
				InOutPOV.Rotation = (AnimRotMat * CameraToWorld).Rotator();
			}
			else
			{
				// handle playing the anim in an arbitrary space relative to the camera

				// find desired space
				FMatrix const PlaySpaceToWorld = (PlaySpace == ECameraAnimPlaySpace::UserDefined) ? UserPlaySpaceMatrix : FMatrix::Identity;

				// loc
				FVector const LocalOffset = PlaySpaceToWorld.TransformVector(AnimatedCamActor->GetActorLocation()*Scale);
				InOutPOV.Location += LocalOffset;

				// rot
				// find transform from camera to the "play space"
				FMatrix const CameraToPlaySpace = CameraToWorld * PlaySpaceToWorld.Inverse();	// CameraToWorld * WorldToPlaySpace

				// find transform from anim (applied in playspace) back to camera
				FRotationMatrix const AnimToPlaySpace(AnimatedCamActor->GetActorRotation()*Scale);
				FMatrix const AnimToCamera = AnimToPlaySpace * CameraToPlaySpace.Inverse();			// AnimToPlaySpace * PlaySpaceToCamera

				// RCS = rotated camera space, meaning camera space after it's been animated
				// this is what we're looking for, the diff between rotated cam space and regular cam space.
				// apply the transform back to camera space from the post-animated transform to get the RCS
				FMatrix const RCSToCamera = CameraToPlaySpace * AnimToCamera;

				// now apply to real camera
				FRotationMatrix const RealCamToWorld(InOutPOV.Rotation);
				InOutPOV.Rotation = (RCSToCamera * RealCamToWorld).Rotator();
			}

			// fov
			if (bHasFOVTrack)
			{
				const float FOVMin = 5.f;
				const float FOVMax = 170.f;

				// Interp the FOV toward the camera component's FOV based on Scale
				if (CamAnim->bRelativeToInitialFOV)
				{
					InOutPOV.FOV += (AnimatedCamActor->GetCameraComponent()->FieldOfView - InitialFOV) * Scale;
				}
				else
				{
					const int32 DesiredDirection = FMath::Sign(AnimatedCamActor->GetCameraComponent()->FieldOfView - InOutPOV.FOV);
					const int32 InitialDirection = FMath::Sign(AnimatedCamActor->GetCameraComponent()->FieldOfView - InitialFOV);
					if (DesiredDirection != InitialDirection)
					{
						InOutPOV.FOV = FMath::Clamp(InOutPOV.FOV + ((AnimatedCamActor->GetCameraComponent()->FieldOfView - InOutPOV.FOV) * Scale), InOutPOV.FOV, AnimatedCamActor->GetCameraComponent()->FieldOfView);
					}
					else
					{
						InOutPOV.FOV = FMath::Clamp(InOutPOV.FOV + ((AnimatedCamActor->GetCameraComponent()->FieldOfView - InitialFOV) * Scale), AnimatedCamActor->GetCameraComponent()->FieldOfView, InitialFOV);
					}
				}
				InOutPOV.FOV = FMath::Clamp<float>(InOutPOV.FOV, FOVMin, FOVMax);
			}
		}
	}
}


// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneCameraAnimTemplate.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraAnim.h"
#include "Camera/CameraAnimInst.h"
#include "Engine/World.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneEvaluation.h"
#include "UObject/Package.h"
#include "IMovieScenePlayer.h"


DECLARE_CYCLE_STAT(TEXT("Camera Anim Track Token Execute"), MovieSceneEval_CameraAnimTrack_TokenExecute, STATGROUP_MovieSceneEval);

/** Structure that holds blended post processing settings */
struct FBlendedPostProcessSettings : FPostProcessSettings
{
	FBlendedPostProcessSettings() : Weight(0.f) {}
	FBlendedPostProcessSettings(float InWeight, const FPostProcessSettings& InSettings) : FPostProcessSettings(InSettings), Weight(InWeight) {}

	/** The weighting to apply to these settings */
	float Weight;
};


/** Persistent data that exists as long as a given camera track is being evaluated */
struct FMovieSceneAdditiveCameraData : IPersistentEvaluationData
{
	static FMovieSceneAdditiveCameraData& Get(const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData)
	{
		return PersistentData.GetOrAdd<FMovieSceneAdditiveCameraData>(FSharedPersistentDataKey(FMovieSceneAdditiveCameraAnimationTrackTemplate::SharedDataId, Operand));
	}

	/** Reset the additive camera values */
	void Reset()
	{
		TotalFOVOffset = 0.f;
		TotalTransform = FTransform::Identity;
		BlendedPostProcessSettings.Reset();

		bApplyTransform = false;
		bApplyPostProcessing = false;
	}

	/** Accumulate the given post processing settings for this frame */
	void AccumulatePostProcessing(const FPostProcessSettings& InPostProcessSettings, float Weight)
	{
		if (Weight > 0.f)
		{
			BlendedPostProcessSettings.Add(FBlendedPostProcessSettings(Weight, InPostProcessSettings));
		}

		bApplyPostProcessing = true;
	}

	/** Accumulate the transform and FOV offset */
	void AccumulateOffset(const FTransform& AdditiveOffset, float AdditiveFOVOffset)
	{
		TotalTransform = TotalTransform * AdditiveOffset;
		TotalFOVOffset += AdditiveFOVOffset;

		bApplyTransform = true;
	}

	/** Apply any cumulative animation states */
	void ApplyCumulativeAnimation(UCameraComponent& CameraComponent) const
	{
		if (bApplyPostProcessing)
		{
			CameraComponent.ClearExtraPostProcessBlends();
			for (const FBlendedPostProcessSettings& Settings : BlendedPostProcessSettings)
			{
				CameraComponent.AddExtraPostProcessBlend(Settings, Settings.Weight);
			}
		}

		if (bApplyTransform)
		{
			CameraComponent.ClearAdditiveOffset();
			CameraComponent.AddAdditiveOffset(TotalTransform, TotalFOVOffset);
		}
	}

	ACameraActor* GetTempCameraActor(IMovieScenePlayer& Player)
	{
		if (!TempCameraActor.IsValid())
		{
			// spawn the temp CameraActor used for updating CameraAnims
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			// We never want to save these temp actors into a map
			SpawnInfo.ObjectFlags |= RF_Transient;
			ACameraActor* Cam = Player.GetPlaybackContext()->GetWorld()->SpawnActor<ACameraActor>(SpawnInfo);
			if (Cam)
			{
#if WITH_EDITOR
				Cam->SetIsTemporarilyHiddenInEditor(true);
#endif
				TempCameraActor = Cam;

				struct FDestroyTempObject : IMovieScenePreAnimatedGlobalToken
				{
					FDestroyTempObject(AActor& InActor) : TempActor(&InActor) {}

					virtual void RestoreState(IMovieScenePlayer& InPlayer)
					{
						AActor* Actor = TempActor.Get();
						if (Actor)
						{
							Actor->Destroy(false, false);
						}
					}

					TWeakObjectPtr<AActor> TempActor;
				};

				Player.SavePreAnimatedState(*Cam, FMovieSceneAnimTypeID::Unique(), FTempCameraPreAnimatedStateProducer());
			}
		}

		return TempCameraActor.Get();
	}

private:

	struct FTempCameraPreAnimatedStateProducer : IMovieScenePreAnimatedTokenProducer
	{
		virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
		{
			struct FTempCameraPreAnimatedState : IMovieScenePreAnimatedToken
			{
				virtual void RestoreState(UObject& InObject, IMovieScenePlayer& InPlayer)
				{
					AActor* Actor = CastChecked<AActor>(&InObject);
					Actor->Destroy(false, false);
				}
			};

			return FTempCameraPreAnimatedState();
		}
	};

	bool bApplyTransform, bApplyPostProcessing;
	TArray<FBlendedPostProcessSettings, TInlineAllocator<2>> BlendedPostProcessSettings;
	FTransform TotalTransform;
	float TotalFOVOffset;
	TWeakObjectPtr<ACameraActor> TempCameraActor;
};

/** Pre animated token that restores a camera component's additive transform */
struct FPreAnimatedCameraTransformTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
	{
		struct FRestoreToken : IMovieScenePreAnimatedToken
		{
			virtual void RestoreState(UObject& InObject, IMovieScenePlayer& Player) override
			{
				UCameraComponent* CameraComponent = CastChecked<UCameraComponent>(&InObject);
				CameraComponent->ClearAdditiveOffset();
			}
		};

		return FRestoreToken();
	}
};

/** Pre animated token that restores a camera component's blended post processing settings */
struct FPreAnimatedPostProcessingBlendsTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
	{
		struct FRestoreToken : IMovieScenePreAnimatedToken
		{
			virtual void RestoreState(UObject& InObject, IMovieScenePlayer& Player) override
			{
				UCameraComponent* CameraComponent = CastChecked<UCameraComponent>(&InObject);
				CameraComponent->ClearExtraPostProcessBlends();
			}
		};

		return FRestoreToken();
	}
};

/** A movie scene execution token that applies camera cuts */
struct FApplyCameraAnimExecutionToken : IMovieSceneExecutionToken
{
	static FMovieSceneAnimTypeID GetAnimTypeID()
	{
		return TMovieSceneAnimTypeID<FApplyCameraAnimExecutionToken>();
	}

	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		FMovieSceneAdditiveCameraData& SharedData = FMovieSceneAdditiveCameraData::Get(Operand, PersistentData);

		for (TWeakObjectPtr<> ObjectWP : Player.FindBoundObjects(Operand))
		{
			UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromRuntimeObject(ObjectWP.Get());
			if (CameraComponent)
			{
				SharedData.ApplyCumulativeAnimation(*CameraComponent);
			}
		}
	}
};

const FMovieSceneSharedDataId FMovieSceneAdditiveCameraAnimationTrackTemplate::SharedDataId = FMovieSceneSharedDataId::Allocate();

FMovieSceneAdditiveCameraAnimationTrackTemplate::FMovieSceneAdditiveCameraAnimationTrackTemplate()
{
}

void FMovieSceneAdditiveCameraAnimationTrackTemplate::Initialize(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	FMovieSceneAdditiveCameraData::Get(Operand, PersistentData).Reset();
}

void FMovieSceneAdditiveCameraAnimationTrackTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	// Add a final execution token that will apply the blended result of anything added to our shared data
	ExecutionTokens.Add(FApplyCameraAnimExecutionToken());
}

/** A movie scene execution token that applies camera animations */
struct FAccumulateCameraAnimExecutionToken : IMovieSceneExecutionToken
{
	FAccumulateCameraAnimExecutionToken(const FMovieSceneAdditiveCameraAnimationTemplate* InTemplate) : Template(InTemplate) {}

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		if (!Template->EnsureSetup(Operand, PersistentData, Player))
		{
			return;
		}

		FMovieSceneAdditiveCameraData& SharedData = FMovieSceneAdditiveCameraData::Get(Operand, PersistentData);

		ACameraActor* TempCameraActor = SharedData.GetTempCameraActor(Player);
		if (!ensure(TempCameraActor))
		{
			return;
		}

		for (TWeakObjectPtr<> ObjectWP : Player.FindBoundObjects(Operand))
		{
			UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromRuntimeObject(ObjectWP.Get());
			if (!CameraComponent)
			{
				continue;
			}

			FMinimalViewInfo POV;
			POV.Location = CameraComponent->GetComponentLocation();
			POV.Rotation = CameraComponent->GetComponentRotation();
			POV.FOV = CameraComponent->FieldOfView;

			if (!Template->UpdateCamera(*TempCameraActor, POV, Context, PersistentData))
			{
				continue;
			}

			FTransform WorldToBaseCamera = CameraComponent->GetComponentToWorld().Inverse();
			float BaseFOV = CameraComponent->FieldOfView;
			FTransform NewCameraToWorld(POV.Rotation, POV.Location);
			float NewFOV = POV.FOV;

			FTransform NewCameraToBaseCamera = NewCameraToWorld * WorldToBaseCamera;

			float NewFOVToBaseFOV = BaseFOV - NewFOV;

			{
				static FMovieSceneAnimTypeID TransformAnimTypeID = TMovieSceneAnimTypeID<FAccumulateCameraAnimExecutionToken, 0>();
				Player.SavePreAnimatedState(*CameraComponent, TransformAnimTypeID, FPreAnimatedCameraTransformTokenProducer());

				// Accumumulate the offsets into the track data for application as part of the track execution token
				SharedData.AccumulateOffset(NewCameraToBaseCamera, NewFOVToBaseFOV);
			}

			// harvest PP changes
			{
				UCameraComponent* AnimCamComp = TempCameraActor->GetCameraComponent();
				if (AnimCamComp && AnimCamComp->PostProcessBlendWeight > 0.f)
				{
					static FMovieSceneAnimTypeID PostAnimTypeID = TMovieSceneAnimTypeID<FAccumulateCameraAnimExecutionToken, 1>();
					Player.SavePreAnimatedState(*CameraComponent, PostAnimTypeID, FPreAnimatedPostProcessingBlendsTokenProducer());

					SharedData.AccumulatePostProcessing(AnimCamComp->PostProcessSettings, AnimCamComp->PostProcessBlendWeight);
				}
			}
		}
	}

	const FMovieSceneAdditiveCameraAnimationTemplate* Template;
};

void FMovieSceneAdditiveCameraAnimationTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	ExecutionTokens.Add(FAccumulateCameraAnimExecutionToken(this));
}

/** Persistent data that exists as long as a given camera anim section is being evaluated */
struct FMovieSceneCameraAnimSectionInstanceData : IPersistentEvaluationData
{
	TWeakObjectPtr<UCameraAnimInst> CameraAnimInst;
};

/** Pre animated token that restores a camera animation */
struct FPreAnimatedCameraAnimTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
	{
		struct FRestoreToken : IMovieScenePreAnimatedToken
		{
			virtual void RestoreState(UObject& InObject, IMovieScenePlayer& Player) override
			{
				UCameraAnimInst* CameraAnim = CastChecked<UCameraAnimInst>(&InObject);
				CameraAnim->Stop(true);
				CameraAnim->RemoveFromRoot();
			}
		};

		return FRestoreToken();
	}
};

FMovieSceneCameraAnimSectionTemplate::FMovieSceneCameraAnimSectionTemplate(const UMovieSceneCameraAnimSection& Section)
	: SourceData(Section.AnimData)
	, SectionStartTime(Section.GetStartTime())
{
}

bool FMovieSceneCameraAnimSectionTemplate::EnsureSetup(const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	// Get the camera anim instance from the section data (local to this specific section)
	FMovieSceneCameraAnimSectionInstanceData& SectionData = PersistentData.GetOrAddSectionData<FMovieSceneCameraAnimSectionInstanceData>();
	UCameraAnimInst* CameraAnimInstance = SectionData.CameraAnimInst.Get();

	if (!CameraAnimInstance)
	{
		if (!SourceData.CameraAnim)
		{
			return false;
		}

		// Start playing the camera anim
		CameraAnimInstance = NewObject<UCameraAnimInst>(GetTransientPackage());
		if (ensure(CameraAnimInstance))
		{
			// make it root so GC doesn't take it away
			CameraAnimInstance->AddToRoot();
			CameraAnimInstance->SetStopAutomatically(false);

			// Store the anim instance with the section and always remove it when we've finished evaluating
			FMovieSceneAnimTypeID AnimTypeID = TMovieSceneAnimTypeID<FMovieSceneCameraAnimSectionTemplate>();
			Player.SavePreAnimatedState(*CameraAnimInstance, AnimTypeID, FPreAnimatedCameraAnimTokenProducer(), PersistentData.GetSectionKey());

			// We use the global temp actor from the shared data (shared across all additive camera effects for this operand)
			ACameraActor* TempCameraActor = FMovieSceneAdditiveCameraData::Get(Operand, PersistentData).GetTempCameraActor(Player);

			CameraAnimInstance->Play(SourceData.CameraAnim, TempCameraActor, SourceData.PlayRate, SourceData.PlayScale, SourceData.BlendInTime, SourceData.BlendOutTime, SourceData.bLooping, SourceData.bRandomStartTime);
		}

		SectionData.CameraAnimInst = CameraAnimInstance;
	}

	// If we failed to create the camera anim instance, we're doomed
	return ensure(CameraAnimInstance);
}

bool FMovieSceneCameraAnimSectionTemplate::UpdateCamera(ACameraActor& TempCameraActor, FMinimalViewInfo& POV, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData) const
{
	// Get the camera anim instance from the section data (local to this specific section)
	FMovieSceneCameraAnimSectionInstanceData& SectionData = PersistentData.GetOrAddSectionData<FMovieSceneCameraAnimSectionInstanceData>();
	UCameraAnimInst* CameraAnimInstance = SectionData.CameraAnimInst.Get();

	if (!CameraAnimInstance || !CameraAnimInstance->CamAnim)
	{
		return false;
	}

	// prepare temp camera actor by resetting it
	{
		TempCameraActor.SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);

		ACameraActor const* const DefaultCamActor = GetDefault<ACameraActor>();
		if (DefaultCamActor)
		{
			TempCameraActor.GetCameraComponent()->AspectRatio = DefaultCamActor->GetCameraComponent()->AspectRatio;
			TempCameraActor.GetCameraComponent()->PostProcessSettings = CameraAnimInstance->CamAnim->BasePostProcessSettings;
			TempCameraActor.GetCameraComponent()->PostProcessBlendWeight = CameraAnimInstance->CamAnim->BasePostProcessBlendWeight;
		}
	}
	
	// set camera anim to the correct time
	const float NewCameraAnimTime = Context.GetTime() - SectionStartTime;
	CameraAnimInstance->SetCurrentTime(NewCameraAnimTime);

	if (CameraAnimInstance->CurrentBlendWeight <= 0.f)
	{
		return false;
	}

	// harvest properties from the actor and apply
	CameraAnimInstance->ApplyToView(POV);

	return true;
}

/** Persistent data that exists as long as a given camera anim section is being evaluated */
struct FMovieSceneCameraShakeSectionInstanceData : IPersistentEvaluationData
{
	TWeakObjectPtr<UCameraShake> CameraShakeInst;
};

/** Pre animated token that restores a camera animation */
struct FPreAnimatedCameraShakeTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
	{
		struct FRestoreToken : IMovieScenePreAnimatedToken
		{
			virtual void RestoreState(UObject& InObject, IMovieScenePlayer& Player) override
			{
				UCameraShake* CameraShake = CastChecked<UCameraShake>(&InObject);
				CameraShake->StopShake(true);
				CameraShake->RemoveFromRoot();
			}
		};

		return FRestoreToken();
	}
};

FMovieSceneCameraShakeSectionTemplate::FMovieSceneCameraShakeSectionTemplate(const UMovieSceneCameraShakeSection& Section)
	: SourceData(Section.ShakeData)
	, SectionStartTime(Section.GetStartTime())
{
}

bool FMovieSceneCameraShakeSectionTemplate::EnsureSetup(const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	// Get the camera anim instance from the section data (local to this specific section)
	FMovieSceneCameraShakeSectionInstanceData& SectionData = PersistentData.GetOrAddSectionData<FMovieSceneCameraShakeSectionInstanceData>();
	UCameraShake* CameraShakeInstance = SectionData.CameraShakeInst.Get();

	if (!CameraShakeInstance)
	{
		if (!*SourceData.ShakeClass)
		{
			return false;
		}

		CameraShakeInstance = NewObject<UCameraShake>(GetTransientPackage(), SourceData.ShakeClass);
		if (CameraShakeInstance)
		{
			// Store the anim instance with the section and always remove it when we've finished evaluating
			FMovieSceneAnimTypeID AnimTypeID = TMovieSceneAnimTypeID<FMovieSceneCameraShakeSectionTemplate>();
			Player.SavePreAnimatedState(*CameraShakeInstance, AnimTypeID, FPreAnimatedCameraShakeTokenProducer(), PersistentData.GetSectionKey());

			// We use the global temp actor from the shared data (shared across all additive camera effects for this operand)
			ACameraActor* TempCameraActor = FMovieSceneAdditiveCameraData::Get(Operand, PersistentData).GetTempCameraActor(Player);

			// make it root so GC doesn't take it away
			CameraShakeInstance->AddToRoot();
			CameraShakeInstance->SetTempCameraAnimActor(TempCameraActor);
			CameraShakeInstance->PlayShake(nullptr, SourceData.PlayScale, SourceData.PlaySpace, SourceData.UserDefinedPlaySpace);
			if (CameraShakeInstance->AnimInst)
			{
				CameraShakeInstance->AnimInst->SetStopAutomatically(false);
			}
		}

		SectionData.CameraShakeInst = CameraShakeInstance;
	}

	// If we failed to create the camera anim instance, we're doomed
	return ensure(CameraShakeInstance);
}

bool FMovieSceneCameraShakeSectionTemplate::UpdateCamera(ACameraActor& TempCameraActor, FMinimalViewInfo& POV, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData) const
{
	// Get the camera anim instance from the section data (local to this specific section)
	FMovieSceneCameraShakeSectionInstanceData& SectionData = PersistentData.GetOrAddSectionData<FMovieSceneCameraShakeSectionInstanceData>();
	UCameraShake* CameraShakeInstance = SectionData.CameraShakeInst.Get();

	if (!ensure(CameraShakeInstance))
	{
		return false;
	}

	// prepare temp camera actor by resetting it
	{
		TempCameraActor.SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);

		ACameraActor const* const DefaultCamActor = GetDefault<ACameraActor>();
		if (DefaultCamActor)
		{
			TempCameraActor.GetCameraComponent()->AspectRatio = DefaultCamActor->GetCameraComponent()->AspectRatio;

			UCameraAnim* CamAnim = CameraShakeInstance->AnimInst ? CameraShakeInstance->AnimInst->CamAnim : nullptr;

			TempCameraActor.GetCameraComponent()->PostProcessSettings = CamAnim ? CamAnim->BasePostProcessSettings : FPostProcessSettings();
			TempCameraActor.GetCameraComponent()->PostProcessBlendWeight = CamAnim ? CameraShakeInstance->AnimInst->CamAnim->BasePostProcessBlendWeight : 0.f;
		}
	}
	
	// set camera anim to the correct time
	const float NewCameraAnimTime = Context.GetTime() - SectionStartTime;
	CameraShakeInstance->SetCurrentTimeAndApplyShake(NewCameraAnimTime, POV);

	return true;
}

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneAnimationSectionRecorder.h"
#include "AnimationRecorder.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "MovieScene.h"
#include "AssetRegistryModule.h"
#include "SequenceRecorderUtils.h"
#include "SequenceRecorderSettings.h"
#include "ActorRecording.h"

TSharedPtr<IMovieSceneSectionRecorder> FMovieSceneAnimationSectionRecorderFactory::CreateSectionRecorder(const FActorRecordingSettings& InActorRecordingSettings) const
{
	return nullptr;
}

TSharedPtr<FMovieSceneAnimationSectionRecorder> FMovieSceneAnimationSectionRecorderFactory::CreateSectionRecorder(UAnimSequence* InAnimSequence, const FAnimationRecordingSettings& InAnimationSettings) const
{
	return MakeShareable(new FMovieSceneAnimationSectionRecorder(InAnimationSettings, InAnimSequence));
}

bool FMovieSceneAnimationSectionRecorderFactory::CanRecordObject(UObject* InObjectToRecord) const
{
	if (InObjectToRecord->IsA<USkeletalMeshComponent>())
	{
		USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(InObjectToRecord);
		if (SkeletalMeshComponent && SkeletalMeshComponent->SkeletalMesh)
		{
			return true;
		}
	}
	return false;
}

FMovieSceneAnimationSectionRecorder::FMovieSceneAnimationSectionRecorder(const FAnimationRecordingSettings& InAnimationSettings, UAnimSequence* InSpecifiedSequence)
	: AnimSequence(InSpecifiedSequence)
	, bRemoveRootTransform(true)
	, AnimationSettings(InAnimationSettings)
{
}

void FMovieSceneAnimationSectionRecorder::CreateSection(UObject* InObjectToRecord, UMovieScene* MovieScene, const FGuid& Guid, float Time)
{
	ObjectToRecord = InObjectToRecord;

	AActor* Actor = nullptr;
	SkeletalMeshComponent = Cast<USkeletalMeshComponent>(InObjectToRecord);
	if(!SkeletalMeshComponent.IsValid())
	{
		Actor = Cast<AActor>(InObjectToRecord);
		if(Actor != nullptr)
		{
			SkeletalMeshComponent = Actor->FindComponentByClass<USkeletalMeshComponent>();
		}
	}
	else
	{
		Actor = SkeletalMeshComponent->GetOwner();
	}

	if(SkeletalMeshComponent.IsValid())
	{
		SkeletalMesh = SkeletalMeshComponent->SkeletalMesh;
		if (SkeletalMesh != nullptr)
		{
			ComponentTransform = SkeletalMeshComponent->GetComponentToWorld().GetRelativeTransform(SkeletalMeshComponent->GetOwner()->GetTransform());

			if (!AnimSequence.IsValid())
			{
				// build an asset path
				const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();

				FString AssetPath = Settings->SequenceRecordingBasePath.Path;
				if (Settings->AnimationSubDirectory.Len() > 0)
				{
					AssetPath /= Settings->AnimationSubDirectory;
				}

				FString AssetName = Settings->SequenceName.Len() > 0 ? Settings->SequenceName : TEXT("RecordedSequence");
				AssetName += TEXT("_");
				check(Actor);
				AssetName += Actor->GetActorLabel();

				AnimSequence = SequenceRecorderUtils::MakeNewAsset<UAnimSequence>(AssetPath, AssetName);
				if (AnimSequence.IsValid())
				{
					FAssetRegistryModule::AssetCreated(AnimSequence.Get());

					// set skeleton
					AnimSequence->SetSkeleton(SkeletalMeshComponent->SkeletalMesh->Skeleton);
				}
			}

			if (AnimSequence.IsValid())
			{
				FAnimationRecorderManager::Get().RecordAnimation(SkeletalMeshComponent.Get(), AnimSequence.Get(), AnimationSettings);

				if (MovieScene)
				{
					UMovieSceneSkeletalAnimationTrack* AnimTrack = MovieScene->AddTrack<UMovieSceneSkeletalAnimationTrack>(Guid);
					if (AnimTrack)
					{
						AnimTrack->AddNewAnimation(Time, AnimSequence.Get());
						MovieSceneSection = Cast<UMovieSceneSkeletalAnimationSection>(AnimTrack->GetAllSections()[0]);
					}
				}
			}
		}
	}
}

void FMovieSceneAnimationSectionRecorder::FinalizeSection()
{
	if(AnimSequence.IsValid())
	{
		if (AnimationSettings.bRemoveRootAnimation)
		{
			// enable root motion on the animation
			AnimSequence->bForceRootLock = true;
		}
	}

	if(SkeletalMeshComponent.IsValid())
	{
		// only show a message if we dont have a valid movie section
		const bool bShowMessage = !MovieSceneSection.IsValid();
		FAnimationRecorderManager::Get().StopRecordingAnimation(SkeletalMeshComponent.Get(), bShowMessage);
	}

	if(MovieSceneSection.IsValid() && AnimSequence.IsValid())
	{
		MovieSceneSection->SetEndTime(MovieSceneSection->GetStartTime() + AnimSequence->GetPlayLength());
	}
}

void FMovieSceneAnimationSectionRecorder::Record(float CurrentTime)
{
	// The animation recorder does most of the work here

	if(SkeletalMeshComponent.IsValid())
	{
		// re-force updates on as gameplay can sometimes turn these back off!
		SkeletalMeshComponent->bEnableUpdateRateOptimizations = false;
		SkeletalMeshComponent->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
	}
}

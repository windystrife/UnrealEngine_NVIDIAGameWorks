// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneCameraCutTrack.h"
#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Evaluation/MovieSceneCameraCutTemplate.h"

#define LOCTEXT_NAMESPACE "MovieSceneCameraCutTrack"

/* UMovieSceneCameraCutTrack interface
 *****************************************************************************/
UMovieSceneCameraCutTrack::UMovieSceneCameraCutTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(0, 0, 0, 65);
#endif

	// By default, don't evaluate camera cuts in pre and postroll
	EvalOptions.bEvaluateInPreroll = EvalOptions.bEvaluateInPostroll = false;
}


void UMovieSceneCameraCutTrack::AddNewCameraCut(FGuid CameraHandle, float StartTime)
{
	Modify();


	float NewSectionEndTime = FindEndTimeForCameraCut(StartTime);

	// If there's an existing section, just swap the camera guid
	UMovieSceneCameraCutSection* ExistingSection = nullptr;
	for (auto Section : Sections)
	{
		if (Section->GetStartTime() == StartTime && Section->GetEndTime() == NewSectionEndTime)
		{
			ExistingSection = Cast<UMovieSceneCameraCutSection>(Section);
			break;
		}
	}

	UMovieSceneCameraCutSection* NewSection = ExistingSection;
	if (ExistingSection != nullptr)
	{
		ExistingSection->SetCameraGuid(CameraHandle);
	}
	else
	{
		NewSection = NewObject<UMovieSceneCameraCutSection>(this, NAME_None, RF_Transactional);
		{
			NewSection->SetStartTime(StartTime);
			NewSection->SetEndTime(FindEndTimeForCameraCut(StartTime));
			NewSection->SetCameraGuid(CameraHandle);
		}

		Sections.Add(NewSection);
	}

	// When a new CameraCut is added, sort all CameraCuts to ensure they are in the correct order
	MovieSceneHelpers::SortConsecutiveSections(Sections);

	// Once CameraCuts are sorted fixup the surrounding CameraCuts to fix any gaps
	MovieSceneHelpers::FixupConsecutiveSections(Sections, *NewSection, false);
}


/* UMovieSceneTrack interface
 *****************************************************************************/

void UMovieSceneCameraCutTrack::AddSection(UMovieSceneSection& Section)
{
	if (Section.IsA<UMovieSceneCameraCutSection>())
	{
		Sections.Add(&Section);
	}
}


UMovieSceneSection* UMovieSceneCameraCutTrack::CreateNewSection()
{
	return NewObject<UMovieSceneCameraCutSection>(this, NAME_None, RF_Transactional);
}


const TArray<UMovieSceneSection*>& UMovieSceneCameraCutTrack::GetAllSections() const
{
	return Sections;
}


void UMovieSceneCameraCutTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
	MovieSceneHelpers::FixupConsecutiveSections(Sections, Section, true);
	MovieSceneHelpers::SortConsecutiveSections(Sections);

	// @todo Sequencer: The movie scene owned by the section is now abandoned.  Should we offer to delete it?  
}


#if WITH_EDITORONLY_DATA
FText UMovieSceneCameraCutTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("TrackName", "Camera Cuts");
}
#endif


#if WITH_EDITOR
void UMovieSceneCameraCutTrack::OnSectionMoved(UMovieSceneSection& Section)
{
	MovieSceneHelpers::FixupConsecutiveSections(Sections, Section, false);
}
#endif

float UMovieSceneCameraCutTrack::FindEndTimeForCameraCut( float StartTime )
{
	float EndTime = 0;
	bool bFoundEndTime = false;

	for( UMovieSceneSection* Section : Sections )
	{
		if( Section->GetStartTime() >= StartTime )
		{
			EndTime = Section->GetStartTime();
			bFoundEndTime = true;
			break;
		}
	}

	if( !bFoundEndTime )
	{
		UMovieScene* OwnerScene = GetTypedOuter<UMovieScene>();

		// End time should just end where the movie scene ends.  Ensure it is at least the same as start time (this should only happen when the movie scene has an initial time range smaller than the start time
		EndTime = FMath::Max( OwnerScene->GetPlaybackRange().GetUpperBoundValue(), StartTime );
	}

			
	if( StartTime == EndTime )
	{
		// Give the CameraCut a reasonable length of time to start out with.  A 0 time CameraCut is not usable
		EndTime = StartTime + .5f;
	}

	return EndTime;
}

#undef LOCTEXT_NAMESPACE

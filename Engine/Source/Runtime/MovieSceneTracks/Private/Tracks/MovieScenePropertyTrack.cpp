// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieScenePropertyTrack.h"


UMovieScenePropertyTrack::UMovieScenePropertyTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EvalOptions.bEvaluateNearestSection_DEPRECATED = EvalOptions.bCanEvaluateNearestSection = true;
}


void UMovieScenePropertyTrack::SetPropertyNameAndPath(FName InPropertyName, const FString& InPropertyPath)
{
	check((InPropertyName != NAME_None) && !InPropertyPath.IsEmpty());

	PropertyName = InPropertyName;
	PropertyPath = InPropertyPath;
	
#if WITH_EDITORONLY_DATA
	if (UniqueTrackName == NAME_None)
	{
		UniqueTrackName = *PropertyPath;
	}
#endif
}


const TArray<UMovieSceneSection*>& UMovieScenePropertyTrack::GetAllSections() const
{
	return Sections;
}


void UMovieScenePropertyTrack::PostLoad()
{
#if WITH_EDITORONLY_DATA
	if (UniqueTrackName.IsNone())
	{
		UniqueTrackName = *PropertyPath;
	}
#endif

	Super::PostLoad();
}

#if WITH_EDITORONLY_DATA
FText UMovieScenePropertyTrack::GetDefaultDisplayName() const
{
	return FText::FromName(PropertyName);
}

FName UMovieScenePropertyTrack::GetTrackName() const
{
	return UniqueTrackName;
}
#endif

void UMovieScenePropertyTrack::RemoveAllAnimationData()
{
	Sections.Empty();
}


bool UMovieScenePropertyTrack::HasSection(const UMovieSceneSection& Section) const 
{
	return Sections.Contains(&Section);
}


void UMovieScenePropertyTrack::AddSection(UMovieSceneSection& Section) 
{
	Sections.Add(&Section);
}


void UMovieScenePropertyTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}


bool UMovieScenePropertyTrack::IsEmpty() const
{
	return Sections.Num() == 0;
}


UMovieSceneSection* UMovieScenePropertyTrack::FindOrAddSection(float Time, bool& bSectionAdded)
{
	bSectionAdded = false;

	// Find a spot for the section so that they are sorted by start time
	for(int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		UMovieSceneSection* Section = Sections[SectionIndex];

		if(Section->IsTimeWithinSection(Time))
		{
			return Section;
		}

		// Check if there are no more sections that would overlap the time 
		if(!Sections.IsValidIndex(SectionIndex+1) || Sections[SectionIndex+1]->GetStartTime() > Time)
		{
			// No sections overlap the time
		
			if(SectionIndex > 0)
			{
				// Append and grow the previous section
				UMovieSceneSection* PreviousSection = Sections[ SectionIndex ? SectionIndex-1 : 0 ];
		
				PreviousSection->SetEndTime(Time);
				return PreviousSection;
			}
			else if(Sections.IsValidIndex(SectionIndex+1))
			{
				// Prepend and grow the next section because there are no sections before this one
				UMovieSceneSection* NextSection = Sections[SectionIndex+1];
				NextSection->SetStartTime(Time);
				return NextSection;
			}	
			else
			{
				// SectionIndex == 0 
				UMovieSceneSection* PreviousSection = Sections[0];
				if(PreviousSection->GetEndTime() < Time)
				{
					// Append and grow the section
					PreviousSection->SetEndTime(Time);
				}
				else
				{
					// Prepend and grow the section
					PreviousSection->SetStartTime(Time);
				}
				return PreviousSection;
			}
		}

	}

	check(Sections.Num() == 0);

	// Add a new section that starts and ends at the same time
	UMovieSceneSection* NewSection = CreateNewSection();
	NewSection->SetFlags(RF_Transactional);
	NewSection->SetStartTime(Time);
	NewSection->SetEndTime(Time);

	Sections.Add(NewSection);
	
	bSectionAdded = true;

	return NewSection;
}


TRange<float> UMovieScenePropertyTrack::GetSectionBoundaries() const
{
	TArray< TRange<float> > Bounds;

	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		Bounds.Add(Sections[SectionIndex]->GetRange());
	}

	return TRange<float>::Hull(Bounds);
}

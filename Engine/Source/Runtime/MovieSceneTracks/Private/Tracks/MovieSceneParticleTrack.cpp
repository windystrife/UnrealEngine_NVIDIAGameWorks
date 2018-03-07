// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneParticleTrack.h"
#include "MovieSceneCommonHelpers.h"
#include "Sections/MovieSceneParticleSection.h"


#define LOCTEXT_NAMESPACE "MovieSceneParticleTrack"


UMovieSceneParticleTrack::UMovieSceneParticleTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(255,255,255,160);
#endif
}


const TArray<UMovieSceneSection*>& UMovieSceneParticleTrack::GetAllSections() const
{
	return ParticleSections;
}


void UMovieSceneParticleTrack::RemoveAllAnimationData()
{
	// do nothing
}


bool UMovieSceneParticleTrack::HasSection(const UMovieSceneSection& Section) const
{
	return ParticleSections.Contains(&Section);
}


void UMovieSceneParticleTrack::AddSection(UMovieSceneSection& Section)
{
	ParticleSections.Add(&Section);
}


void UMovieSceneParticleTrack::RemoveSection(UMovieSceneSection& Section)
{
	ParticleSections.Remove(&Section);
}


bool UMovieSceneParticleTrack::IsEmpty() const
{
	return ParticleSections.Num() == 0;
}


TRange<float> UMovieSceneParticleTrack::GetSectionBoundaries() const
{
	TArray< TRange<float> > Bounds;
	for (int32 i = 0; i < ParticleSections.Num(); ++i)
	{
		Bounds.Add(ParticleSections[i]->GetRange());
	}
	return TRange<float>::Hull(Bounds);
}


void UMovieSceneParticleTrack::AddNewSection( float SectionTime )
{
	if ( MovieSceneHelpers::FindSectionAtTime( ParticleSections, SectionTime ) == nullptr )
	{
		UMovieSceneParticleSection* NewSection = Cast<UMovieSceneParticleSection>( CreateNewSection() );
		NewSection->SetStartTime( SectionTime );
		NewSection->SetEndTime( SectionTime );
		NewSection->SetStartTime( SectionTime );
		NewSection->SetEndTime( SectionTime );
		ParticleSections.Add(NewSection);
	}
}

UMovieSceneSection* UMovieSceneParticleTrack::CreateNewSection()
{
	return NewObject<UMovieSceneParticleSection>( this );
}

#if WITH_EDITORONLY_DATA
FText UMovieSceneParticleTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("DisplayName", "Particle System");
}
#endif

#undef LOCTEXT_NAMESPACE

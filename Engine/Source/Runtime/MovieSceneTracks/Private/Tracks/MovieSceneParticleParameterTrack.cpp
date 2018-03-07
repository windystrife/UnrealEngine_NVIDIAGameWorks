// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneParticleParameterTrack.h"
#include "MovieSceneCommonHelpers.h"
#include "Evaluation/MovieSceneParticleParameterTemplate.h"

#define LOCTEXT_NAMESPACE "ParticleParameterTrack"

UMovieSceneParticleParameterTrack::UMovieSceneParticleParameterTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(0, 170, 255, 65);
#endif
}

FMovieSceneEvalTemplatePtr UMovieSceneParticleParameterTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneParticleParameterSectionTemplate(*CastChecked<UMovieSceneParameterSection>(&InSection), *this);
}

UMovieSceneSection* UMovieSceneParticleParameterTrack::CreateNewSection()
{
	return NewObject<UMovieSceneParameterSection>( this, UMovieSceneParameterSection::StaticClass(), NAME_None, RF_Transactional );
}

void UMovieSceneParticleParameterTrack::RemoveAllAnimationData()
{
	Sections.Empty();
}

bool UMovieSceneParticleParameterTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}

void UMovieSceneParticleParameterTrack::AddSection(UMovieSceneSection& Section)
{
	Sections.Add(&Section);
}

void UMovieSceneParticleParameterTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}

bool UMovieSceneParticleParameterTrack::IsEmpty() const
{
	return Sections.Num() == 0;
}

TRange<float> UMovieSceneParticleParameterTrack::GetSectionBoundaries() const
{
	TArray< TRange<float> > Bounds;

	for ( int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex )
	{
		Bounds.Add( Sections[SectionIndex]->GetRange() );
	}

	return TRange<float>::Hull( Bounds );
}

const TArray<UMovieSceneSection*>& UMovieSceneParticleParameterTrack::GetAllSections() const
{
	return Sections;
}


#if WITH_EDITORONLY_DATA
FText UMovieSceneParticleParameterTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("DisplayName", "Particle Parameter");
}
#endif


void UMovieSceneParticleParameterTrack::AddScalarParameterKey( FName ParameterName, float Time, float Value )
{
	UMovieSceneParameterSection* NearestSection = Cast<UMovieSceneParameterSection>(MovieSceneHelpers::FindNearestSectionAtTime(Sections, Time));
	if ( NearestSection == nullptr )
	{
		NearestSection = Cast<UMovieSceneParameterSection>(CreateNewSection());
		NearestSection->SetStartTime( Time );
		NearestSection->SetEndTime( Time );
		Sections.Add( NearestSection );
	}
	NearestSection->AddScalarParameterKey(ParameterName, Time, Value);
}

void UMovieSceneParticleParameterTrack::AddVectorParameterKey( FName ParameterName, float Time, FVector Value )
{
	UMovieSceneParameterSection* NearestSection = Cast<UMovieSceneParameterSection>( MovieSceneHelpers::FindNearestSectionAtTime( Sections, Time ) );
	if ( NearestSection == nullptr )
	{
		NearestSection = Cast<UMovieSceneParameterSection>( CreateNewSection() );
		NearestSection->SetStartTime( Time );
		NearestSection->SetEndTime( Time );
		Sections.Add( NearestSection );
	}
	NearestSection->AddVectorParameterKey( ParameterName, Time, Value );
}

void UMovieSceneParticleParameterTrack::AddColorParameterKey( FName ParameterName, float Time, FLinearColor Value )
{
	UMovieSceneParameterSection* NearestSection = Cast<UMovieSceneParameterSection>( MovieSceneHelpers::FindNearestSectionAtTime( Sections, Time ) );
	if ( NearestSection == nullptr )
	{
		NearestSection = Cast<UMovieSceneParameterSection>( CreateNewSection() );
		NearestSection->SetStartTime( Time );
		NearestSection->SetEndTime( Time );
		Sections.Add( NearestSection );
	}
	NearestSection->AddColorParameterKey( ParameterName, Time, Value );
}
#undef LOCTEXT_NAMESPACE

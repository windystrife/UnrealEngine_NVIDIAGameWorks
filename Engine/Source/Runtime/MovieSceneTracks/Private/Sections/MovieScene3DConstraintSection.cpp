// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieScene3DConstraintSection.h"


UMovieScene3DConstraintSection::UMovieScene3DConstraintSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }


void UMovieScene3DConstraintSection::SetConstraintId(const FGuid& InConstraintId)
{
	if (TryModify())
	{
		ConstraintId = InConstraintId;
	}
}


FGuid UMovieScene3DConstraintSection::GetConstraintId() const
{
	return ConstraintId;
}


TOptional<float> UMovieScene3DConstraintSection::GetKeyTime(FKeyHandle KeyHandle) const
{
	return TOptional<float>();
}


void UMovieScene3DConstraintSection::SetKeyTime(FKeyHandle KeyHandle, float Time)
{
	// do nothing
}


void UMovieScene3DConstraintSection::OnBindingsUpdated(const TMap<FGuid, FGuid>& OldGuidToNewGuidMap)
{
	if (OldGuidToNewGuidMap.Contains(GetConstraintId()))
	{
		SetConstraintId(OldGuidToNewGuidMap[GetConstraintId()]);
	}
}


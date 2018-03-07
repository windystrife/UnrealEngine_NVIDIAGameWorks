// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieScene3DAttachSection.h"


UMovieScene3DAttachSection::UMovieScene3DAttachSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	AttachSocketName = NAME_None;
	AttachComponentName = NAME_None;
	AttachmentLocationRule = EAttachmentRule::KeepRelative;
	AttachmentRotationRule = EAttachmentRule::KeepRelative;
	AttachmentScaleRule = EAttachmentRule::KeepRelative;
	DetachmentLocationRule = EDetachmentRule::KeepRelative;
	DetachmentRotationRule = EDetachmentRule::KeepRelative;
	DetachmentScaleRule = EDetachmentRule::KeepRelative;

	EvalOptions.EnableAndSetCompletionMode(EMovieSceneCompletionMode::RestoreState);
}


void UMovieScene3DAttachSection::AddAttach( float Time, float SequenceEndTime, const FGuid& InAttachId )
{
	if (TryModify())
	{
		ConstraintId = InAttachId;
	}
}


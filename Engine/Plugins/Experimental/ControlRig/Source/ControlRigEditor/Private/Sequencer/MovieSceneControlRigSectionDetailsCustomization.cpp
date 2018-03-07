// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneControlRigSectionDetailsCustomization.h"
#include "DetailLayoutBuilder.h"

TSharedRef<IDetailCustomization> FMovieSceneControlRigSectionDetailsCustomization::MakeInstance()
{
	return MakeShareable(new FMovieSceneControlRigSectionDetailsCustomization);
}

void FMovieSceneControlRigSectionDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	// Hide recording UI
	DetailLayout.HideCategory("Sequence Recording");

	// @TODO: restrict subsequences property to only accept control rig sequences
}
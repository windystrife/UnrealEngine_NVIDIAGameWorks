// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "MovieSceneSection.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;

class FIntegralKeyDetailsCustomization : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<class IDetailCustomization> MakeInstance(TWeakObjectPtr<const UMovieSceneSection> InSection);

	FIntegralKeyDetailsCustomization(TWeakObjectPtr<const UMovieSceneSection> InSection)
		: Section(InSection)
	{}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:
	TWeakObjectPtr<const UMovieSceneSection> Section;
};

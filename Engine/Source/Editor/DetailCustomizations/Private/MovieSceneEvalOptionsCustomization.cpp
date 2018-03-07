// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneEvalOptionsCustomization.h"
#include "PropertyHandle.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "IDetailChildrenBuilder.h"

TSharedRef<IPropertyTypeCustomization> FMovieSceneTrackEvalOptionsCustomization::MakeInstance()
{
	return MakeShared<FMovieSceneTrackEvalOptionsCustomization>();
}

void FMovieSceneTrackEvalOptionsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FMovieSceneTrackEvalOptionsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	bool bCanEvaluateNearestSection = !RawData.ContainsByPredicate(
		[](void* Ptr){
			return !static_cast<FMovieSceneTrackEvalOptions*>(Ptr)->bCanEvaluateNearestSection;
		}
	);

	TSharedPtr<IPropertyHandle> bEvalNearestSectionHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMovieSceneTrackEvalOptions, bEvalNearestSection));
	if (bCanEvaluateNearestSection && bEvalNearestSectionHandle.IsValid())
	{
		ChildBuilder.AddProperty(bEvalNearestSectionHandle.ToSharedRef());
	}
}

TSharedRef<IPropertyTypeCustomization> FMovieSceneSectionEvalOptionsCustomization::MakeInstance()
{
	return MakeShared<FMovieSceneSectionEvalOptionsCustomization>();
}

void FMovieSceneSectionEvalOptionsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FMovieSceneSectionEvalOptionsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	bool bCanEditCompletionMode = !RawData.ContainsByPredicate(
		[](void* Ptr){
			return !static_cast<FMovieSceneSectionEvalOptions*>(Ptr)->bCanEditCompletionMode;
		}
	);

	TSharedPtr<IPropertyHandle> CompletionModeHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMovieSceneSectionEvalOptions, CompletionMode));
	if (bCanEditCompletionMode && CompletionModeHandle.IsValid())
	{
		ChildBuilder.AddProperty(CompletionModeHandle.ToSharedRef());
	}
}

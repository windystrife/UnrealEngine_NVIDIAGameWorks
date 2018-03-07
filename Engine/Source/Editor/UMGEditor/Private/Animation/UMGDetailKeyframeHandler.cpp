// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/UMGDetailKeyframeHandler.h"
#include "Animation/WidgetAnimation.h"

#include "PropertyHandle.h"

FUMGDetailKeyframeHandler::FUMGDetailKeyframeHandler(TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor)
	: BlueprintEditor( InBlueprintEditor )
{}

bool FUMGDetailKeyframeHandler::IsPropertyKeyable(UClass* InObjectClass, const IPropertyHandle& InPropertyHandle) const
{
	return BlueprintEditor.Pin()->GetSequencer()->CanKeyProperty(FCanKeyPropertyParams(InObjectClass, InPropertyHandle));
}

bool FUMGDetailKeyframeHandler::IsPropertyKeyingEnabled() const
{
	UMovieSceneSequence* Sequence = BlueprintEditor.Pin()->GetSequencer()->GetRootMovieSceneSequence();
	return Sequence != nullptr && Sequence != UWidgetAnimation::GetNullAnimation();
}

void FUMGDetailKeyframeHandler::OnKeyPropertyClicked(const IPropertyHandle& KeyedPropertyHandle)
{
	TArray<UObject*> Objects;
	KeyedPropertyHandle.GetOuterObjects( Objects );

	FKeyPropertyParams KeyPropertyParams(Objects, KeyedPropertyHandle, ESequencerKeyMode::ManualKeyForced);

	BlueprintEditor.Pin()->GetSequencer()->KeyProperty(KeyPropertyParams);
}

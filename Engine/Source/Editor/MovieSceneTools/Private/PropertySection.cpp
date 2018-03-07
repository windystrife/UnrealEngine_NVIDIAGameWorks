// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PropertySection.h"
#include "SequencerSectionPainter.h"


FPropertySection::FPropertySection(UMovieSceneSection& InSectionObject, const FText& InDisplayName)
	: DisplayName(InDisplayName)
	, SectionObject(InSectionObject)
	, Sequencer(nullptr)
{ }

FPropertySection::FPropertySection(ISequencer* InSequencer, FGuid InObjectBinding, FName InPropertyName, 
	const FString& InPropertyPath, UMovieSceneSection& InSectionObject, const FText& InDisplayName)
	: DisplayName(InDisplayName)
	, SectionObject(InSectionObject)
	, Sequencer(InSequencer)
	, ObjectBinding(InObjectBinding)
	, PropertyBindings(MakeShareable(new FTrackInstancePropertyBindings(InPropertyName, InPropertyPath)))
{

}

UMovieSceneSection* FPropertySection::GetSectionObject()
{
	return &SectionObject;
}

FText FPropertySection::GetSectionTitle() const
{
	return FText::GetEmpty();
}

int32 FPropertySection::OnPaintSection( FSequencerSectionPainter& Painter ) const
{
	return Painter.PaintSectionBackground();
}

UProperty* FPropertySection::GetProperty() const
{
	if (!PropertyBindings.IsValid())
	{
		return nullptr;
	}

	for (const TWeakObjectPtr<UObject>& WeakObject : Sequencer->FindBoundObjects(ObjectBinding, Sequencer->GetFocusedTemplateID()))
	{
		if (UObject* Object = WeakObject.Get())
		{
			return PropertyBindings->GetProperty(*Object);
		}
	}

	return nullptr;
}

bool FPropertySection::CanGetPropertyValue() const
{
	return Sequencer != nullptr && PropertyBindings.IsValid();
}


// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CurveKeyEditors/SStringCurveKeyEditor.h"
#include "Widgets/Input/SEditableText.h"
#include "Curves/KeyHandle.h"
#include "ISequencer.h"
#include "ScopedTransaction.h"
#include "Curves/StringCurve.h"

#define LOCTEXT_NAMESPACE "StringCurveKeyEditor"

void SStringCurveKeyEditor::Construct(const FArguments& InArgs)
{
	Sequencer = InArgs._Sequencer;
	OwningSection = InArgs._OwningSection;
	Curve = InArgs._Curve;
	ExternalValue = InArgs._ExternalValue;

	float CurrentTime = Sequencer->GetLocalTime();
	ChildSlot
	[
		SNew(SEditableText)
		.SelectAllTextWhenFocused(true)
		.Text(this, &SStringCurveKeyEditor::GetText)
		.OnTextCommitted(this, &SStringCurveKeyEditor::OnTextCommitted)
 	];
}

FText SStringCurveKeyEditor::GetText() const
{
	if (ExternalValue.IsSet() && ExternalValue.Get().IsSet())
	{
		return FText::FromString(ExternalValue.Get().GetValue());
	}
	float CurrentTime = Sequencer->GetLocalTime();
	FString DefaultValue;
	FString CurrentValue = Curve->Eval(CurrentTime, DefaultValue);
	
	return FText::FromString(CurrentValue);
}

void SStringCurveKeyEditor::OnTextCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	FScopedTransaction Transaction(LOCTEXT("SetStringKey", "Set String Key Value"));
	OwningSection->SetFlags(RF_Transactional);
	if (OwningSection->TryModify())
	{
		float CurrentTime = Sequencer->GetLocalTime();
		bool bAutoSetTrackDefaults = Sequencer->GetAutoSetTrackDefaults();

		FKeyHandle CurrentKeyHandle = Curve->FindKey(CurrentTime);
		if (Curve->IsKeyHandleValid(CurrentKeyHandle))
		{
			Curve->SetKeyValue(CurrentKeyHandle, InText.ToString());
		}
		else
		{
			if (Curve->GetNumKeys() != 0 || bAutoSetTrackDefaults == false)
			{
				// When auto setting track defaults are disabled, add a key even when it's empty so that the changed
				// value is saved and is propagated to the property.
				Curve->AddKey(CurrentTime, InText.ToString(), CurrentKeyHandle);
			}

			if (Curve->GetNumKeys() != 0)
			{
				if (OwningSection->GetStartTime() > CurrentTime)
				{
					OwningSection->SetStartTime(CurrentTime);
				}
				if (OwningSection->GetEndTime() < CurrentTime)
				{
					OwningSection->SetEndTime(CurrentTime);
				}
			}
		}

		// Always update the default value when auto-set default values is enabled so that the last changes
		// are always saved to the track.
		if (bAutoSetTrackDefaults)
		{
			Curve->SetDefaultValue(InText.ToString());
		}

		Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately );
	}
}

#undef LOCTEXT_NAMESPACE

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CurveKeyEditors/SBoolCurveKeyEditor.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SCheckBox.h"
#include "Curves/KeyHandle.h"
#include "ISequencer.h"
#include "ScopedTransaction.h"
#include "Curves/IntegralCurve.h"

#define LOCTEXT_NAMESPACE "BoolCurveKeyEditor"

void SBoolCurveKeyEditor::Construct(const FArguments& InArgs)
{
	Sequencer = InArgs._Sequencer;
	OwningSection = InArgs._OwningSection;
	Curve = InArgs._Curve;
	ExternalValue = InArgs._ExternalValue;

	ChildSlot
	[
		SNew(SCheckBox)
		.IsChecked(this, &SBoolCurveKeyEditor::IsChecked)
		.OnCheckStateChanged(this, &SBoolCurveKeyEditor::OnCheckStateChanged)
	];
}

ECheckBoxState SBoolCurveKeyEditor::IsChecked() const
{
	bool bCurrentValue;
	if (ExternalValue.IsSet() && ExternalValue.Get().IsSet())
	{
		bCurrentValue = ExternalValue.Get().GetValue();
	}
	else
	{
		float CurrentTime = Sequencer->GetLocalTime();
		bool DefaultValue = false;
		bCurrentValue = Curve->Evaluate(CurrentTime, DefaultValue) != 0;
	}
	
	return bCurrentValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SBoolCurveKeyEditor::OnCheckStateChanged(ECheckBoxState NewCheckboxState)
{
	FScopedTransaction Transaction(LOCTEXT("SetBoolKey", "Set Bool Key Value"));
	OwningSection->SetFlags(RF_Transactional);
	if (OwningSection->TryModify())
	{
		float CurrentTime = Sequencer->GetLocalTime();
		bool bAutoSetTrackDefaults = Sequencer->GetAutoSetTrackDefaults();
		int32 NewValue = NewCheckboxState == ECheckBoxState::Checked ? 1 : 0;

		FKeyHandle CurrentKeyHandle = Curve->FindKey(CurrentTime);
		if (Curve->IsKeyHandleValid(CurrentKeyHandle))
		{
			Curve->SetKeyValue(CurrentKeyHandle, NewValue);
		}
		else
		{
			if (Curve->GetNumKeys() != 0 || bAutoSetTrackDefaults == false)
			{
				// When auto setting track defaults are disabled, add a key even when it's empty so that the changed
				// value is saved and is propagated to the property.
				Curve->AddKey(CurrentTime, NewValue, CurrentKeyHandle);
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
			Curve->SetDefaultValue(NewValue);
		}

		Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately );
	}
}

#undef LOCTEXT_NAMESPACE

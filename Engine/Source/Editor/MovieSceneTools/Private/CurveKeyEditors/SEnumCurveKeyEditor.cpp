// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CurveKeyEditors/SEnumCurveKeyEditor.h"
#include "Curves/KeyHandle.h"
#include "Curves/IntegralCurve.h"
#include "ISequencer.h"
#include "ScopedTransaction.h"
#include "MovieSceneToolHelpers.h"


#define LOCTEXT_NAMESPACE "EnumCurveKeyEditor"


void SEnumCurveKeyEditor::Construct(const FArguments& InArgs)
{
	Sequencer = InArgs._Sequencer;
	OwningSection = InArgs._OwningSection;
	Curve = InArgs._Curve;
	ExternalValue = InArgs._ExternalValue;

	ChildSlot
	[
		MovieSceneToolHelpers::MakeEnumComboBox(
			InArgs._Enum,
			TAttribute<int32>::Create(TAttribute<int32>::FGetter::CreateSP(this, &SEnumCurveKeyEditor::OnGetCurrentValue)),
			FOnEnumSelectionChanged::CreateSP(this, &SEnumCurveKeyEditor::OnComboSelectionChanged)
		)
	];
}


int32 SEnumCurveKeyEditor::OnGetCurrentValue() const
{
	if (ExternalValue.IsSet() && ExternalValue.Get().IsSet())
	{
		return ExternalValue.Get().GetValue();
	}

	float CurrentTime = Sequencer->GetLocalTime();
	int32 DefaultValue = 0;
	return Curve->Evaluate(CurrentTime, DefaultValue);
}


void SEnumCurveKeyEditor::OnComboSelectionChanged(int32 InSelectedItem, ESelectInfo::Type SelectInfo)
{
	FScopedTransaction Transaction(LOCTEXT("SetEnumKey", "Set Enum Key Value"));
	OwningSection->SetFlags(RF_Transactional);

	if (OwningSection->TryModify())
	{
		float CurrentTime = Sequencer->GetLocalTime();
		bool bAutoSetTrackDefaults = Sequencer->GetAutoSetTrackDefaults();

		FKeyHandle CurrentKeyHandle = Curve->FindKey(CurrentTime);
		if (Curve->IsKeyHandleValid(CurrentKeyHandle))
		{
			Curve->SetKeyValue(CurrentKeyHandle, InSelectedItem);
		}
		else
		{
			if (Curve->GetNumKeys() != 0 || bAutoSetTrackDefaults == false)
			{
				// When auto setting track defaults are disabled, add a key even when it's empty so that the changed
				// value is saved and is propagated to the property.
				Curve->AddKey(CurrentTime, InSelectedItem, CurrentKeyHandle);
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
			Curve->SetDefaultValue(InSelectedItem);
		}

		Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately );
	}
}


#undef LOCTEXT_NAMESPACE

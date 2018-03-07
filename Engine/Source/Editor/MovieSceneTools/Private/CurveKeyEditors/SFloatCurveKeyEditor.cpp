// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CurveKeyEditors/SFloatCurveKeyEditor.h"
#include "Curves/KeyHandle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SSpinBox.h"
#include "Editor.h"
#include "ISequencer.h"
#include "ScopedTransaction.h"
#include "MovieSceneCommonHelpers.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "FloatCurveKeyEditor"


template<typename T>
struct SNonThrottledSpinBox : SSpinBox<T>
{
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		FReply Reply = SSpinBox<T>::OnMouseButtonDown(MyGeometry, MouseEvent);
		if (Reply.IsEventHandled())
		{
			Reply.PreventThrottling();
		}
		return Reply;
	}
};

void SFloatCurveKeyEditor::Construct(const FArguments& InArgs)
{
	Sequencer = InArgs._Sequencer;
	OwningSection = InArgs._OwningSection;
	Curve = InArgs._Curve;
	ExternalValue = InArgs._ExternalValue;

	ChildSlot
	[
		SNew(SNonThrottledSpinBox<float>)
		.Style(&FEditorStyle::GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
		.Font(FEditorStyle::GetFontStyle("Sequencer.AnimationOutliner.RegularFont"))
		.MinValue(TOptional<float>())
		.MaxValue(TOptional<float>())
		.MaxSliderValue(TOptional<float>())
		.MinSliderValue(TOptional<float>())
		.Delta(0.001f)
		.Value(this, &SFloatCurveKeyEditor::OnGetKeyValue)
		.OnValueChanged(this, &SFloatCurveKeyEditor::OnValueChanged)
		.OnValueCommitted(this, &SFloatCurveKeyEditor::OnValueCommitted)
		.OnBeginSliderMovement(this, &SFloatCurveKeyEditor::OnBeginSliderMovement)
		.OnEndSliderMovement(this, &SFloatCurveKeyEditor::OnEndSliderMovement)
		.ClearKeyboardFocusOnCommit(true)
	];
}

void SFloatCurveKeyEditor::OnBeginSliderMovement()
{
	GEditor->BeginTransaction(LOCTEXT("SetFloatKey", "Set Float Key Value"));
	OwningSection->SetFlags(RF_Transactional);
}

void SFloatCurveKeyEditor::OnEndSliderMovement(float Value)
{
	if (GEditor->IsTransactionActive())
	{
		GEditor->EndTransaction();
	}
}

float SFloatCurveKeyEditor::OnGetKeyValue() const
{
	if (TOptional<float> ExtVal = ExternalValue.Get(TOptional<float>()))
	{
		return ExtVal.GetValue();
	}

	float CurrentTime = Sequencer->GetLocalTime();
	return Curve->Eval(CurrentTime);
}

void SFloatCurveKeyEditor::SetValue(float Value)
{
	if (!OwningSection->TryModify())
	{
		return;
	}

	float CurrentTime = Sequencer->GetLocalTime();
	bool bAutoSetTrackDefaults = Sequencer->GetAutoSetTrackDefaults();
		
	FKeyHandle CurrentKeyHandle = Curve->FindKey(CurrentTime);
	if (Curve->IsKeyHandleValid(CurrentKeyHandle))
	{
		Curve->SetKeyValue(CurrentKeyHandle, Value);
	}
	else
	{
		if (Curve->GetNumKeys() != 0 || bAutoSetTrackDefaults == false)
		{
			// When auto setting track defaults are disabled, add a key even when it's empty so that the changed
			// value is saved and is propagated to the property.
			Curve->AddKey(CurrentTime, Value, false, CurrentKeyHandle);
			MovieSceneHelpers::SetKeyInterpolation(*Curve, CurrentKeyHandle, Sequencer->GetKeyInterpolation());
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
		Curve->SetDefaultValue(Value);
	}
}

void SFloatCurveKeyEditor::OnValueChanged(float Value)
{
	SetValue(Value);

	Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
}

void SFloatCurveKeyEditor::OnValueCommitted(float Value, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		const FScopedTransaction Transaction( LOCTEXT("SetFloatKey", "Set Float Key Value") );
		OwningSection->SetFlags(RF_Transactional);

		SetValue(Value);

		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately);
	}
}

#undef LOCTEXT_NAMESPACE

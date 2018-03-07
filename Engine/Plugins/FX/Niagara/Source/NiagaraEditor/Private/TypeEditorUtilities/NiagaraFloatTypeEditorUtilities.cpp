// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraFloatTypeEditorUtilities.h"
#include "SNiagaraParameterEditor.h"
#include "NiagaraTypes.h"
#include "NiagaraEditorStyle.h"

#include "SSpinBox.h"

class SNiagaraFloatParameterEditor : public SNiagaraParameterEditor
{
public:
	SLATE_BEGIN_ARGS(SNiagaraFloatParameterEditor) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs)
	{
		ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSpinBox<float>)
				.Style(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterSpinBox")
				.Font(FNiagaraEditorStyle::Get().GetFontStyle("NiagaraEditor.ParameterFont"))
				.MinValue(TOptional<float>())
				.MaxValue(TOptional<float>())
				.MaxSliderValue(TOptional<float>())
				.MinSliderValue(TOptional<float>())
				.Delta(0.0f)
				.Value(this, &SNiagaraFloatParameterEditor::GetValue)
				.OnValueChanged(this, &SNiagaraFloatParameterEditor::ValueChanged)
				.OnValueCommitted(this, &SNiagaraFloatParameterEditor::ValueCommitted)
				.OnBeginSliderMovement(this, &SNiagaraFloatParameterEditor::BeginSliderMovement)
				.OnEndSliderMovement(this, &SNiagaraFloatParameterEditor::EndSliderMovement)
				.MinDesiredWidth(100)
			]
		];
	}

	virtual void UpdateInternalValueFromStruct(TSharedRef<FStructOnScope> Struct) override
	{
		checkf(Struct->GetStruct() == FNiagaraTypeDefinition::GetFloatStruct(), TEXT("Struct type not supported."));
		FloatValue = ((FNiagaraFloat*)Struct->GetStructMemory())->Value;
	}

	virtual void UpdateStructFromInternalValue(TSharedRef<FStructOnScope> Struct) override
	{
		checkf(Struct->GetStruct() == FNiagaraTypeDefinition::GetFloatStruct(), TEXT("Struct type not supported."));
		((FNiagaraFloat*)Struct->GetStructMemory())->Value = FloatValue;
	}

private:
	void BeginSliderMovement()
	{
		ExecuteOnBeginValueChange();
	}

	void EndSliderMovement(float Value)
	{
		ExecuteOnEndValueChange();
	}

	float GetValue() const
	{
		return FloatValue;
	}

	void ValueChanged(float Value)
	{
		FloatValue = Value;
		ExecuteOnValueChanged();
	}

	void ValueCommitted(float Value, ETextCommit::Type CommitInfo)
	{
		if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
		{
			ValueChanged(Value);
		}
	}

private:
	float FloatValue;
};

TSharedPtr<SNiagaraParameterEditor> FNiagaraEditorFloatTypeUtilities::CreateParameterEditor() const
{
	return SNew(SNiagaraFloatParameterEditor);
}

bool FNiagaraEditorFloatTypeUtilities::CanHandlePinDefaults() const
{
	return true;
}

FString FNiagaraEditorFloatTypeUtilities::GetPinDefaultStringFromValue(const FNiagaraVariable& Variable) const
{
	float Value = 0.0f;
	if (Variable.IsDataAllocated())
	{
		Value = Variable.GetValue<FNiagaraFloat>()->Value;
	}
	return FString::Printf(TEXT("%3.3f"), Value);
}

void FNiagaraEditorFloatTypeUtilities::SetValueFromPinDefaultString(const FString& StringValue, FNiagaraVariable& Variable) const
{
	Variable.AllocateData();
	Variable.GetValue<FNiagaraFloat>()->Value = FCString::Atof(*StringValue);
}
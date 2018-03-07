// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraIntegerTypeEditorUtilities.h"
#include "SNiagaraParameterEditor.h"
#include "NiagaraTypes.h"
#include "NiagaraEditorStyle.h"

#include "SSpinBox.h"

class SNiagaraIntegerParameterEditor : public SNiagaraParameterEditor
{
public:
	SLATE_BEGIN_ARGS(SNiagaraIntegerParameterEditor) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs)
	{
		ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSpinBox<int32>)
				.Style(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterSpinBox")
				.Font(FNiagaraEditorStyle::Get().GetFontStyle("NiagaraEditor.ParameterFont"))
				.MinValue(TOptional<int32>())
				.MaxValue(TOptional<int32>())
				.MaxSliderValue(TOptional<int32>())
				.MinSliderValue(TOptional<int32>())
				.Delta(0.001f)
				.Value(this, &SNiagaraIntegerParameterEditor::GetValue)
				.OnValueChanged(this, &SNiagaraIntegerParameterEditor::ValueChanged)
				.OnValueCommitted(this, &SNiagaraIntegerParameterEditor::ValueCommitted)
				.OnBeginSliderMovement(this, &SNiagaraIntegerParameterEditor::BeginSliderMovement)
				.OnEndSliderMovement(this, &SNiagaraIntegerParameterEditor::EndSliderMovement)
				.MinDesiredWidth(100)
			]
		];
	}

	virtual void UpdateInternalValueFromStruct(TSharedRef<FStructOnScope> Struct) override
	{
		checkf(Struct->GetStruct() == FNiagaraTypeDefinition::GetIntStruct(), TEXT("Struct type not supported."));
		IntValue = ((FNiagaraInt32*)Struct->GetStructMemory())->Value;
	}

	virtual void UpdateStructFromInternalValue(TSharedRef<FStructOnScope> Struct) override
	{
		checkf(Struct->GetStruct() == FNiagaraTypeDefinition::GetIntStruct(), TEXT("Struct type not supported."));
		((FNiagaraInt32*)Struct->GetStructMemory())->Value = IntValue;
	}

private:
	void BeginSliderMovement()
	{
		ExecuteOnBeginValueChange();
	}

	void EndSliderMovement(int32 Value)
	{
		ExecuteOnEndValueChange();
	}

	int32 GetValue() const
	{
		return IntValue;
	}

	void ValueChanged(int32 Value)
	{
		IntValue = Value;
		ExecuteOnValueChanged();
	}

	void ValueCommitted(int32 Value, ETextCommit::Type CommitInfo)
	{
		if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
		{
			ValueChanged(Value);
		}
	}

private:
	int32 IntValue;
};

TSharedPtr<SNiagaraParameterEditor> FNiagaraEditorIntegerTypeUtilities::CreateParameterEditor() const
{
	return SNew(SNiagaraIntegerParameterEditor);
}

bool FNiagaraEditorIntegerTypeUtilities::CanHandlePinDefaults() const
{
	return true;
}

FString FNiagaraEditorIntegerTypeUtilities::GetPinDefaultStringFromValue(const FNiagaraVariable& Variable) const
{
	int32 Value = 0;
	if (Variable.IsDataAllocated())
	{
		Value = Variable.GetValue<FNiagaraInt32>()->Value;
	}
	return FString::Printf(TEXT("%i"), Value);
}

void FNiagaraEditorIntegerTypeUtilities::SetValueFromPinDefaultString(const FString& StringValue, FNiagaraVariable& Variable) const
{
	Variable.AllocateData();
	Variable.GetValue<FNiagaraInt32>()->Value = FCString::Atoi(*StringValue);
}
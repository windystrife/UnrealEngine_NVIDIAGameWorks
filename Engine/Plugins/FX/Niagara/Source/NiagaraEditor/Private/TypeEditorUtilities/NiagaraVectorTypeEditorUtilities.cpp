// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraVectorTypeEditorUtilities.h"
#include "SNiagaraParameterEditor.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraTypes.h"

#include "SNumericEntryBox.h"
#include "Misc/DefaultValueHelper.h"

class SNiagaraVectorParameterEditorBase : public SNiagaraParameterEditor
{
public:
	SLATE_BEGIN_ARGS(SNiagaraVectorParameterEditorBase) { }
		SLATE_ARGUMENT(int32, ComponentCount)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs)
	{
		ComponentLabels.Add(NSLOCTEXT("VectorParameterEditor", "XLabel", "X"));
		ComponentLabels.Add(NSLOCTEXT("VectorParameterEditor", "YLabel", "Y"));
		ComponentLabels.Add(NSLOCTEXT("VectorParameterEditor", "ZLabel", "Z"));
		ComponentLabels.Add(NSLOCTEXT("VectorParameterEditor", "WLabel", "W"));

		TSharedRef<SHorizontalBox> ComponentBox = SNew(SHorizontalBox);
		for (int32 ComponentIndex = 0; ComponentIndex < InArgs._ComponentCount; ++ComponentIndex)
		{
			ComponentBox->AddSlot()
			.Padding(ComponentIndex == 0 ? 0 : 3, 0, 0, 0)
			[
				ConstructComponentWidget(ComponentIndex)
			];
		}

		ChildSlot
		[
			ComponentBox
		];
	}

protected:
	virtual float GetValue(int32 Index) const = 0;

	virtual void SetValue(int32 Index, float Value) = 0;

private:
	TSharedRef<SWidget> ConstructComponentWidget(int32 Index)
	{
		return SNew(SNumericEntryBox<float>)
		.Font(FNiagaraEditorStyle::Get().GetFontStyle("NiagaraEditor.ParameterFont"))
		.OverrideTextMargin(2)
		.MinValue(TOptional<float>())
		.MaxValue(TOptional<float>())
		.MaxSliderValue(TOptional<float>())
		.MinSliderValue(TOptional<float>())
		.Delta(0.0f)
		.Value(this, &SNiagaraVectorParameterEditorBase::GetValueInternal, Index)
		.OnValueChanged(this, &SNiagaraVectorParameterEditorBase::ValueChanged, Index)
		.OnValueCommitted(this, &SNiagaraVectorParameterEditorBase::ValueCommitted, Index)
		.OnBeginSliderMovement(this, &SNiagaraVectorParameterEditorBase::BeginSliderMovement)
		.OnEndSliderMovement(this, &SNiagaraVectorParameterEditorBase::EndSliderMovement)
		.AllowSpin(true)
		.LabelVAlign(EVerticalAlignment::VAlign_Center)
		.Label()
		[
			SNew(STextBlock)
			.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
			.Text(ComponentLabels[Index])
		];
	}

	void BeginSliderMovement()
	{
		ExecuteOnBeginValueChange();
	}

	void EndSliderMovement(float Value)
	{
		ExecuteOnEndValueChange();
	}

	TOptional<float> GetValueInternal(int32 Index) const
	{
		return TOptional<float>(GetValue(Index));
	}

	void ValueChanged(float Value, int32 Index)
	{
		SetValue(Index, Value);
		ExecuteOnValueChanged();
	}

	void ValueCommitted(float Value, ETextCommit::Type CommitInfo, int32 Index)
	{
		if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
		{
			ValueChanged(Value, Index);
		}
	}

private:
	TArray<FText> ComponentLabels;
};


class SNiagaraVector2ParameterEditor : public SNiagaraVectorParameterEditorBase
{
public:
	SLATE_BEGIN_ARGS(SNiagaraVector2ParameterEditor) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs)
	{
		SNiagaraVectorParameterEditorBase::Construct(
			SNiagaraVectorParameterEditorBase::FArguments()
			.ComponentCount(2));
	}

	virtual void UpdateInternalValueFromStruct(TSharedRef<FStructOnScope> Struct) override
	{
		checkf(Struct->GetStruct() == FNiagaraTypeDefinition::GetVec2Struct(), TEXT("Struct type not supported."));
		VectorValue = *((FVector2D*)Struct->GetStructMemory());
	}

	virtual void UpdateStructFromInternalValue(TSharedRef<FStructOnScope> Struct) override
	{
		checkf(Struct->GetStruct() == FNiagaraTypeDefinition::GetVec2Struct(), TEXT("Struct type not supported."));
		*((FVector2D*)Struct->GetStructMemory()) = VectorValue;
	}

protected:
	virtual float GetValue(int32 Index) const override
	{
		return VectorValue[Index];
	}

	virtual void SetValue(int32 Index, float Value) override
	{
		VectorValue[Index] = Value;
	}

private:
	FVector2D VectorValue;
};

TSharedPtr<SNiagaraParameterEditor> FNiagaraEditorVector2TypeUtilities::CreateParameterEditor() const
{
	return SNew(SNiagaraVector2ParameterEditor);
}

bool FNiagaraEditorVector2TypeUtilities::CanHandlePinDefaults() const
{
	return true;
}

FString FNiagaraEditorVector2TypeUtilities::GetPinDefaultStringFromValue(const FNiagaraVariable& Variable) const
{
	if (Variable.IsDataAllocated())
	{
		return Variable.GetValue<FVector2D>()->ToString();
	}
	return FVector2D(0.0f, 0.0f).ToString();
}

void FNiagaraEditorVector2TypeUtilities::SetValueFromPinDefaultString(const FString& StringValue, FNiagaraVariable& Variable) const
{
	Variable.AllocateData();
	Variable.GetValue<FVector2D>()->InitFromString(StringValue);
}

class SNiagaraVector3ParameterEditor : public SNiagaraVectorParameterEditorBase
{
public:
	SLATE_BEGIN_ARGS(SNiagaraVector3ParameterEditor) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs)
	{
		SNiagaraVectorParameterEditorBase::Construct(
			SNiagaraVectorParameterEditorBase::FArguments()
			.ComponentCount(3));
	}

	virtual void UpdateInternalValueFromStruct(TSharedRef<FStructOnScope> Struct) override
	{
		checkf(Struct->GetStruct() == FNiagaraTypeDefinition::GetVec3Struct(), TEXT("Struct type not supported."));
		VectorValue = *((FVector*)Struct->GetStructMemory());
	}

	virtual void UpdateStructFromInternalValue(TSharedRef<FStructOnScope> Struct) override
	{
		checkf(Struct->GetStruct() == FNiagaraTypeDefinition::GetVec3Struct(), TEXT("Struct type not supported."));
		*((FVector*)Struct->GetStructMemory()) = VectorValue;
	}

protected:
	virtual float GetValue(int32 Index) const override
	{
		return VectorValue[Index];
	}

	virtual void SetValue(int32 Index, float Value) override
	{
		VectorValue[Index] = Value;
	}

private:
	FVector VectorValue;
};

TSharedPtr<SNiagaraParameterEditor> FNiagaraEditorVector3TypeUtilities::CreateParameterEditor() const
{
	return SNew(SNiagaraVector3ParameterEditor);
}

bool FNiagaraEditorVector3TypeUtilities::CanHandlePinDefaults() const
{
	return true;
}

FString FNiagaraEditorVector3TypeUtilities::GetPinDefaultStringFromValue(const FNiagaraVariable& Variable) const
{
	// NOTE: We can not use ToString() here since the vector pin control doesn't use the standard 'X=0,Y=0,Z=0' syntax.
	FVector Value(0.0f, 0.0f, 0.0f);
	if (Variable.IsDataAllocated())
	{
		Value = *Variable.GetValue<FVector>();
	}
	return FString::Printf(TEXT("%3.3f,%3.3f,%3.3f"), Value.X, Value.Y, Value.Z);
}

void FNiagaraEditorVector3TypeUtilities::SetValueFromPinDefaultString(const FString& StringValue, FNiagaraVariable& Variable) const
{
	// NOTE: We can not use InitFromString() here since the vector pin control doesn't use the standard 'X=0,Y=0,Z=0' syntax.
	FVector Value;
	if (FDefaultValueHelper::ParseVector(StringValue, Value) == false)
	{
		Value = FVector(0.0f, 0.0f, 0.0f);
	}
	Variable.AllocateData();
	*Variable.GetValue<FVector>() = Value;
}

class SNiagaraVector4ParameterEditor : public SNiagaraVectorParameterEditorBase
{
public:
	SLATE_BEGIN_ARGS(SNiagaraVector4ParameterEditor) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs)
	{
		SNiagaraVectorParameterEditorBase::Construct(
			SNiagaraVectorParameterEditorBase::FArguments()
			.ComponentCount(4));
	}

	virtual void UpdateInternalValueFromStruct(TSharedRef<FStructOnScope> Struct) override
	{
		checkf(Struct->GetStruct() == FNiagaraTypeDefinition::GetVec4Struct(), TEXT("Struct type not supported."));
		VectorValue = *((FVector4*)Struct->GetStructMemory());
	}

	virtual void UpdateStructFromInternalValue(TSharedRef<FStructOnScope> Struct) override
	{
		checkf(Struct->GetStruct() == FNiagaraTypeDefinition::GetVec4Struct(), TEXT("Struct type not supported."));
		*((FVector4*)Struct->GetStructMemory()) = VectorValue;
	}

protected:
	virtual float GetValue(int32 Index) const override
	{
		return VectorValue[Index];
	}

	virtual void SetValue(int32 Index, float Value) override
	{
		VectorValue[Index] = Value;
	}

private:
	FVector4 VectorValue;
};

TSharedPtr<SNiagaraParameterEditor> FNiagaraEditorVector4TypeUtilities::CreateParameterEditor() const
{
	return SNew(SNiagaraVector4ParameterEditor);
}

bool FNiagaraEditorVector4TypeUtilities::CanHandlePinDefaults() const
{
	return true;
}

FString FNiagaraEditorVector4TypeUtilities::GetPinDefaultStringFromValue(const FNiagaraVariable& Variable) const
{
	// NOTE: We can not use ToString() here since the vector pin control doesn't use the standard 'X=0,Y=0,Z=0,W=0' syntax.
	FVector4 Value(0.0f, 0.0f, 0.0f, 0.0f);
	if (Variable.IsDataAllocated())
	{
		// FVector4 is aligned, so casting a pointer to it won't work
		const uint8 *VarData = Variable.GetData();
		FPlatformMemory::Memcpy(&Value, VarData, sizeof(FVector4));
		//Value = *Variable.GetValue<FVector4>();
	}
	return FString::Printf(TEXT("%3.3f,%3.3f,%3.3f,%3.3f"), Value.X, Value.Y, Value.Z, Value.W);
}

void FNiagaraEditorVector4TypeUtilities::SetValueFromPinDefaultString(const FString& StringValue, FNiagaraVariable& Variable) const
{
	// NOTE: We can not use InitFromString() here since the vector pin control doesn't use the standard 'X=0,Y=0,Z=0,W=0' syntax.
	FVector4 Value;
	if (FDefaultValueHelper::ParseVector4(StringValue, Value) == false)
	{
		Value = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	// FVector4 requires aligned memory, which cannot be safely guaranteed by the AllocateData call. Therefore, we cannot 
	// write it as a FVector4. Instead, we must write component by component.
	Variable.AllocateData();
	void* ValueWrite = Variable.GetValue<FVector4>();
	if (nullptr != ValueWrite)
	{
		((float*)ValueWrite)[0] = Value.X;
		((float*)ValueWrite)[1] = Value.Y;
		((float*)ValueWrite)[2] = Value.Z;
		((float*)ValueWrite)[3] = Value.W;
	}
	else
	{
		check(!"UNKNOWNW");
	}
}
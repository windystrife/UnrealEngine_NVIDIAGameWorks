// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/SpinBox.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Font.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// USpinBox

USpinBox::USpinBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(TEXT("/Engine/EngineFonts/Roboto"));
		Font = FSlateFontInfo(RobotoFontObj.Object, 12, FName("Bold"));
	}

	// Grab other defaults from slate arguments.
	SSpinBox<float>::FArguments Defaults;

	Value = Defaults._Value.Get();
	MinValue = Defaults._MinValue.Get().Get(0.0f);
	MaxValue = Defaults._MaxValue.Get().Get(0.0f);
	MinSliderValue = Defaults._MinSliderValue.Get().Get(0.0f);
	MaxSliderValue = Defaults._MaxSliderValue.Get().Get(0.0f);
	Delta = Defaults._Delta.Get();
	SliderExponent = Defaults._SliderExponent.Get();
	MinDesiredWidth = Defaults._MinDesiredWidth.Get();
	ClearKeyboardFocusOnCommit = Defaults._ClearKeyboardFocusOnCommit.Get();
	SelectAllTextOnCommit = Defaults._SelectAllTextOnCommit.Get();

	WidgetStyle = *Defaults._Style;
	ForegroundColor = FSlateColor(FLinearColor::Black);
}

void USpinBox::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MySpinBox.Reset();
}

TSharedRef<SWidget> USpinBox::RebuildWidget()
{
	MySpinBox = SNew(SSpinBox<float>)
	.Style(&WidgetStyle)
	.Font(Font)
	.ClearKeyboardFocusOnCommit(ClearKeyboardFocusOnCommit)
	.SelectAllTextOnCommit(SelectAllTextOnCommit)
	.Justification(Justification)
	.OnValueChanged(BIND_UOBJECT_DELEGATE(FOnFloatValueChanged, HandleOnValueChanged))
	.OnValueCommitted(BIND_UOBJECT_DELEGATE(FOnFloatValueCommitted, HandleOnValueCommitted))
	.OnBeginSliderMovement(BIND_UOBJECT_DELEGATE(FSimpleDelegate, HandleOnBeginSliderMovement))
	.OnEndSliderMovement(BIND_UOBJECT_DELEGATE(FOnFloatValueChanged, HandleOnEndSliderMovement))
	;
	
	return MySpinBox.ToSharedRef();
}

void USpinBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MySpinBox->SetDelta(Delta);
	MySpinBox->SetSliderExponent(SliderExponent);
	MySpinBox->SetMinDesiredWidth(MinDesiredWidth);

	MySpinBox->SetForegroundColor(ForegroundColor);

	// Set optional values
	bOverride_MinValue ? SetMinValue(MinValue) : ClearMinValue();
	bOverride_MaxValue ? SetMaxValue(MaxValue) : ClearMaxValue();
	bOverride_MinSliderValue ? SetMinSliderValue(MinSliderValue) : ClearMinSliderValue();
	bOverride_MaxSliderValue ? SetMaxSliderValue(MaxSliderValue) : ClearMaxSliderValue();

	// Always set the value last so that the max/min values are taken into account.
	TAttribute<float> ValueBinding = PROPERTY_BINDING(float, Value);
	MySpinBox->SetValue(ValueBinding);
}

float USpinBox::GetValue() const
{
	if (MySpinBox.IsValid())
	{
		return MySpinBox->GetValue();
	}

	return Value;
}

void USpinBox::SetValue(float InValue)
{
	Value = InValue;
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetValue(InValue);
	}
}

// MIN VALUE
float USpinBox::GetMinValue() const
{
	float ReturnVal = TNumericLimits<float>::Lowest();

	if (MySpinBox.IsValid())
	{
		ReturnVal = MySpinBox->GetMinValue();
	}
	else if (bOverride_MinValue)
	{
		ReturnVal = MinValue;
	}

	return ReturnVal;
}

void USpinBox::SetMinValue(float InMinValue)
{
	bOverride_MinValue = true;
	MinValue = InMinValue;
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetMinValue(InMinValue);
	}
}

void USpinBox::ClearMinValue()
{
	bOverride_MinValue = false;
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetMinValue(TOptional<float>());
	}
}

// MAX VALUE
float USpinBox::GetMaxValue() const
{
	float ReturnVal = TNumericLimits<float>::Max();

	if (MySpinBox.IsValid())
	{
		ReturnVal = MySpinBox->GetMaxValue();
	}
	else if (bOverride_MaxValue)
	{
		ReturnVal = MaxValue;
	}

	return ReturnVal;
}

void USpinBox::SetMaxValue(float InMaxValue)
{
	bOverride_MaxValue = true;
	MaxValue = InMaxValue;
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetMaxValue(InMaxValue);
	}
}
void USpinBox::ClearMaxValue()
{
	bOverride_MaxValue = false;
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetMaxValue(TOptional<float>());
	}
}

// MIN SLIDER VALUE
float USpinBox::GetMinSliderValue() const
{
	float ReturnVal = TNumericLimits<float>::Min();

	if (MySpinBox.IsValid())
	{
		ReturnVal = MySpinBox->GetMinSliderValue();
	}
	else if (bOverride_MinSliderValue)
	{
		ReturnVal = MinSliderValue;
	}

	return ReturnVal;
}

void USpinBox::SetMinSliderValue(float InMinSliderValue)
{
	bOverride_MinSliderValue = true;
	MinSliderValue = InMinSliderValue;
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetMinSliderValue(InMinSliderValue);
	}
}

void USpinBox::ClearMinSliderValue()
{
	bOverride_MinSliderValue = false;
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetMinSliderValue(TOptional<float>());
	}
}

// MAX SLIDER VALUE
float USpinBox::GetMaxSliderValue() const
{
	float ReturnVal = TNumericLimits<float>::Max();

	if (MySpinBox.IsValid())
	{
		ReturnVal = MySpinBox->GetMaxSliderValue();
	}
	else if (bOverride_MaxSliderValue)
	{
		ReturnVal = MaxSliderValue;
	}

	return ReturnVal;
}

void USpinBox::SetMaxSliderValue(float InMaxSliderValue)
{
	bOverride_MaxSliderValue = true;
	MaxSliderValue = InMaxSliderValue;
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetMaxSliderValue(InMaxSliderValue);
	}
}

void USpinBox::ClearMaxSliderValue()
{
	bOverride_MaxSliderValue = false;
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetMaxSliderValue(TOptional<float>());
	}
}

void USpinBox::SetForegroundColor(FSlateColor InForegroundColor)
{
	ForegroundColor = InForegroundColor;
	if ( MySpinBox.IsValid() )
	{
		MySpinBox->SetForegroundColor(ForegroundColor);
	}
}

// Event handlers
void USpinBox::HandleOnValueChanged(float InValue)
{
	if ( !IsDesignTime() )
	{
		OnValueChanged.Broadcast(InValue);
	}
}

void USpinBox::HandleOnValueCommitted(float InValue, ETextCommit::Type CommitMethod)
{
	if ( !IsDesignTime() )
	{
		OnValueCommitted.Broadcast(InValue, CommitMethod);
	}
}

void USpinBox::HandleOnBeginSliderMovement()
{
	if ( !IsDesignTime() )
	{
		OnBeginSliderMovement.Broadcast();
	}
}

void USpinBox::HandleOnEndSliderMovement(float InValue)
{
	if ( !IsDesignTime() )
	{
		OnEndSliderMovement.Broadcast(InValue);
	}
}

void USpinBox::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_ASSETS )
	{
		if ( Style_DEPRECATED != nullptr )
		{
			const FSpinBoxStyle* StylePtr = Style_DEPRECATED->GetStyle<FSpinBoxStyle>();
			if ( StylePtr != nullptr )
			{
				WidgetStyle = *StylePtr;
			}

			Style_DEPRECATED = nullptr;
		}
	}
}


#if WITH_EDITOR

const FText USpinBox::GetPaletteCategory()
{
	return LOCTEXT("Input", "Input");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE

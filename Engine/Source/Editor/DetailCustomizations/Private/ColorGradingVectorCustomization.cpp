// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "ColorGradingVectorCustomization.h"
#include "IPropertyUtilities.h"
#include "SNumericEntryBox.h"
#include "SColorGradingPicker.h"
#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBox.h"
#include "Vector4StructCustomization.h"
#include "IDetailGroup.h"
#include "SComplexGradient.h"
#include "ConfigCacheIni.h"
#include "IDetailPropertyRow.h"
#include "SCheckBox.h"
#include "Editor.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "FColorGradingCustomization"

FColorGradingVectorCustomizationBase::FColorGradingVectorCustomizationBase(TWeakPtr<IPropertyHandle> InColorGradingPropertyHandle, const TArray<TWeakPtr<IPropertyHandle>>& InSortedChildArray)
	: ColorGradingPropertyHandle(InColorGradingPropertyHandle)
	, SortedChildArray(InSortedChildArray)
	, IsRGBMode(true)
	, bIsUsingSlider(false)
{
	if (ColorGradingPropertyHandle.IsValid())
	{
		FVector4 VectorValue;
		ColorGradingPropertyHandle.Pin()->GetValue(VectorValue);
		CurrentHSVColor = FLinearColor(VectorValue.X, VectorValue.Y, VectorValue.Z).LinearRGBToHSV();
	}
}

EColorGradingModes FColorGradingVectorCustomizationBase::GetColorGradingMode() const
{
	EColorGradingModes ColorGradingMode = EColorGradingModes::Invalid;

	if (ColorGradingPropertyHandle.IsValid())
	{
		//Query all meta data we need
		UProperty* Property = ColorGradingPropertyHandle.Pin()->GetProperty();
		const FString& ColorGradingModeString = Property->GetMetaData(TEXT("ColorGradingMode"));

		if (ColorGradingModeString.Len() > 0)
		{
			if (ColorGradingModeString.Compare(TEXT("saturation")) == 0)
			{
				ColorGradingMode = EColorGradingModes::Saturation;
			}
			else if (ColorGradingModeString.Compare(TEXT("contrast")) == 0)
			{
				ColorGradingMode = EColorGradingModes::Contrast;
			}
			else if (ColorGradingModeString.Compare(TEXT("gamma")) == 0)
			{
				ColorGradingMode = EColorGradingModes::Gamma;
			}
			else if (ColorGradingModeString.Compare(TEXT("gain")) == 0)
			{
				ColorGradingMode = EColorGradingModes::Gain;
			}
			else if (ColorGradingModeString.Compare(TEXT("offset")) == 0)
			{
				ColorGradingMode = EColorGradingModes::Offset;
			}
		}
	}

	return ColorGradingMode;
}

bool FColorGradingVectorCustomizationBase::IsInRGBMode() const
{
	return IsRGBMode;
}

TOptional<float> FColorGradingVectorCustomizationBase::OnGetMaxSliderValue(TOptional<float> DefaultMaxSliderValue, int32 ColorIndex) const
{
	if (ColorIndex == 0 && !IsRGBMode) // Hue value
	{
		return 359.0f;
	}
	else if (ColorIndex == 1 && !IsRGBMode) // Saturation value
	{
		return 1.0f;
	}

	return SpinBoxMinMaxSliderValues.CurrentMaxSliderValue.IsSet() ? SpinBoxMinMaxSliderValues.CurrentMaxSliderValue : DefaultMaxSliderValue;
}

TOptional<float> FColorGradingVectorCustomizationBase::OnGetMinSliderValue(TOptional<float> DefaultMinSliderValue, int32 ColorIndex) const
{
	if (!IsRGBMode)
	{
		return 0.0f;
	}

	return SpinBoxMinMaxSliderValues.CurrentMinSliderValue.IsSet() ? SpinBoxMinMaxSliderValues.CurrentMinSliderValue : DefaultMinSliderValue;
}

float FColorGradingVectorCustomizationBase::OnGetSliderDeltaValue(float DefaultValue, int32 ColorIndex) const
{
	if (ColorIndex == 0 && !IsRGBMode) // Hue value
	{
		return 1.0f;
	}

	return DefaultValue;
}

TOptional<float> FColorGradingVectorCustomizationBase::OnGetMaxValue(TOptional<float> DefaultValue, int32 ColorIndex) const
{
	if (ColorIndex == 0 && !IsRGBMode) // Hue value
	{
		return 359.0f;
	}
	else if (ColorIndex == 1 && !IsRGBMode) // Saturation value
	{
		return 1.0f;
	}

	return DefaultValue;
}

void FColorGradingVectorCustomizationBase::OnBeginSliderMovement()
{
	bIsUsingSlider = true;
	GEditor->BeginTransaction(FText::Format(NSLOCTEXT("ColorGradingVectorCustomization", "SetPropertyValue", "Edit {0}"), ColorGradingPropertyHandle.Pin()->GetPropertyDisplayName()));
}

void FColorGradingVectorCustomizationBase::OnEndSliderMovement(float NewValue, int32 ColorIndex)
{
	bIsUsingSlider = false;

	OnValueChanged(NewValue, ColorIndex);

	GEditor->EndTransaction();
}

FText FColorGradingVectorCustomizationBase::OnGetColorLabelText(FText DefaultText, int32 ColorIndex) const
{
	if (ColorIndex >= 0 && ColorIndex < 3)
	{
		if (IsRGBMode)
		{
			FText LabelRGBText[3];
			LabelRGBText[0] = NSLOCTEXT("ColorGradingVectorCustomizationRGBNS", "RedChannelSmallName", "R");
			LabelRGBText[1] = NSLOCTEXT("ColorGradingVectorCustomizationRGBNS", "GreenChannelSmallName", "G");
			LabelRGBText[2] = NSLOCTEXT("ColorGradingVectorCustomizationRGBNS", "BlueChannelSmallName", "B");

			return LabelRGBText[ColorIndex];
		}
		else
		{
			FText LabelHSVText[3];
			LabelHSVText[0] = NSLOCTEXT("ColorGradingVectorCustomizationHSVNS", "HueChannelSmallName", "H");
			LabelHSVText[1] = NSLOCTEXT("ColorGradingVectorCustomizationHSVNS", "SaturationChannelSmallName", "S");
			LabelHSVText[2] = NSLOCTEXT("ColorGradingVectorCustomizationHSVNS", "ValueChannelSmallName", "V");

			return LabelHSVText[ColorIndex];
		}
	}
	else if (ColorIndex == 3)
	{
		return NSLOCTEXT("ColorGradingVectorCustomizationNS", "LuminanceChannelSmallName", "Y");
	}

	return DefaultText;
}

FText FColorGradingVectorCustomizationBase::OnGetColorLabelToolTipsText(FText DefaultText, int32 ColorIndex) const
{
	if (ColorIndex >= 0 && ColorIndex < 3)
	{
		if (IsRGBMode)
		{
			FText LabelRGBText[3];
			LabelRGBText[0] = NSLOCTEXT("ColorGradingVectorCustomizationRGBNSToolTips", "RedChannelSmallNameToolTips", "Red");
			LabelRGBText[1] = NSLOCTEXT("ColorGradingVectorCustomizationRGBNSToolTips", "GreenChannelSmallNameToolTips", "Green");
			LabelRGBText[2] = NSLOCTEXT("ColorGradingVectorCustomizationRGBNSToolTips", "BlueChannelSmallNameToolTips", "Blue");

			return LabelRGBText[ColorIndex];
		}
		else
		{
			FText LabelHSVText[3];
			LabelHSVText[0] = NSLOCTEXT("ColorGradingVectorCustomizationHSVNSToolTips", "HueChannelSmallNameToolTips", "Hue");
			LabelHSVText[1] = NSLOCTEXT("ColorGradingVectorCustomizationHSVNSToolTips", "SaturationChannelSmallNameToolTips", "Saturation");
			LabelHSVText[2] = NSLOCTEXT("ColorGradingVectorCustomizationHSVNSToolTips", "ValueChannelSmallNameToolTips", "Value");

			return LabelHSVText[ColorIndex];
		}
	}
	else if (ColorIndex == 3)
	{
		return NSLOCTEXT("ColorGradingVectorCustomizationNSToolTips", "LuminanceChannelSmallNameToolTips", "Luminance");
	}

	return DefaultText;
}

void FColorGradingVectorCustomizationBase::OnValueChanged(float NewValue, int32 ColorIndex)
{
	FVector4 CurrentValueVector;
	verifySlow(ColorGradingPropertyHandle.Pin()->GetValue(CurrentValueVector) == FPropertyAccess::Success);

	FVector4 NewValueVector = CurrentValueVector;

	if (IsRGBMode)
	{
		NewValueVector[ColorIndex] = NewValue;

		if (ColorIndex < 3)
		{
			CurrentHSVColor = FLinearColor(NewValueVector.X, NewValueVector.Y, NewValueVector.Z).LinearRGBToHSV();
		}
	}
	else
	{
		if (ColorIndex < 3) 
		{
			CurrentHSVColor.Component(ColorIndex) = NewValue;
			NewValueVector = CurrentHSVColor.HSVToLinearRGB();
			NewValueVector.W = CurrentValueVector.W;
		}
		else // Luminance
		{
			NewValueVector[ColorIndex] = NewValue;
		}

		OnCurrentHSVColorChanged.Broadcast(CurrentHSVColor, true);
	}

	if (ColorGradingPropertyHandle.IsValid())
	{
		ColorGradingPropertyHandle.Pin()->SetValue(NewValueVector, bIsUsingSlider ? EPropertyValueSetFlags::InteractiveChange : EPropertyValueSetFlags::DefaultFlags);
	}
}

TOptional<float> FColorGradingVectorCustomizationBase::OnSliderGetValue(int32 ColorIndex) const
{
	FVector4 ValueVector;
	
	if (ColorGradingPropertyHandle.Pin()->GetValue(ValueVector) == FPropertyAccess::Success)
	{
		float Value = 0.0f;

		if (IsRGBMode)
		{
			Value = ValueVector[ColorIndex];
		}
		else
		{
			Value = ColorIndex < 3 ? CurrentHSVColor.Component(ColorIndex) : ValueVector.W;
		}

		return Value;
	}

	return TOptional<float>();
}

void FColorGradingVectorCustomizationBase::OnCurrentHSVColorChangedDelegate(FLinearColor NewHSVColor, bool Originator)
{
	CurrentHSVColor = NewHSVColor;

	if (Originator)
	{
		OnCurrentHSVColorChanged.Broadcast(CurrentHSVColor, false);
	}
}

FLinearColor FColorGradingVectorCustomizationBase::GetGradientFillerColor(int32 ColorIndex) const
{
	FVector4 ValueVector;

	if (ColorGradingPropertyHandle.Pin()->GetValue(ValueVector) == FPropertyAccess::Success)
	{
		if (IsRGBMode)
		{
			switch (ColorIndex)
			{
				case 0:	return FLinearColor(SpinBoxMinMaxSliderValues.CurrentMaxSliderValue.GetValue(), ValueVector.Y, ValueVector.Z, 1.0f);
				case 1:	return FLinearColor(ValueVector.X, SpinBoxMinMaxSliderValues.CurrentMaxSliderValue.GetValue(), ValueVector.Z, 1.0f);
				case 2:	return FLinearColor(ValueVector.X, ValueVector.Y, SpinBoxMinMaxSliderValues.CurrentMaxSliderValue.GetValue(), 1.0f);
				case 3: return FLinearColor(ValueVector.X, ValueVector.Y, ValueVector.Z, 1.0f);
				default:	return FLinearColor(ForceInit);
			}
		}

		switch (ColorIndex)
		{
			case 0:		return FLinearColor(CurrentHSVColor.R, CurrentHSVColor.G, CurrentHSVColor.B, 1.0f);
			case 1:		return FLinearColor(CurrentHSVColor.R, 1.0f, CurrentHSVColor.B, 1.0f).HSVToLinearRGB();
			case 2:		return FLinearColor(CurrentHSVColor.R, CurrentHSVColor.G, SpinBoxMinMaxSliderValues.CurrentMaxSliderValue.GetValue(), 1.0f).HSVToLinearRGB();
			case 3:		return FLinearColor(CurrentHSVColor.R, CurrentHSVColor.G, CurrentHSVColor.B, 1.0f).HSVToLinearRGB();
			default:	return FLinearColor(ForceInit);
		}
	}

	return FLinearColor(ForceInit);
}

FLinearColor FColorGradingVectorCustomizationBase::GetGradientEndColor(int32 ColorIndex) const
{
	FVector4 ValueVector;
	if (ColorGradingPropertyHandle.Pin()->GetValue(ValueVector) == FPropertyAccess::Success)
	{
		if (IsRGBMode)
		{
			switch (ColorIndex)
			{
				case 0:	return FLinearColor(1.0f, ValueVector.Y, ValueVector.Z, 1.0f);
				case 1:	return FLinearColor(ValueVector.X, 1.0f, ValueVector.Z, 1.0f);
				case 2:	return FLinearColor(ValueVector.X, ValueVector.Y, 1.0f, 1.0f);
				case 3: return FLinearColor(ValueVector.X, ValueVector.Y, ValueVector.Z, 1.0f);
				default:	return FLinearColor(ForceInit);
			}
		}

		switch (ColorIndex)
		{
			case 0:		return FLinearColor(CurrentHSVColor.R, CurrentHSVColor.G, CurrentHSVColor.B, 1.0f);
			case 1:		return FLinearColor(CurrentHSVColor.R, 1.0f, CurrentHSVColor.B, 1.0f).HSVToLinearRGB();
			case 2:		return FLinearColor(CurrentHSVColor.R, CurrentHSVColor.G, 1.0f, 1.0f).HSVToLinearRGB();
			case 3:		return FLinearColor(CurrentHSVColor.R, CurrentHSVColor.G, CurrentHSVColor.B, 1.0f).HSVToLinearRGB();
			default:	return FLinearColor(ForceInit);
		}
	}

	return FLinearColor(ForceInit);
}

FLinearColor FColorGradingVectorCustomizationBase::GetGradientStartColor(int32 ColorIndex) const
{
	FVector4 ValueVector;

	if (ColorGradingPropertyHandle.Pin()->GetValue(ValueVector) == FPropertyAccess::Success)
	{
		if (IsRGBMode)
		{
			switch (ColorIndex)
			{
			case 0:		return FLinearColor(0.0f, ValueVector.Y, ValueVector.Z, 1.0f);
			case 1:		return FLinearColor(ValueVector.X, 0.0f, ValueVector.Z, 1.0f);
			case 2:		return FLinearColor(ValueVector.X, ValueVector.Y, 0.0f, 1.0f);
			case 3:		return FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
			default:	return FLinearColor(ForceInit);
			}
		}

		switch (ColorIndex)
		{
		case 0:		return FLinearColor(CurrentHSVColor.R, CurrentHSVColor.G, CurrentHSVColor.B, 1.0f);
		case 1:		return FLinearColor(CurrentHSVColor.R, 0.0f, CurrentHSVColor.B, 1.0f).HSVToLinearRGB();
		case 2:		return FLinearColor(CurrentHSVColor.R, CurrentHSVColor.G, 0.0f, 1.0f).HSVToLinearRGB();
		case 3:		return FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
		default:	return FLinearColor(ForceInit);
		}
	}

	return FLinearColor(ForceInit);
}

TArray<FLinearColor> FColorGradingVectorCustomizationBase::GetGradientColor(int32 ColorIndex) const
{
	TArray<FLinearColor> GradientColors;

	if (IsRGBMode || ColorIndex > 0)
	{
		GradientColors.Add(GetGradientStartColor(ColorIndex));
		GradientColors.Add(GetGradientEndColor(ColorIndex));
		GradientColors.Add(GetGradientFillerColor(ColorIndex));
	}
	else // HSV Hue handling
	{
		for (int32 i = 0; i < 7; ++i)
		{
			GradientColors.Add(FLinearColor((i % 6) * 60.f, 1.f, 1.f).HSVToLinearRGB());
		}
	}

	return GradientColors;
}

void FColorGradingVectorCustomizationBase::OnDynamicSliderMaxValueChanged(float NewMaxSliderValue, TWeakPtr<SWidget> InValueChangedSourceWidget, bool IsOriginator, bool UpdateOnlyIfHigher)
{
	if (NumericEntryBoxWidgetList.Num() > 0)
	{
		if (!SpinBoxMinMaxSliderValues.CurrentMaxSliderValue.IsSet() || (NewMaxSliderValue > SpinBoxMinMaxSliderValues.CurrentMaxSliderValue.GetValue() && UpdateOnlyIfHigher) || !UpdateOnlyIfHigher)
		{
			SpinBoxMinMaxSliderValues.CurrentMaxSliderValue = NewMaxSliderValue;
		}

		if (IsOriginator)
		{
			OnNumericEntryBoxDynamicSliderMaxValueChanged.Broadcast(NewMaxSliderValue, InValueChangedSourceWidget, false, UpdateOnlyIfHigher);
		}
	}
}

void FColorGradingVectorCustomizationBase::OnDynamicSliderMinValueChanged(float NewMinSliderValue, TWeakPtr<SWidget> InValueChangedSourceWidget, bool IsOriginator, bool UpdateOnlyIfLower)
{
	if (NumericEntryBoxWidgetList.Num() > 0)
	{
		if (!SpinBoxMinMaxSliderValues.CurrentMinSliderValue.IsSet() || (NewMinSliderValue < SpinBoxMinMaxSliderValues.CurrentMinSliderValue.GetValue() && UpdateOnlyIfLower) || !UpdateOnlyIfLower)
		{
			SpinBoxMinMaxSliderValues.CurrentMinSliderValue = NewMinSliderValue;
		}

		if (IsOriginator)
		{
			OnNumericEntryBoxDynamicSliderMinValueChanged.Broadcast(NewMinSliderValue, InValueChangedSourceWidget, false, UpdateOnlyIfLower);
		}
	}
}

bool FColorGradingVectorCustomizationBase::GetSupportDynamicSliderMaxValue(bool DefaultValue, int32 ColorIndex) const
{
	if (DefaultValue)
	{
		if (!IsRGBMode)
		{
			return ColorIndex >= 2;
		}
	}

	return DefaultValue;
}

bool FColorGradingVectorCustomizationBase::GetSupportDynamicSliderMinValue(bool DefaultValue, int32 ColorIndex) const
{
	if (DefaultValue)
	{
		if (!IsRGBMode)
		{
			return ColorIndex >= 2;
		}
	}

	return DefaultValue;
}

bool FColorGradingVectorCustomizationBase::IsEntryBoxEnabled(int32 ColorIndex) const
{
	return OnSliderGetValue(ColorIndex) != TOptional<float>();
}

TSharedRef<SNumericEntryBox<float>> FColorGradingVectorCustomizationBase::MakeNumericEntryBox(int32 ColorIndex, TOptional<float>& MinValue, TOptional<float>& MaxValue, TOptional<float>& SliderMinValue, TOptional<float>& SliderMaxValue, float& SliderExponent, float& Delta, int32 &ShiftMouseMovePixelPerDelta, bool& SupportDynamicSliderMaxValue, bool& SupportDynamicSliderMinValue)
{
	TAttribute<FText> TextGetter = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FColorGradingVectorCustomizationBase::OnGetColorLabelText, ColorGradingPropertyHandle.Pin()->GetPropertyDisplayName(), ColorIndex));
	TSharedRef<SWidget> LabelWidget = SNumericEntryBox<float>::BuildLabel(TextGetter, FLinearColor::White, FLinearColor(0.2f, 0.2f, 0.2f));

	return SNew(SNumericEntryBox<float>)
		.SpinBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FSpinBoxStyle>("NumericEntrySpinBox_Dark"))
		.EditableTextBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("DarkEditableTextBox"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.UndeterminedString(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"))
		.Value(this, &FColorGradingVectorCustomizationBase::OnSliderGetValue, ColorIndex)
		.OnValueChanged(this, &FColorGradingVectorCustomizationBase::OnValueChanged, ColorIndex)
		.OnBeginSliderMovement(this, &FColorGradingVectorCustomizationBase::OnBeginSliderMovement)
		.OnEndSliderMovement(this, &FColorGradingVectorCustomizationBase::OnEndSliderMovement, ColorIndex)
		// Only allow spin on handles with one object.  Otherwise it is not clear what value to spin
		.AllowSpin(ColorGradingPropertyHandle.Pin()->GetNumOuterObjects() == 1)
		.ShiftMouseMovePixelPerDelta(ShiftMouseMovePixelPerDelta)
		.SupportDynamicSliderMaxValue(this, &FColorGradingVectorCustomizationBase::GetSupportDynamicSliderMaxValue, SupportDynamicSliderMaxValue, ColorIndex)
		.SupportDynamicSliderMinValue(this, &FColorGradingVectorCustomizationBase::GetSupportDynamicSliderMinValue, SupportDynamicSliderMinValue, ColorIndex)
		.OnDynamicSliderMaxValueChanged(this, &FColorGradingVectorCustomizationBase::OnDynamicSliderMaxValueChanged)
		.OnDynamicSliderMinValueChanged(this, &FColorGradingVectorCustomizationBase::OnDynamicSliderMinValueChanged)
		.MinValue(MinValue)
		.MaxValue(this, &FColorGradingVectorCustomizationBase::OnGetMaxValue, MaxValue, ColorIndex)
		.MinSliderValue(this, &FColorGradingVectorCustomizationBase::OnGetMinSliderValue, SliderMinValue, ColorIndex)
		.MaxSliderValue(this, &FColorGradingVectorCustomizationBase::OnGetMaxSliderValue, SliderMaxValue, ColorIndex)
		.SliderExponent(SliderExponent)
		.SliderExponentNeutralValue(SliderMinValue.GetValue() + (SliderMaxValue.GetValue() - SliderMinValue.GetValue()) / 2.0f)
		.Delta(this, &FColorGradingVectorCustomizationBase::OnGetSliderDeltaValue, Delta, ColorIndex)
		.ToolTipText(this, &FColorGradingVectorCustomizationBase::OnGetColorLabelToolTipsText, ColorGradingPropertyHandle.Pin()->GetPropertyDisplayName(), ColorIndex)
		.LabelPadding(FMargin(0))
		.IsEnabled(this, &FColorGradingVectorCustomizationBase::IsEntryBoxEnabled, ColorIndex)
		.Label()
		[
			LabelWidget
		];
}

//////////////////////////////////////////////////////////////////////////
// Color Gradient customization implementation

FColorGradingVectorCustomization::FColorGradingVectorCustomization(TWeakPtr<IPropertyHandle> InColorGradingPropertyHandle, const TArray<TWeakPtr<IPropertyHandle>>& InSortedChildArray)
	: FColorGradingVectorCustomizationBase(InColorGradingPropertyHandle, InSortedChildArray)
{
	CustomColorGradingBuilder = nullptr;
}

FColorGradingVectorCustomization::~FColorGradingVectorCustomization()
{	
}

void FColorGradingVectorCustomization::MakeHeaderRow(FDetailWidgetRow& Row, TSharedRef<FVector4StructCustomization> InVector4Customization)
{
	TSharedPtr<SHorizontalBox> ContentHorizontalBox = SNew(SHorizontalBox)
		.IsEnabled(InVector4Customization, &FMathStructCustomization::IsValueEnabled, ColorGradingPropertyHandle);

	Row.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			[
				ColorGradingPropertyHandle.Pin()->CreatePropertyNameWidget()
			]
		];

	EColorGradingModes ColorGradingMode = GetColorGradingMode();

	if (ColorGradingMode == EColorGradingModes::Offset)
	{
		Row.ValueContent()
			// Make enough space for each child handle
			.MinDesiredWidth(125.0f * SortedChildArray.Num())
			.MaxDesiredWidth(125.0f * SortedChildArray.Num())
			[
				ContentHorizontalBox.ToSharedRef()
			];

		// Make a widget for each property.  The vector component properties  will be displayed in the header
		TOptional<float> MinValue, MaxValue, SliderMinValue, SliderMaxValue;
		float SliderExponent, Delta;
		int32 ShiftMouseMovePixelPerDelta = 1;
		bool SupportDynamicSliderMaxValue = false;
		bool SupportDynamicSliderMinValue = false;

		TSharedRef<IPropertyHandle> ColorGradingPropertyHandleRef = ColorGradingPropertyHandle.Pin().ToSharedRef();
		FMathStructCustomization::ExtractNumericMetadata<float>(ColorGradingPropertyHandleRef, MinValue, MaxValue, SliderMinValue, SliderMaxValue, SliderExponent, Delta, ShiftMouseMovePixelPerDelta, SupportDynamicSliderMaxValue, SupportDynamicSliderMinValue);

		for (int32 ColorIndex = 0; ColorIndex < SortedChildArray.Num(); ++ColorIndex)
		{
			TWeakPtr<IPropertyHandle> WeakHandlePtr = SortedChildArray[ColorIndex];
			TSharedRef<SNumericEntryBox<float>> NumericEntryBox = MakeNumericEntryBox(ColorIndex, MinValue, MaxValue, SliderMinValue, SliderMaxValue, SliderExponent, Delta, ShiftMouseMovePixelPerDelta, SupportDynamicSliderMaxValue, SupportDynamicSliderMinValue);
			TSharedPtr<SSpinBox<float>> NumericEntrySpinBox = StaticCastSharedPtr<SSpinBox<float>>(NumericEntryBox->GetSpinBox());
			 
			NumericEntryBoxWidgetList.Add(NumericEntryBox);

			if (NumericEntrySpinBox.IsValid())
			{
				float MinSliderValue = NumericEntrySpinBox->GetMinSliderValue();
				float MaxSliderValue = NumericEntrySpinBox->GetMaxSliderValue();

				SpinBoxMinMaxSliderValues.CurrentMinSliderValue = MinSliderValue == TNumericLimits<float>::Lowest() ? TOptional<float>() : MinSliderValue;
				SpinBoxMinMaxSliderValues.CurrentMaxSliderValue = MaxSliderValue == TNumericLimits<float>::Max() ? TOptional<float>() : MaxSliderValue;
				SpinBoxMinMaxSliderValues.DefaultMinSliderValue = SpinBoxMinMaxSliderValues.CurrentMinSliderValue;
				SpinBoxMinMaxSliderValues.DefaultMaxSliderValue = SpinBoxMinMaxSliderValues.CurrentMaxSliderValue;
			}

			ContentHorizontalBox->AddSlot()
				.Padding(FMargin(0.0f, 2.0f, 3.0f, 0.0f))
				.VAlign(VAlign_Top)
				[
					NumericEntryBox
				];
		}
	}
	else
	{
		Row.ValueContent()
			.VAlign(VAlign_Center)
			.MinDesiredWidth(250.0f)
			[
				ContentHorizontalBox.ToSharedRef()
			];

		ContentHorizontalBox->AddSlot()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.0f, 2.0f, 3.0f, 0.0f))
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SColorBlock)
					.Color(this, &FColorGradingVectorCustomization::OnGetHeaderColorBlock)
					.ShowBackgroundForAlpha(false)
					.IgnoreAlpha(true)
					.ColorIsHSV(false)
					.Size(FVector2D(70.0f, 12.0f))
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ColorAndOpacity(FSlateColor(FLinearColor::Black)) // we know the background is always white, so can safely set this to black
					.Visibility(this, &FColorGradingVectorCustomization::GetMultipleValuesTextVisibility)
				]
			];
	}
}

EVisibility FColorGradingVectorCustomization::GetMultipleValuesTextVisibility() const
{
	FVector4 VectorValue;
	return (ColorGradingPropertyHandle.Pin()->GetValue(VectorValue) == FPropertyAccess::MultipleValues) ? EVisibility::Visible : EVisibility::Collapsed;
}

FLinearColor FColorGradingVectorCustomization::OnGetHeaderColorBlock() const
{
	FLinearColor ColorValue(0.0f, 0.0f, 0.0f);
	FVector4 VectorValue;
	if (ColorGradingPropertyHandle.Pin()->GetValue(VectorValue) == FPropertyAccess::Success)
	{
		ColorValue.R = VectorValue.X * VectorValue.W;
		ColorValue.G = VectorValue.Y * VectorValue.W;
		ColorValue.B = VectorValue.Z * VectorValue.W;
	}
	else
	{
		ColorValue = FLinearColor::White;
	}

	return ColorValue;
}

void FColorGradingVectorCustomization::OnColorModeChanged(bool InIsRGBMode)
{
	IsRGBMode = InIsRGBMode;

	for (int32 ColorIndex = 0; ColorIndex < NumericEntryBoxWidgetList.Num(); ++ColorIndex)
	{
		TSharedPtr<SNumericEntryBox<float>> NumericBox = StaticCastSharedPtr<SNumericEntryBox<float>>(NumericEntryBoxWidgetList[ColorIndex].Pin());

		if (NumericBox.IsValid())
		{
			TSharedPtr<SSpinBox<float>> SpinBox = StaticCastSharedPtr<SSpinBox<float>>(NumericBox->GetSpinBox());

			if (SpinBox.IsValid())
			{
				SpinBox->SetValue(SpinBox->GetValueAttribute());
			}
		}
	}
}

void FColorGradingVectorCustomization::CustomizeChildren(IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	ParentGroup = StructBuilder.GetParentGroup();
	CustomColorGradingBuilder = MakeShareable(new FColorGradingCustomBuilder(ColorGradingPropertyHandle, SortedChildArray, StaticCastSharedRef<FColorGradingVectorCustomization>(AsShared()), ParentGroup));

	// Add the individual properties as children as well so the vector can be expanded for more room
	StructBuilder.AddCustomBuilder(CustomColorGradingBuilder.ToSharedRef());

	if (ParentGroup != nullptr)
	{
		TSharedPtr<IDetailPropertyRow> PropertyRow = ParentGroup->FindPropertyRow(ColorGradingPropertyHandle.Pin().ToSharedRef());
		verifySlow(PropertyRow.IsValid());

		PropertyRow->OverrideResetToDefault(FResetToDefaultOverride::Create(FIsResetToDefaultVisible::CreateSP(CustomColorGradingBuilder.Get(), &FColorGradingCustomBuilder::CanResetToDefault),
			FResetToDefaultHandler::CreateSP(CustomColorGradingBuilder.Get(), &FColorGradingCustomBuilder::ResetToDefault)));
	}
}


//////////////////////////////////////////////////////////////////////////
// Color Gradient custom builder implementation

FColorGradingCustomBuilder::FColorGradingCustomBuilder(TWeakPtr<IPropertyHandle> InColorGradingPropertyHandle, const TArray<TWeakPtr<IPropertyHandle>>& InSortedChildArray, 
													   TSharedRef<FColorGradingVectorCustomization> InColorGradingCustomization, IDetailGroup* InParentGroup)
	: FColorGradingVectorCustomizationBase(InColorGradingPropertyHandle, InSortedChildArray)
	, ColorGradingCustomization(InColorGradingCustomization)
{
	ParentGroup = InParentGroup;
}

FColorGradingCustomBuilder::~FColorGradingCustomBuilder()
{
	if (ColorGradingCustomization.IsValid())
	{
		OnColorModeChanged.RemoveAll(this);

		ColorGradingCustomization->GetOnCurrentHSVColorChangedDelegate().RemoveAll(this);
		OnCurrentHSVColorChanged.RemoveAll(ColorGradingCustomization.Get());

		ColorGradingCustomization->GetOnNumericEntryBoxDynamicSliderMaxValueChangedDelegate().RemoveAll(this);
		ColorGradingCustomization->GetOnNumericEntryBoxDynamicSliderMinValueChangedDelegate().RemoveAll(this);
		ColorGradingCustomization->GetOnNumericEntryBoxDynamicSliderMaxValueChangedDelegate().RemoveAll(ColorGradingPickerWidget.Pin().Get());
		ColorGradingCustomization->GetOnNumericEntryBoxDynamicSliderMinValueChangedDelegate().RemoveAll(ColorGradingPickerWidget.Pin().Get());

		OnNumericEntryBoxDynamicSliderMaxValueChanged.RemoveAll(ColorGradingCustomization.Get());
		OnNumericEntryBoxDynamicSliderMinValueChanged.RemoveAll(ColorGradingCustomization.Get());
	}

	if (ColorGradingPickerWidget.IsValid())
	{
		if (ColorGradingCustomization.IsValid())
		{
			ColorGradingPickerWidget.Pin()->GetOnNumericEntryBoxDynamicSliderMaxValueChangedDelegate().RemoveAll(ColorGradingCustomization.Get());
			ColorGradingPickerWidget.Pin()->GetOnNumericEntryBoxDynamicSliderMinValueChangedDelegate().RemoveAll(ColorGradingCustomization.Get());
		}

		ColorGradingPickerWidget.Pin()->GetOnNumericEntryBoxDynamicSliderMaxValueChangedDelegate().RemoveAll(this);
		ColorGradingPickerWidget.Pin()->GetOnNumericEntryBoxDynamicSliderMinValueChangedDelegate().RemoveAll(this);

		OnNumericEntryBoxDynamicSliderMaxValueChanged.RemoveAll(ColorGradingPickerWidget.Pin().Get());
		OnNumericEntryBoxDynamicSliderMinValueChanged.RemoveAll(ColorGradingPickerWidget.Pin().Get());
	}

	if (ParentGroup != nullptr)
	{
		ParentGroup->GetOnDetailGroupReset().RemoveAll(this);
	}

	OnCurrentHSVColorChanged.RemoveAll(this);
}

void FColorGradingCustomBuilder::OnDetailGroupReset()
{
	FVector4 CurrentValueVector;
	verifySlow(ColorGradingPropertyHandle.Pin()->GetValue(CurrentValueVector) == FPropertyAccess::Success);
	CurrentHSVColor = FLinearColor(CurrentValueVector.X, CurrentValueVector.Y, CurrentValueVector.Z).LinearRGBToHSV();

	OnCurrentHSVColorChanged.Broadcast(CurrentHSVColor, true);

	if (SpinBoxMinMaxSliderValues.DefaultMaxSliderValue.IsSet())
	{
		OnDynamicSliderMaxValueChanged(SpinBoxMinMaxSliderValues.DefaultMaxSliderValue.GetValue(), nullptr, true, false);
	}

	if (SpinBoxMinMaxSliderValues.DefaultMinSliderValue.IsSet())
	{
		OnDynamicSliderMinValueChanged(SpinBoxMinMaxSliderValues.DefaultMinSliderValue.GetValue(), nullptr, true, false);
	}
}

void FColorGradingCustomBuilder::ResetToDefault(TSharedPtr<IPropertyHandle> PropertyHandle)
{
	PropertyHandle->ResetToDefault();

	FVector4 CurrentValueVector;
	verifySlow(ColorGradingPropertyHandle.Pin()->GetValue(CurrentValueVector) == FPropertyAccess::Success);
	CurrentHSVColor = FLinearColor(CurrentValueVector.X, CurrentValueVector.Y, CurrentValueVector.Z).LinearRGBToHSV();

	OnCurrentHSVColorChanged.Broadcast(CurrentHSVColor, true);

	if (SpinBoxMinMaxSliderValues.DefaultMaxSliderValue.IsSet())
	{
		OnDynamicSliderMaxValueChanged(SpinBoxMinMaxSliderValues.DefaultMaxSliderValue.GetValue(), nullptr, true, false);
	}

	if (SpinBoxMinMaxSliderValues.DefaultMinSliderValue.IsSet())
	{
		OnDynamicSliderMinValueChanged(SpinBoxMinMaxSliderValues.DefaultMinSliderValue.GetValue(), nullptr, true, false);
	}
}

bool FColorGradingCustomBuilder::CanResetToDefault(TSharedPtr<IPropertyHandle> PropertyHandle)
{
	return PropertyHandle->DiffersFromDefault();
}

void FColorGradingCustomBuilder::Tick(float DeltaTime)
{
}

void FColorGradingCustomBuilder::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
	// Make a widget for each property.  The vector component properties  will be displayed in the header
	TOptional<float> MinValue, MaxValue, SliderMinValue, SliderMaxValue;
	float SliderExponent, Delta;
	int32 ShiftMouseMovePixelPerDelta = 1;
	bool SupportDynamicSliderMaxValue = false;
	bool SupportDynamicSliderMinValue = false;
	TSharedRef<IPropertyHandle> ColorGradingPropertyHandleRef = ColorGradingPropertyHandle.Pin().ToSharedRef();

	FMathStructCustomization::ExtractNumericMetadata<float>(ColorGradingPropertyHandleRef, MinValue, MaxValue, SliderMinValue, SliderMaxValue, SliderExponent, Delta, ShiftMouseMovePixelPerDelta, SupportDynamicSliderMaxValue, SupportDynamicSliderMinValue);	

	EColorGradingModes ColorGradingMode = GetColorGradingMode();

	NodeRow.NameContent()
		.HAlign(HAlign_Fill)
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.WidthOverride(125.0f)
			//.HeightOverride(150.0f)
			.MinDesiredWidth(125.0f)
			.MaxDesiredWidth(125.0f)
			.Padding(FMargin(2.0f, 2.0f, 2.0f, 2.0f))
			[
				SAssignNew(ColorGradingPickerWidget, SColorGradingPicker)
				.ValueMin(MinValue)
				.ValueMax(MaxValue)
				.SliderValueMin(SliderMinValue)
				.SliderValueMax(SliderMaxValue)
				.MainDelta(Delta)
				.SupportDynamicSliderMaxValue(SupportDynamicSliderMaxValue)
				.SupportDynamicSliderMinValue(SupportDynamicSliderMinValue)
				.MainShiftMouseMovePixelPerDelta(ShiftMouseMovePixelPerDelta)
				.ColorGradingModes(ColorGradingMode)
				.OnColorCommitted(this, &FColorGradingCustomBuilder::OnColorGradingPickerChanged)
				.OnQueryCurrentColor(this, &FColorGradingCustomBuilder::GetCurrentColorGradingValue)
				.AllowSpin(ColorGradingPropertyHandle.Pin()->GetNumOuterObjects() == 1)
				.OnBeginSliderMovement(this, &FColorGradingCustomBuilder::OnBeginMainValueSliderMovement)
				.OnEndSliderMovement(this, &FColorGradingCustomBuilder::OnEndMainValueSliderMovement)
			]
		];

	TSharedPtr<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	VerticalBox->AddSlot()
	.Padding(FMargin(0.0f, 2.0f, 0.0f, 2.0f))
	.VAlign(VAlign_Top)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.0f, 0.0f, 3.0f, 0.0f))
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
			.Type(ESlateCheckBoxType::ToggleButton)
			.IsChecked(this, &FColorGradingCustomBuilder::OnGetChangeColorMode, ColorModeType::RGB)
			.OnCheckStateChanged(this, &FColorGradingCustomBuilder::OnChangeColorModeClicked, ColorModeType::RGB)
			.ToolTipText(this, &FColorGradingCustomBuilder::OnChangeColorModeToolTipText, ColorModeType::RGB)
			.Visibility(this, &FColorGradingCustomBuilder::OnGetRGBHSVButtonVisibility, ColorModeType::RGB)
			.Padding(4)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &FColorGradingCustomBuilder::OnChangeColorModeText, ColorModeType::RGB)
				.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)			
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.0f, 0.0f, 3.0f, 0.0f))
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
			.Type(ESlateCheckBoxType::ToggleButton)
			.IsChecked(this, &FColorGradingCustomBuilder::OnGetChangeColorMode, ColorModeType::HSV)
			.OnCheckStateChanged(this, &FColorGradingCustomBuilder::OnChangeColorModeClicked, ColorModeType::HSV)
			.ToolTipText(this, &FColorGradingCustomBuilder::OnChangeColorModeToolTipText, ColorModeType::HSV)
			.Visibility(this, &FColorGradingCustomBuilder::OnGetRGBHSVButtonVisibility, ColorModeType::RGB)
			.Padding(4)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &FColorGradingCustomBuilder::OnChangeColorModeText, ColorModeType::HSV)
				.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
			]
		]
	];

	for (int32 ColorIndex = 0; ColorIndex < SortedChildArray.Num(); ++ColorIndex)
	{
		TWeakPtr<IPropertyHandle> WeakHandlePtr = SortedChildArray[ColorIndex];

		TSharedRef<SNumericEntryBox<float>> NumericEntryBox = MakeNumericEntryBox(ColorIndex, MinValue, MaxValue, SliderMinValue, SliderMaxValue, SliderExponent, Delta, ShiftMouseMovePixelPerDelta, SupportDynamicSliderMaxValue, SupportDynamicSliderMinValue);
		TSharedPtr<SSpinBox<float>> NumericEntrySpinBox = StaticCastSharedPtr<SSpinBox<float>>(NumericEntryBox->GetSpinBox());

		NumericEntryBoxWidgetList.Add(NumericEntryBox);

		if (NumericEntrySpinBox.IsValid())
		{
			float MinSliderValue = NumericEntrySpinBox->GetMinSliderValue();
			float MaxSliderValue = NumericEntrySpinBox->GetMaxSliderValue();

			SpinBoxMinMaxSliderValues.CurrentMinSliderValue = MinSliderValue == TNumericLimits<float>::Lowest() ? TOptional<float>() : MinSliderValue;
			SpinBoxMinMaxSliderValues.CurrentMaxSliderValue = MaxSliderValue == TNumericLimits<float>::Max() ? TOptional<float>() : MaxSliderValue;
			SpinBoxMinMaxSliderValues.DefaultMinSliderValue = SpinBoxMinMaxSliderValues.CurrentMinSliderValue;
			SpinBoxMinMaxSliderValues.DefaultMaxSliderValue = SpinBoxMinMaxSliderValues.CurrentMaxSliderValue;
		}

		VerticalBox->AddSlot()
			.Padding(FMargin(0.0f, 2.0f, 3.0f, 0.0f))
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				NumericEntryBox
			];


		// Color Box

		VerticalBox->AddSlot()
			.Padding(FMargin(15.0f, 0.0f, 3.0f, 2.0f))
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Bottom)
			[
				SNew(SBox)
				.HeightOverride(6.0f)
				[
					SNew(SComplexGradient)
						.GradientColors(TAttribute<TArray<FLinearColor>>::Create(TAttribute<TArray<FLinearColor>>::FGetter::CreateSP(this, &FColorGradingVectorCustomizationBase::GetGradientColor, ColorIndex)))
						.Visibility(this, &FColorGradingCustomBuilder::OnGetGradientVisibility)
				]
			];
	}	
	
	NodeRow.ValueContent()
		.HAlign(HAlign_Fill)
	[
		VerticalBox.ToSharedRef()
	];

	if (ParentGroup)
	{
		ParentGroup->GetOnDetailGroupReset().AddSP(this, &FColorGradingCustomBuilder::OnDetailGroupReset);
	}

	if (ColorGradingCustomization.IsValid())
	{
		OnColorModeChanged.AddSP(ColorGradingCustomization.Get(), &FColorGradingVectorCustomization::OnColorModeChanged);

		OnCurrentHSVColorChanged.AddSP(ColorGradingCustomization.Get(), &FColorGradingVectorCustomizationBase::OnCurrentHSVColorChangedDelegate);
		ColorGradingCustomization->GetOnCurrentHSVColorChangedDelegate().AddSP(this, &FColorGradingVectorCustomizationBase::OnCurrentHSVColorChangedDelegate);

		ColorGradingCustomization->GetOnNumericEntryBoxDynamicSliderMaxValueChangedDelegate().AddSP(this, &FColorGradingVectorCustomizationBase::OnDynamicSliderMaxValueChanged);
		ColorGradingCustomization->GetOnNumericEntryBoxDynamicSliderMinValueChangedDelegate().AddSP(this, &FColorGradingVectorCustomizationBase::OnDynamicSliderMinValueChanged);
		ColorGradingCustomization->GetOnNumericEntryBoxDynamicSliderMaxValueChangedDelegate().AddSP(ColorGradingPickerWidget.Pin().Get(), &SColorGradingPicker::OnDynamicSliderMaxValueChanged);
		ColorGradingCustomization->GetOnNumericEntryBoxDynamicSliderMinValueChangedDelegate().AddSP(ColorGradingPickerWidget.Pin().Get(), &SColorGradingPicker::OnDynamicSliderMinValueChanged);

		OnNumericEntryBoxDynamicSliderMaxValueChanged.AddSP(ColorGradingCustomization.Get(), &FColorGradingVectorCustomizationBase::OnDynamicSliderMaxValueChanged);
		OnNumericEntryBoxDynamicSliderMinValueChanged.AddSP(ColorGradingCustomization.Get(), &FColorGradingVectorCustomizationBase::OnDynamicSliderMinValueChanged);
	}

	if (ColorGradingPickerWidget.IsValid())
	{
		if (ColorGradingCustomization.IsValid())
		{
			ColorGradingPickerWidget.Pin()->GetOnNumericEntryBoxDynamicSliderMaxValueChangedDelegate().AddSP(ColorGradingCustomization.Get(), &FColorGradingVectorCustomizationBase::OnDynamicSliderMaxValueChanged);
			ColorGradingPickerWidget.Pin()->GetOnNumericEntryBoxDynamicSliderMinValueChangedDelegate().AddSP(ColorGradingCustomization.Get(), &FColorGradingVectorCustomizationBase::OnDynamicSliderMinValueChanged);
		}

		ColorGradingPickerWidget.Pin()->GetOnNumericEntryBoxDynamicSliderMaxValueChangedDelegate().AddSP(this, &FColorGradingVectorCustomizationBase::OnDynamicSliderMaxValueChanged);
		ColorGradingPickerWidget.Pin()->GetOnNumericEntryBoxDynamicSliderMinValueChangedDelegate().AddSP(this, &FColorGradingVectorCustomizationBase::OnDynamicSliderMinValueChanged);

		OnNumericEntryBoxDynamicSliderMaxValueChanged.AddSP(ColorGradingPickerWidget.Pin().Get(), &SColorGradingPicker::OnDynamicSliderMaxValueChanged);
		OnNumericEntryBoxDynamicSliderMinValueChanged.AddSP(ColorGradingPickerWidget.Pin().Get(), &SColorGradingPicker::OnDynamicSliderMinValueChanged);
	}

	OnCurrentHSVColorChanged.AddSP(this, &FColorGradingVectorCustomizationBase::OnCurrentHSVColorChangedDelegate);

	bool RGBMode = true;

	// Find the highest current value and propagate it to all others so they all matches
	float BestMaxSliderValue = 0.0f;
	float BestMinSliderValue = 0.0f;

	for (TWeakPtr<SWidget>& Widget : NumericEntryBoxWidgetList)
	{
		TSharedPtr<SNumericEntryBox<float>> NumericBox = StaticCastSharedPtr<SNumericEntryBox<float>>(Widget.Pin());

		if (NumericBox.IsValid())
		{
			TSharedPtr<SSpinBox<float>> SpinBox = StaticCastSharedPtr<SSpinBox<float>>(NumericBox->GetSpinBox());

			if (SpinBox.IsValid())
			{
				if (SpinBox->GetMaxSliderValue() > BestMaxSliderValue) 
				{
					BestMaxSliderValue = SpinBox->GetMaxSliderValue();
				}

				if (SpinBox->GetMinSliderValue() < BestMinSliderValue)
				{
					BestMinSliderValue = SpinBox->GetMinSliderValue();
				}
			}
		}
	}

	OnDynamicSliderMaxValueChanged(BestMaxSliderValue, nullptr, true, true);
	OnDynamicSliderMinValueChanged(BestMinSliderValue, nullptr, true, true);
	
	FString ParentGroupName = ParentGroup != nullptr ? ParentGroup->GetGroupName().ToString() : TEXT("NoParentGroup");
	ParentGroupName.ReplaceInline(TEXT(" "), TEXT("_"));
	ParentGroupName.ReplaceInline(TEXT("|"), TEXT("_"));

	GConfig->GetBool(TEXT("ColorGrading"), *FString::Printf(TEXT("%s_%s_IsRGB"), *ParentGroupName, *ColorGradingPropertyHandle.Pin()->GetPropertyDisplayName().ToString()), RGBMode, GEditorPerProjectIni);
	OnChangeColorModeClicked(ECheckBoxState::Checked, RGBMode ? ColorModeType::RGB : ColorModeType::HSV);

}

FText FColorGradingCustomBuilder::OnChangeColorModeText(ColorModeType ModeType) const
{
	FText Text;

	switch (ModeType)
	{
		case FColorGradingCustomBuilder::ColorModeType::RGB: Text = LOCTEXT("ChangeColorModeRGB", "RGB"); break;
		case FColorGradingCustomBuilder::ColorModeType::HSV: Text = LOCTEXT("ChangeColorModeHSV", "HSV"); break;
	}

	return Text;
}

FText FColorGradingCustomBuilder::OnChangeColorModeToolTipText(ColorModeType ModeType) const
{
	FText Text;

	switch (ModeType)
	{
		case FColorGradingCustomBuilder::ColorModeType::RGB: Text = LOCTEXT("ChangeColorModeRGBToolTips", "Change to RGB color mode"); break;
		case FColorGradingCustomBuilder::ColorModeType::HSV: Text = LOCTEXT("ChangeColorModeHSVToolTips", "Change to HSV color mode"); break;
	}

	return Text;
}

EVisibility FColorGradingCustomBuilder::OnGetRGBHSVButtonVisibility(ColorModeType ModeType) const
{
	return GetColorGradingMode() == EColorGradingModes::Offset ? EVisibility::Hidden : EVisibility::Visible;
}

EVisibility FColorGradingCustomBuilder::OnGetGradientVisibility() const
{
	return GetColorGradingMode() == EColorGradingModes::Offset ? EVisibility::Hidden : EVisibility::Visible;
}

void FColorGradingCustomBuilder::OnChangeColorModeClicked(ECheckBoxState NewValue, ColorModeType ModeType)
{
	FVector4 CurrentValueVector;
	if (ColorGradingPropertyHandle.Pin()->GetValue(CurrentValueVector) != FPropertyAccess::Success)
	{
		return;
	}

	bool NewIsRGBMode = true;

	switch (ModeType)
	{
		case FColorGradingCustomBuilder::ColorModeType::RGB: NewIsRGBMode = NewValue == ECheckBoxState::Checked ? true : false; break;
		case FColorGradingCustomBuilder::ColorModeType::HSV: NewIsRGBMode = NewValue == ECheckBoxState::Checked ? false : true; break;
	}

	if (NewIsRGBMode != IsRGBMode)
	{
		IsRGBMode = NewIsRGBMode;

		FString ParentGroupName = ParentGroup != nullptr ? ParentGroup->GetGroupName().ToString() : TEXT("NoParentGroup");
		ParentGroupName.ReplaceInline(TEXT(" "), TEXT("_"));
		ParentGroupName.ReplaceInline(TEXT("|"), TEXT("_"));

		GConfig->SetBool(TEXT("ColorGrading"), *FString::Printf(TEXT("%s_%s_IsRGB"), *ParentGroupName, *ColorGradingPropertyHandle.Pin()->GetPropertyDisplayName().ToString()), IsRGBMode, GEditorPerProjectIni);

		CurrentHSVColor = FLinearColor(CurrentValueVector.X, CurrentValueVector.Y, CurrentValueVector.Z).LinearRGBToHSV();

		OnCurrentHSVColorChanged.Broadcast(CurrentHSVColor, true);

		// Force Refresh of the internal cache of the spinner
		for (int32 ColorIndex = 0; ColorIndex < NumericEntryBoxWidgetList.Num(); ++ColorIndex)
		{
			TSharedPtr<SNumericEntryBox<float>> NumericBox = StaticCastSharedPtr<SNumericEntryBox<float>>(NumericEntryBoxWidgetList[ColorIndex].Pin());

			if (NumericBox.IsValid())
			{
				TSharedPtr<SSpinBox<float>> SpinBox = StaticCastSharedPtr<SSpinBox<float>>(NumericBox->GetSpinBox());

				if (SpinBox.IsValid())
				{
					SpinBox->SetValue(SpinBox->GetValueAttribute());
				}
			}
		}

		OnColorModeChanged.Broadcast(IsRGBMode);
	}
}

ECheckBoxState FColorGradingCustomBuilder::OnGetChangeColorMode(ColorModeType ModeType) const
{
	switch (ModeType)
	{
		case FColorGradingCustomBuilder::ColorModeType::RGB: return IsRGBMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		case FColorGradingCustomBuilder::ColorModeType::HSV: return !IsRGBMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		default: break;
	}

	return ECheckBoxState::Unchecked;
}

void FColorGradingCustomBuilder::OnColorGradingPickerChanged(FVector4& NewValue, bool ShouldCommitValueChanges)
{
	
	FScopedTransaction Transaction(LOCTEXT("ColorGradingMainValue", "Color Grading Main Value"),ShouldCommitValueChanges);
	if (ColorGradingPropertyHandle.IsValid())
	{
		if (ShouldCommitValueChanges && !bIsUsingSlider)
		{
			FVector4 ExistingValue;
			ColorGradingPropertyHandle.Pin()->GetValue(ExistingValue);
			if (ExistingValue != NewValue)
			{
				ColorGradingPropertyHandle.Pin()->SetValue(NewValue, ShouldCommitValueChanges ? EPropertyValueSetFlags::DefaultFlags : EPropertyValueSetFlags::InteractiveChange);
			}
			else
			{
				Transaction.Cancel();
			}
		}
		else
		{
			ColorGradingPropertyHandle.Pin()->SetValue(NewValue, ShouldCommitValueChanges ? EPropertyValueSetFlags::DefaultFlags : EPropertyValueSetFlags::InteractiveChange);
		}
	}

	FLinearColor NewHSVColor(NewValue.X, NewValue.Y, NewValue.Z);
	NewHSVColor = NewHSVColor.LinearRGBToHSV();

	OnCurrentHSVColorChangedDelegate(NewHSVColor, true);
}

bool FColorGradingCustomBuilder::GetCurrentColorGradingValue(FVector4& OutCurrentValue)
{
	return ColorGradingPropertyHandle.Pin()->GetValue(OutCurrentValue) == FPropertyAccess::Success;
}


void FColorGradingCustomBuilder::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
}

void FColorGradingCustomBuilder::OnBeginMainValueSliderMovement()
{
	bIsUsingSlider = true;
	GEditor->BeginTransaction(LOCTEXT("ColorGradingMainValue", "Color Grading Main Value"));
}

void FColorGradingCustomBuilder::OnEndMainValueSliderMovement()
{
	bIsUsingSlider = false;
	GEditor->EndTransaction();
}

#undef LOCTEXT_NAMESPACE
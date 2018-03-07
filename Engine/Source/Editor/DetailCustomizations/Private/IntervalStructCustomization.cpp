// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IntervalStructCustomization.h"
#include "Widgets/Text/STextBlock.h"
#include "Editor.h"
#include "Widgets/Layout/SBox.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "Widgets/Input/SNumericEntryBox.h"


#define LOCTEXT_NAMESPACE "IntervalStructCustomization"


/* Helper traits for getting a metadata property based on the template parameter type
 *****************************************************************************/

namespace IntervalMetadata
{
	template <typename NumericType>
	struct FGetHelper
	{ };

	template <>
	struct FGetHelper<float>
	{
		static float GetMetaData(const UProperty* Property, const TCHAR* Key)
		{
			return Property->GetFLOATMetaData(Key);
		}
	};

	template <>
	struct FGetHelper<int32>
	{
		static int32 GetMetaData(const UProperty* Property, const TCHAR* Key)
		{
			return Property->GetINTMetaData(Key);
		}
	};
}


/* FIntervalStructCustomization static interface
 *****************************************************************************/

template <typename NumericType>
TSharedRef<IPropertyTypeCustomization> FIntervalStructCustomization<NumericType>::MakeInstance()
{
	return MakeShareable(new FIntervalStructCustomization<NumericType>);
}


/* IPropertyTypeCustomization interface
 *****************************************************************************/

template <typename NumericType>
void FIntervalStructCustomization<NumericType>::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// Get handles to the properties we're interested in
	MinValueHandle = StructPropertyHandle->GetChildHandle(TEXT("Min"));
	MaxValueHandle = StructPropertyHandle->GetChildHandle(TEXT("Max"));
	check(MinValueHandle.IsValid());
	check(MaxValueHandle.IsValid());

	// Get min/max metadata values if defined
	auto Property = StructPropertyHandle->GetProperty();
	check(Property != nullptr);

	if (Property->HasMetaData(TEXT("UIMin")))
	{
		MinAllowedValue = TOptional<NumericType>(IntervalMetadata::FGetHelper<NumericType>::GetMetaData(Property, TEXT("UIMin")));
	}

	if (Property->HasMetaData(TEXT("UIMax")))
	{
		MaxAllowedValue = TOptional<NumericType>(IntervalMetadata::FGetHelper<NumericType>::GetMetaData(Property, TEXT("UIMax")));
	}

	bAllowInvertedInterval = Property->HasMetaData(TEXT("AllowInvertedInterval"));
	bClampToMinMaxLimits = Property->HasMetaData(TEXT("ClampToMinMaxLimits"));

	// Build the widgets
	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(200.0f)
	//.MaxDesiredWidth(200.0f)
	[
		SNew(SBox)
		.Padding(FMargin(0.0f, 3.0f, 0.0f, 2.0f))
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.Padding(FMargin(0.0f, 0.0f, 5.0f, 0.0f))
			.VAlign(VAlign_Center)
			[
				SNew(SNumericEntryBox<NumericType>)
				.Value(this, &FIntervalStructCustomization<NumericType>::OnGetValue, EIntervalField::Min)
				.MinValue(MinAllowedValue)
				.MinSliderValue(MinAllowedValue)
				.MaxValue(this, &FIntervalStructCustomization<NumericType>::OnGetMaxValue)
				.MaxSliderValue(this, &FIntervalStructCustomization<NumericType>::OnGetMaxValue)
				.OnValueCommitted(this, &FIntervalStructCustomization<NumericType>::OnValueCommitted, EIntervalField::Min)
				.OnValueChanged(this, &FIntervalStructCustomization<NumericType>::OnValueChanged, EIntervalField::Min)
				.OnBeginSliderMovement(this, &FIntervalStructCustomization<NumericType>::OnBeginSliderMovement)
				.OnEndSliderMovement(this, &FIntervalStructCustomization<NumericType>::OnEndSliderMovement)
				.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.AllowSpin(true)
				.LabelVAlign(VAlign_Center)
				.Label()
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("MinLabel", "Min"))
				]
			]

			+SHorizontalBox::Slot()
			.Padding(FMargin(0.0f, 0.0f, 5.0f, 0.0f))
			.VAlign(VAlign_Center)
			[
				SNew(SNumericEntryBox<NumericType>)
				.Value(this, &FIntervalStructCustomization<NumericType>::OnGetValue, EIntervalField::Max)
				.MinValue(this, &FIntervalStructCustomization<NumericType>::OnGetMinValue)
				.MinSliderValue(this, &FIntervalStructCustomization<NumericType>::OnGetMinValue)
				.MaxValue(MaxAllowedValue)
				.MaxSliderValue(MaxAllowedValue)
				.OnValueCommitted(this, &FIntervalStructCustomization<NumericType>::OnValueCommitted, EIntervalField::Max)
				.OnValueChanged(this, &FIntervalStructCustomization<NumericType>::OnValueChanged, EIntervalField::Max)
				.OnBeginSliderMovement(this, &FIntervalStructCustomization<NumericType>::OnBeginSliderMovement)
				.OnEndSliderMovement(this, &FIntervalStructCustomization<NumericType>::OnEndSliderMovement)
				.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.AllowSpin(true)
				.LabelVAlign(VAlign_Center)
				.Label()
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("MaxLabel", "Max"))
				]
			]
		]
	];
}


template <typename NumericType>
void FIntervalStructCustomization<NumericType>::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// Don't display children, as editing them directly can break the constraints
}


/* FIntervalStructCustomization callbacks
 *****************************************************************************/

template <typename NumericType>
static TOptional<NumericType> GetValue(IPropertyHandle* Handle)
{
	NumericType Value;
	if (Handle->GetValue(Value) == FPropertyAccess::Success)
	{
		return TOptional<NumericType>(Value);
	}

	return TOptional<NumericType>();
}

template <typename NumericType>
TOptional<NumericType> FIntervalStructCustomization<NumericType>::OnGetValue(EIntervalField Field) const
{
	return GetValue<NumericType>((Field == EIntervalField::Min) ? MinValueHandle.Get() : MaxValueHandle.Get());
}

template <typename NumericType>
TOptional<NumericType> FIntervalStructCustomization<NumericType>::OnGetMinValue() const
{
	if (bClampToMinMaxLimits)
	{
		return GetValue<NumericType>(MinValueHandle.Get());
	}

	return MinAllowedValue;
}

template <typename NumericType>
TOptional<NumericType> FIntervalStructCustomization<NumericType>::OnGetMaxValue() const
{
	if (bClampToMinMaxLimits)
	{
		return GetValue<NumericType>(MaxValueHandle.Get());
	}

	return MaxAllowedValue;
}

template <typename NumericType>
void FIntervalStructCustomization<NumericType>::SetValue(NumericType NewValue, EIntervalField Field, EPropertyValueSetFlags::Type Flags)
{
	IPropertyHandle* Handle = (Field == EIntervalField::Min) ? MinValueHandle.Get() : MaxValueHandle.Get();
	IPropertyHandle* OtherHandle = (Field == EIntervalField::Min) ? MaxValueHandle.Get() : MinValueHandle.Get();

	const TOptional<NumericType> OtherValue = GetValue<NumericType>(OtherHandle);
	const bool bOutOfRange = OtherValue.IsSet() && ((Field == EIntervalField::Min && NewValue > OtherValue.GetValue()) || (Field == EIntervalField::Max && NewValue < OtherValue.GetValue()));

	// The order of execution of the Intertactive change vs commit is super important for the Undo history
	// So Interactive we must do, Handle, Other Handle
	// For Commit: we must do the reverse to properly close the scope of traction
	if (!bOutOfRange || bAllowInvertedInterval)
	{
		if (Flags == EPropertyValueSetFlags::InteractiveChange)
		{
			ensure(Handle->SetValue(NewValue, Flags) == FPropertyAccess::Success);

			if (OtherValue.IsSet())
			{
				ensure(OtherHandle->SetValue(OtherValue.GetValue(), Flags) == FPropertyAccess::Success);
			}
		}
		else
		{
			if (OtherValue.IsSet())
			{
				ensure(OtherHandle->SetValue(OtherValue.GetValue(), Flags) == FPropertyAccess::Success);
			}

			ensure(Handle->SetValue(NewValue, Flags) == FPropertyAccess::Success);
		}
	}
	else if (!bClampToMinMaxLimits)
	{
		if (Flags == EPropertyValueSetFlags::InteractiveChange)
		{
			ensure(Handle->SetValue(NewValue, Flags) == FPropertyAccess::Success);
			ensure(OtherHandle->SetValue(NewValue, Flags) == FPropertyAccess::Success);
		}
		else
		{
			ensure(OtherHandle->SetValue(NewValue, Flags) == FPropertyAccess::Success);
			ensure(Handle->SetValue(NewValue, Flags) == FPropertyAccess::Success);
		}
	}
}

template <typename NumericType>
void FIntervalStructCustomization<NumericType>::OnValueCommitted(NumericType NewValue, ETextCommit::Type CommitType, EIntervalField Field)
{
	if (!bIsUsingSlider || (bIsUsingSlider && ShouldAllowSpin()))
	{
		SetValue(NewValue, Field);
	}
}

template <typename NumericType>
void FIntervalStructCustomization<NumericType>::OnValueChanged(NumericType NewValue, EIntervalField Field)
{
	if (bIsUsingSlider && ShouldAllowSpin())
	{
		SetValue(NewValue, Field, EPropertyValueSetFlags::InteractiveChange);
	}
}


template <typename NumericType>
void FIntervalStructCustomization<NumericType>::OnBeginSliderMovement()
{
	bIsUsingSlider = true;

	if (ShouldAllowSpin())
	{
		GEditor->BeginTransaction(LOCTEXT("SetIntervalProperty", "Set Interval Property"));
	}
}


template <typename NumericType>
void FIntervalStructCustomization<NumericType>::OnEndSliderMovement(NumericType /*NewValue*/)
{
	bIsUsingSlider = false;

	if (ShouldAllowSpin())
	{
		GEditor->EndTransaction();
	}
}

template <typename NumericType>
bool FIntervalStructCustomization<NumericType>::ShouldAllowSpin() const
{
	return true;
}

/* Only explicitly instantiate the types which are supported
 *****************************************************************************/

template class FIntervalStructCustomization<float>;
template class FIntervalStructCustomization<int32>;


#undef LOCTEXT_NAMESPACE

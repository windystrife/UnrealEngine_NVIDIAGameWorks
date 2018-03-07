// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MathStructCustomizations.h"
#include "UObject/UnrealType.h"
#include "Widgets/Text/STextBlock.h"
#include "Editor.h"
#include "Misc/ConfigCacheIni.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "Widgets/Input/SNumericEntryBox.h"


#define LOCTEXT_NAMESPACE "FMathStructCustomization"


TSharedRef<IPropertyTypeCustomization> FMathStructCustomization::MakeInstance() 
{
	return MakeShareable(new FMathStructCustomization);
}


void FMathStructCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	GetSortedChildren(StructPropertyHandle, SortedChildHandles);
	MakeHeaderRow(StructPropertyHandle, HeaderRow);
}


void FMathStructCustomization::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	for (int32 ChildIndex = 0; ChildIndex < SortedChildHandles.Num(); ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = SortedChildHandles[ChildIndex];

		// Add the individual properties as children as well so the vector can be expanded for more room
		StructBuilder.AddProperty(ChildHandle);
	}
}


void FMathStructCustomization::MakeHeaderRow(TSharedRef<class IPropertyHandle>& StructPropertyHandle, FDetailWidgetRow& Row)
{
	// We'll set up reset to default ourselves
	const bool bDisplayResetToDefault = false;
	const FText DisplayNameOverride = FText::GetEmpty();
	const FText DisplayToolTipOverride = FText::GetEmpty();

	TWeakPtr<IPropertyHandle> StructWeakHandlePtr = StructPropertyHandle;

	TSharedPtr<SHorizontalBox> HorizontalBox;

	Row.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget(DisplayNameOverride, DisplayToolTipOverride, bDisplayResetToDefault)
	]
	.ValueContent()
	// Make enough space for each child handle
	.MinDesiredWidth(125.0f * SortedChildHandles.Num())
	.MaxDesiredWidth(125.0f * SortedChildHandles.Num())
	[
		SAssignNew(HorizontalBox, SHorizontalBox)
		.IsEnabled(this, &FMathStructCustomization::IsValueEnabled, StructWeakHandlePtr)
	];

	for (int32 ChildIndex = 0; ChildIndex < SortedChildHandles.Num(); ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = SortedChildHandles[ChildIndex];

		const bool bLastChild = SortedChildHandles.Num()-1 == ChildIndex;
		// Make a widget for each property.  The vector component properties  will be displayed in the header

		TSharedRef<SWidget> NumericEntryBox = MakeChildWidget(StructPropertyHandle, ChildHandle);
		NumericEntryBoxWidgetList.Add(NumericEntryBox);

		HorizontalBox->AddSlot()
		.Padding(FMargin(0.0f, 2.0f, bLastChild ? 0.0f : 3.0f, 2.0f))
		[
			NumericEntryBox
		];
	}

	if (StructPropertyHandle->GetProperty()->HasMetaData("AllowPreserveRatio"))
	{
		if (!GConfig->GetBool(TEXT("SelectionDetails"), *(StructPropertyHandle->GetProperty()->GetName() + TEXT("_PreserveScaleRatio")), bPreserveScaleRatio, GEditorPerProjectIni))
		{
			bPreserveScaleRatio = true;
		}

		HorizontalBox->AddSlot()
		.AutoWidth()
		.MaxWidth(18.0f)
		[
			// Add a checkbox to toggle between preserving the ratio of x,y,z components of scale when a value is entered
			SNew(SCheckBox)
			.IsChecked(this, &FMathStructCustomization::IsPreserveScaleRatioChecked)
			.OnCheckStateChanged(this, &FMathStructCustomization::OnPreserveScaleRatioToggled, StructWeakHandlePtr)
			.Style(FEditorStyle::Get(), "TransparentCheckBox")
			.ToolTipText(LOCTEXT("PreserveScaleToolTip", "When locked, scales uniformly based on the current xyz scale values so the object maintains its shape in each direction when scaled"))
			[
				SNew(SImage)
				.Image(this, &FMathStructCustomization::GetPreserveScaleRatioImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];
	}
}


const FSlateBrush* FMathStructCustomization::GetPreserveScaleRatioImage() const
{
	return bPreserveScaleRatio ? FEditorStyle::GetBrush(TEXT("GenericLock")) : FEditorStyle::GetBrush(TEXT("GenericUnlock"));
}


ECheckBoxState FMathStructCustomization::IsPreserveScaleRatioChecked() const
{
	return bPreserveScaleRatio ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}


void FMathStructCustomization::OnPreserveScaleRatioToggled(ECheckBoxState NewState, TWeakPtr<IPropertyHandle> PropertyHandle)
{
	bPreserveScaleRatio = (NewState == ECheckBoxState::Checked) ? true : false;

	if (PropertyHandle.IsValid() && PropertyHandle.Pin()->GetProperty())
	{
		FString SettingKey = (PropertyHandle.Pin()->GetProperty()->GetName() + TEXT("_PreserveScaleRatio"));
		GConfig->SetBool(TEXT("SelectionDetails"), *SettingKey, bPreserveScaleRatio, GEditorPerProjectIni);
	}
}


void FMathStructCustomization::GetSortedChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, TArray< TSharedRef<IPropertyHandle> >& OutChildren)
{
	OutChildren.Empty();

	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		OutChildren.Add(StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef());
	}
}


template<typename NumericType>
void FMathStructCustomization::ExtractNumericMetadata(TSharedRef<IPropertyHandle>& PropertyHandle, TOptional<NumericType>& MinValue, TOptional<NumericType>& MaxValue, TOptional<NumericType>& SliderMinValue, TOptional<NumericType>& SliderMaxValue, NumericType& SliderExponent, NumericType& Delta, int32 &ShiftMouseMovePixelPerDelta, bool& SupportDynamicSliderMaxValue, bool& SupportDynamicSliderMinValue)
{
	UProperty* Property = PropertyHandle->GetProperty();

	const FString& MetaUIMinString = Property->GetMetaData(TEXT("UIMin"));
	const FString& MetaUIMaxString = Property->GetMetaData(TEXT("UIMax"));
	const FString& SliderExponentString = Property->GetMetaData(TEXT("SliderExponent"));
	const FString& DeltaString = Property->GetMetaData(TEXT("Delta"));
	const FString& ShiftMouseMovePixelPerDeltaString = Property->GetMetaData(TEXT("ShiftMouseMovePixelPerDelta"));
	const FString& SupportDynamicSliderMaxValueString = Property->GetMetaData(TEXT("SupportDynamicSliderMaxValue"));
	const FString& SupportDynamicSliderMinValueString = Property->GetMetaData(TEXT("SupportDynamicSliderMinValue"));
	const FString& ClampMinString = Property->GetMetaData(TEXT("ClampMin"));
	const FString& ClampMaxString = Property->GetMetaData(TEXT("ClampMax"));

	// If no UIMin/Max was specified then use the clamp string
	const FString& UIMinString = MetaUIMinString.Len() ? MetaUIMinString : ClampMinString;
	const FString& UIMaxString = MetaUIMaxString.Len() ? MetaUIMaxString : ClampMaxString;

	NumericType ClampMin = TNumericLimits<NumericType>::Lowest();
	NumericType ClampMax = TNumericLimits<NumericType>::Max();

	if (!ClampMinString.IsEmpty())
	{
		TTypeFromString<NumericType>::FromString(ClampMin, *ClampMinString);
	}

	if (!ClampMaxString.IsEmpty())
	{
		TTypeFromString<NumericType>::FromString(ClampMax, *ClampMaxString);
	}

	NumericType UIMin = TNumericLimits<NumericType>::Lowest();
	NumericType UIMax = TNumericLimits<NumericType>::Max();
	TTypeFromString<NumericType>::FromString(UIMin, *UIMinString);
	TTypeFromString<NumericType>::FromString(UIMax, *UIMaxString);

	SliderExponent = NumericType(1);

	if (SliderExponentString.Len())
	{
		TTypeFromString<NumericType>::FromString(SliderExponent, *SliderExponentString);
	}

	Delta = NumericType(0);

	if (DeltaString.Len())
	{
		TTypeFromString<NumericType>::FromString(Delta, *DeltaString);
	}

	ShiftMouseMovePixelPerDelta = 1;
	if (ShiftMouseMovePixelPerDeltaString.Len())
	{
		TTypeFromString<int32>::FromString(ShiftMouseMovePixelPerDelta, *ShiftMouseMovePixelPerDeltaString);
		//The value should be greater or equal to 1
		// 1 is neutral since it is a multiplier of the mouse drag pixel
		if (ShiftMouseMovePixelPerDelta < 1)
		{
			ShiftMouseMovePixelPerDelta = 1;
		}
	}

	if (ClampMin >= ClampMax && (ClampMinString.Len() || ClampMaxString.Len()))
	{
		//UE_LOG(LogPropertyNode, Warning, TEXT("Clamp Min (%s) >= Clamp Max (%s) for Ranged Numeric"), *ClampMinString, *ClampMaxString);
	}

	const NumericType ActualUIMin = FMath::Max(UIMin, ClampMin);
	const NumericType ActualUIMax = FMath::Min(UIMax, ClampMax);

	MinValue = ClampMinString.Len() ? ClampMin : TOptional<NumericType>();
	MaxValue = ClampMaxString.Len() ? ClampMax : TOptional<NumericType>();
	SliderMinValue = (UIMinString.Len()) ? ActualUIMin : TOptional<NumericType>();
	SliderMaxValue = (UIMaxString.Len()) ? ActualUIMax : TOptional<NumericType>();

	if (ActualUIMin >= ActualUIMax && (MetaUIMinString.Len() || MetaUIMaxString.Len()))
	{
		//UE_LOG(LogPropertyNode, Warning, TEXT("UI Min (%s) >= UI Max (%s) for Ranged Numeric"), *UIMinString, *UIMaxString);
	}
	
	SupportDynamicSliderMaxValue = SupportDynamicSliderMaxValueString.Len() > 0 && SupportDynamicSliderMaxValueString.ToBool();
	SupportDynamicSliderMinValue = SupportDynamicSliderMinValueString.Len() > 0 && SupportDynamicSliderMinValueString.ToBool();
}


template<typename NumericType>
TSharedRef<SWidget> FMathStructCustomization::MakeNumericWidget(
	TSharedRef<IPropertyHandle>& StructurePropertyHandle,
	TSharedRef<IPropertyHandle>& PropertyHandle)
{
	TOptional<NumericType> MinValue, MaxValue, SliderMinValue, SliderMaxValue;
	NumericType SliderExponent, Delta;
	int32 ShiftMouseMovePixelPerDelta = 1;
	bool SupportDynamicSliderMaxValue = false;
	bool SupportDynamicSliderMinValue = false;

	ExtractNumericMetadata(StructurePropertyHandle, MinValue, MaxValue, SliderMinValue, SliderMaxValue, SliderExponent, Delta, ShiftMouseMovePixelPerDelta, SupportDynamicSliderMaxValue, SupportDynamicSliderMinValue);

	TWeakPtr<IPropertyHandle> WeakHandlePtr = PropertyHandle;

	return SNew(SNumericEntryBox<NumericType>)
		.IsEnabled(this, &FMathStructCustomization::IsValueEnabled, WeakHandlePtr)
		.EditableTextBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
		.Value(this, &FMathStructCustomization::OnGetValue, WeakHandlePtr)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.UndeterminedString(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"))
		.OnValueCommitted(this, &FMathStructCustomization::OnValueCommitted<NumericType>, WeakHandlePtr)
		.OnValueChanged(this, &FMathStructCustomization::OnValueChanged<NumericType>, WeakHandlePtr)
		.OnBeginSliderMovement(this, &FMathStructCustomization::OnBeginSliderMovement)
		.OnEndSliderMovement(this, &FMathStructCustomization::OnEndSliderMovement<NumericType>)
		.LabelVAlign(VAlign_Center)
		// Only allow spin on handles with one object.  Otherwise it is not clear what value to spin
		.AllowSpin(PropertyHandle->GetNumOuterObjects() < 2)
		.ShiftMouseMovePixelPerDelta(ShiftMouseMovePixelPerDelta)
		.SupportDynamicSliderMaxValue(SupportDynamicSliderMaxValue)
		.SupportDynamicSliderMinValue(SupportDynamicSliderMinValue)
		.OnDynamicSliderMaxValueChanged(this, &FMathStructCustomization::OnDynamicSliderMaxValueChanged<NumericType>)
		.OnDynamicSliderMinValueChanged(this, &FMathStructCustomization::OnDynamicSliderMinValueChanged<NumericType>)
		.MinValue(MinValue)
		.MaxValue(MaxValue)
		.MinSliderValue(SliderMinValue)
		.MaxSliderValue(SliderMaxValue)
		.SliderExponent(SliderExponent)
		.Delta(Delta)
		.Label()
		[
			SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(PropertyHandle->GetPropertyDisplayName())
		];
}

template <typename NumericType>
void FMathStructCustomization::OnDynamicSliderMaxValueChanged(NumericType NewMaxSliderValue, TWeakPtr<SWidget> InValueChangedSourceWidget, bool IsOriginator, bool UpdateOnlyIfHigher)
{
	for (TWeakPtr<SWidget>& Widget : NumericEntryBoxWidgetList)
	{
		TSharedPtr<SNumericEntryBox<NumericType>> NumericBox = StaticCastSharedPtr<SNumericEntryBox<NumericType>>(Widget.Pin());

		if (NumericBox.IsValid())
		{
			TSharedPtr<SSpinBox<NumericType>> SpinBox = StaticCastSharedPtr<SSpinBox<NumericType>>(NumericBox->GetSpinBox());

			if (SpinBox.IsValid())
			{
				if (SpinBox != InValueChangedSourceWidget)
				{
					if ((NewMaxSliderValue > SpinBox->GetMaxSliderValue() && UpdateOnlyIfHigher) || !UpdateOnlyIfHigher)
					{
						// Make sure the max slider value is not a getter otherwise we will break the link!
						verifySlow(!SpinBox->IsMaxSliderValueBound());
						SpinBox->SetMaxSliderValue(NewMaxSliderValue);
					}
				}
			}
		}
	}

	if (IsOriginator)
	{
		OnNumericEntryBoxDynamicSliderMaxValueChanged.Broadcast((float)NewMaxSliderValue, InValueChangedSourceWidget, false, UpdateOnlyIfHigher);
	}
}

template <typename NumericType>
void FMathStructCustomization::OnDynamicSliderMinValueChanged(NumericType NewMinSliderValue, TWeakPtr<SWidget> InValueChangedSourceWidget, bool IsOriginator, bool UpdateOnlyIfLower)
{
	for (TWeakPtr<SWidget>& Widget : NumericEntryBoxWidgetList)
	{
		TSharedPtr<SNumericEntryBox<NumericType>> NumericBox = StaticCastSharedPtr<SNumericEntryBox<NumericType>>(Widget.Pin());

		if (NumericBox.IsValid())
		{
			TSharedPtr<SSpinBox<NumericType>> SpinBox = StaticCastSharedPtr<SSpinBox<NumericType>>(NumericBox->GetSpinBox());

			if (SpinBox.IsValid())
			{
				if (SpinBox != InValueChangedSourceWidget)
				{
					if ((NewMinSliderValue < SpinBox->GetMinSliderValue() && UpdateOnlyIfLower) || !UpdateOnlyIfLower)
					{
						// Make sure the min slider value is not a getter otherwise we will break the link!
						verifySlow(!SpinBox->IsMinSliderValueBound());
						SpinBox->SetMinSliderValue(NewMinSliderValue);
					}
				}
			}
		}
	}

	if (IsOriginator)
	{
		OnNumericEntryBoxDynamicSliderMinValueChanged.Broadcast((float)NewMinSliderValue, InValueChangedSourceWidget, false, UpdateOnlyIfLower);
	}
}

TSharedRef<SWidget> FMathStructCustomization::MakeChildWidget(
	TSharedRef<IPropertyHandle>& StructurePropertyHandle,
	TSharedRef<IPropertyHandle>& PropertyHandle)
{
	const UClass* PropertyClass = PropertyHandle->GetPropertyClass();
	
	if (PropertyClass == UFloatProperty::StaticClass())
	{
		return MakeNumericWidget<float>(StructurePropertyHandle, PropertyHandle);
	}
	
	if (PropertyClass == UIntProperty::StaticClass())
	{
		return MakeNumericWidget<int32>(StructurePropertyHandle, PropertyHandle);
	}

	if (PropertyClass == UByteProperty::StaticClass())
	{
		return MakeNumericWidget<uint8>(StructurePropertyHandle, PropertyHandle);
	}

	if (PropertyClass == UEnumProperty::StaticClass())
	{
		const UEnumProperty* EnumPropertyClass = static_cast<const UEnumProperty*>(PropertyHandle->GetProperty());
		const UProperty* Enum = EnumPropertyClass->GetUnderlyingProperty();
		const UClass* EnumClass = Enum->GetClass();
		if (EnumClass == UByteProperty::StaticClass())
		{
			return MakeNumericWidget<uint8>(StructurePropertyHandle, PropertyHandle);
		}
		else if (EnumClass == UIntProperty::StaticClass())
		{
			return MakeNumericWidget<int32>(StructurePropertyHandle, PropertyHandle);
		}
	}

	check(0); // Unsupported class
	return SNullWidget::NullWidget;
}


template<typename NumericType>
TOptional<NumericType> FMathStructCustomization::OnGetValue(TWeakPtr<IPropertyHandle> WeakHandlePtr) const
{
	NumericType NumericVal = 0;
	if (WeakHandlePtr.Pin()->GetValue(NumericVal) == FPropertyAccess::Success)
	{
		return TOptional<NumericType>(NumericVal);
	}

	// Value couldn't be accessed.  Return an unset value
	return TOptional<NumericType>();
}


template<typename NumericType>
void FMathStructCustomization::OnValueCommitted(NumericType NewValue, ETextCommit::Type CommitType, TWeakPtr<IPropertyHandle> WeakHandlePtr)
{
	EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::DefaultFlags;
	SetValue(NewValue, Flags, WeakHandlePtr);
}


template<typename NumericType>
void FMathStructCustomization::OnValueChanged(NumericType NewValue, TWeakPtr<IPropertyHandle> WeakHandlePtr)
{
	if (bIsUsingSlider)
	{
		EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::InteractiveChange;
		SetValue(NewValue, Flags, WeakHandlePtr);
	}
}


template<typename NumericType>
void FMathStructCustomization::SetValue(NumericType NewValue, EPropertyValueSetFlags::Type Flags, TWeakPtr<IPropertyHandle> WeakHandlePtr)
{
	if (bPreserveScaleRatio)
	{
		// Get the value for each object for the modified component
		TArray<FString> OldValues;
		if (WeakHandlePtr.Pin()->GetPerObjectValues(OldValues) == FPropertyAccess::Success)
		{
			// Loop through each object and scale based on the new ratio for each object individually
			for (int32 OutputIndex = 0; OutputIndex < OldValues.Num(); ++OutputIndex)
			{
				NumericType OldValue;
				TTypeFromString<NumericType>::FromString(OldValue, *OldValues[OutputIndex]);

				// Account for the previous scale being zero.  Just set to the new value in that case?
				NumericType Ratio = OldValue == 0 ? NewValue : NewValue / OldValue;
				if (Ratio == 0)
				{
					Ratio = NewValue;
				}

				// Loop through all the child handles (each component of the math struct, like X, Y, Z...etc)
				for (int32 ChildIndex = 0; ChildIndex < SortedChildHandles.Num(); ++ChildIndex)
				{
					// Ignore scaling our selves.
					TSharedRef<IPropertyHandle> ChildHandle = SortedChildHandles[ChildIndex];
					if (ChildHandle != WeakHandlePtr.Pin())
					{
						// Get the value for each object.
						TArray<FString> ObjectChildValues;
						if (ChildHandle->GetPerObjectValues(ObjectChildValues) == FPropertyAccess::Success)
						{
							// Individually scale each object's components by the same ratio.
							for (int32 ChildOutputIndex = 0; ChildOutputIndex < ObjectChildValues.Num(); ++ChildOutputIndex)
							{
								NumericType ChildOldValue;
								TTypeFromString<NumericType>::FromString(ChildOldValue, *ObjectChildValues[ChildOutputIndex]);

								NumericType ChildNewValue = ChildOldValue * Ratio;
								ObjectChildValues[ChildOutputIndex] = TTypeToString<NumericType>::ToSanitizedString(ChildNewValue);
							}

							ChildHandle->SetPerObjectValues(ObjectChildValues);
						}
					}
				}
			}
		}
	}

	WeakHandlePtr.Pin()->SetValue(NewValue, Flags);
}


bool FMathStructCustomization::IsValueEnabled(TWeakPtr<IPropertyHandle> WeakHandlePtr) const
{
	if (WeakHandlePtr.IsValid())
	{
		return !WeakHandlePtr.Pin()->IsEditConst();
	}

	return false;
}


void FMathStructCustomization::OnBeginSliderMovement()
{
	bIsUsingSlider = true;

	GEditor->BeginTransaction(LOCTEXT("SetVectorProperty", "Set Vector Property"));
}


template<typename NumericType>
void FMathStructCustomization::OnEndSliderMovement(NumericType NewValue)
{
	bIsUsingSlider = false;

	GEditor->EndTransaction();
}


#undef LOCTEXT_NAMESPACE

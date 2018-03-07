// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MathStructProxyCustomizations.h"
#include "Framework/Commands/UIAction.h"
#include "UObject/UnrealType.h"
#include "Editor.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "IPropertyUtilities.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "HAL/PlatformApplicationMisc.h"

void FMathStructProxyCustomization::CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	PropertyUtilities = StructCustomizationUtils.GetPropertyUtilities();
}

void FMathStructProxyCustomization::MakeHeaderRow( TSharedRef<class IPropertyHandle>& StructPropertyHandle, FDetailWidgetRow& Row )
{

}

template<typename ProxyType, typename NumericType>
TSharedRef<SWidget> FMathStructProxyCustomization::MakeNumericProxyWidget(TSharedRef<IPropertyHandle>& StructPropertyHandle, TSharedRef< TProxyProperty<ProxyType, NumericType> >& ProxyValue, const FText& Label, bool bRotationInDegrees, const FLinearColor& LabelColor, const FLinearColor& LabelBackgroundColor)
{
	TWeakPtr<IPropertyHandle> WeakHandlePtr = StructPropertyHandle;

	return 
		SNew( SNumericEntryBox<NumericType> )
		.Value( this, &FMathStructProxyCustomization::OnGetValue<ProxyType, NumericType>, WeakHandlePtr, ProxyValue )
		.Font( IDetailLayoutBuilder::GetDetailFont() )
		.UndeterminedString( NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values") )
		.OnValueCommitted( this, &FMathStructProxyCustomization::OnValueCommitted<ProxyType, NumericType>, WeakHandlePtr, ProxyValue )
		.OnValueChanged( this, &FMathStructProxyCustomization::OnValueChanged<ProxyType, NumericType>, WeakHandlePtr, ProxyValue )
		.OnBeginSliderMovement( this, &FMathStructProxyCustomization::OnBeginSliderMovement )
		.OnEndSliderMovement( this, &FMathStructProxyCustomization::OnEndSliderMovement<ProxyType, NumericType>, WeakHandlePtr, ProxyValue )
		.LabelVAlign(VAlign_Fill)
		.LabelPadding(0)
		// Only allow spin on handles with one object.  Otherwise it is not clear what value to spin
		.AllowSpin( StructPropertyHandle->GetNumOuterObjects() == 1 )
		.MinValue(TOptional<NumericType>())
		.MaxValue(TOptional<NumericType>())
		.MaxSliderValue(bRotationInDegrees ? 360.0f : TOptional<NumericType>())
		.MinSliderValue(bRotationInDegrees ? 0.0f : TOptional<NumericType>())
		.Label()
		[
			SNumericEntryBox<float>::BuildLabel( Label, LabelColor, LabelBackgroundColor )
		];
}


template<typename ProxyType, typename NumericType>
TOptional<NumericType> FMathStructProxyCustomization::OnGetValue( TWeakPtr<IPropertyHandle> WeakHandlePtr, TSharedRef< TProxyProperty<ProxyType, NumericType> > ProxyValue ) const
{
	if(CacheValues(WeakHandlePtr))
	{
		return ProxyValue->Get();
	}
	return TOptional<NumericType>();
}

template<typename ProxyType, typename NumericType>
void FMathStructProxyCustomization::OnValueCommitted( NumericType NewValue, ETextCommit::Type CommitType, TWeakPtr<IPropertyHandle> WeakHandlePtr, TSharedRef< TProxyProperty<ProxyType, NumericType> > ProxyValue )
{
	if (!bIsUsingSlider && !GIsTransacting)
	{
		ProxyValue->Set(NewValue);
		FlushValues(WeakHandlePtr);
	}
}	

template<typename ProxyType, typename NumericType>
void FMathStructProxyCustomization::OnValueChanged( NumericType NewValue, TWeakPtr<IPropertyHandle> WeakHandlePtr, TSharedRef< TProxyProperty<ProxyType, NumericType> > ProxyValue )
{
	if( bIsUsingSlider )
	{
		ProxyValue->Set(NewValue);
		FlushValues(WeakHandlePtr);
	}
}

void FMathStructProxyCustomization::OnBeginSliderMovement()
{
	bIsUsingSlider = true;
}

template<typename ProxyType, typename NumericType>
void FMathStructProxyCustomization::OnEndSliderMovement( NumericType NewValue, TWeakPtr<IPropertyHandle> WeakHandlePtr, TSharedRef< TProxyProperty<ProxyType, NumericType> > ProxyValue )
{
	bIsUsingSlider = false;

	ProxyValue->Set(NewValue);
	FlushValues(WeakHandlePtr);
}


#define LOCTEXT_NAMESPACE "MatrixStructCustomization"

TSharedRef<IPropertyTypeCustomization> FMatrixStructCustomization::MakeInstance()
{
	return MakeShareable( new FMatrixStructCustomization );
}

void FMatrixStructCustomization::MakeHeaderRow(TSharedRef<class IPropertyHandle>& StructPropertyHandle, FDetailWidgetRow& Row)
{
	Row
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(0.0f)
	.MaxDesiredWidth(0.0f)
	[
		SNullWidget::NullWidget
	];
}

void FMatrixStructCustomization::CustomizeLocation(TSharedRef<class IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& Row)
{
	TWeakPtr<IPropertyHandle> WeakHandlePtr = StructPropertyHandle;

	Row
	.CopyAction(FUIAction(FExecuteAction::CreateSP(this, &FMatrixStructCustomization::OnCopy, FTransformField::Location, WeakHandlePtr)))
	.PasteAction(FUIAction(FExecuteAction::CreateSP(this, &FMatrixStructCustomization::OnPaste, FTransformField::Location, WeakHandlePtr)))
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget(LOCTEXT("LocationLabel", "Location"))
	]
	.ValueContent()
	.MinDesiredWidth(375.0f)
	.MaxDesiredWidth(375.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(FMargin(0.0f, 2.0f, 3.0f, 2.0f))
		[
			MakeNumericProxyWidget<FVector, float>(StructPropertyHandle, CachedTranslationX, LOCTEXT("TranslationX", "X"), false, FLinearColor::White, SNumericEntryBox<float>::RedLabelBackgroundColor)
		]
		+ SHorizontalBox::Slot()
		.Padding(FMargin(0.0f, 2.0f, 3.0f, 2.0f))
		[
			MakeNumericProxyWidget<FVector, float>(StructPropertyHandle, CachedTranslationY, LOCTEXT("TranslationY", "Y"), false, FLinearColor::White, SNumericEntryBox<float>::GreenLabelBackgroundColor)
		]
		+ SHorizontalBox::Slot()
		.Padding(FMargin(0.0f, 2.0f, 0.0f, 2.0f))
		[
			MakeNumericProxyWidget<FVector, float>(StructPropertyHandle, CachedTranslationZ, LOCTEXT("TranslationZ", "Z"), false, FLinearColor::White, SNumericEntryBox<float>::BlueLabelBackgroundColor)
		]
	];
}


void FMatrixStructCustomization::CustomizeRotation(TSharedRef<class IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& Row)
{
	TWeakPtr<IPropertyHandle> WeakHandlePtr = StructPropertyHandle;

	Row
	.CopyAction(FUIAction(FExecuteAction::CreateSP(this, &FMatrixStructCustomization::OnCopy, FTransformField::Rotation, WeakHandlePtr)))
	.PasteAction(FUIAction(FExecuteAction::CreateSP(this, &FMatrixStructCustomization::OnPaste, FTransformField::Rotation, WeakHandlePtr)))
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget(LOCTEXT("RotationLabel", "Rotation"))
	]
	.ValueContent()
	.MinDesiredWidth(375.0f)
	.MaxDesiredWidth(375.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(FMargin(0.0f, 2.0f, 3.0f, 2.0f))
		[
			MakeNumericProxyWidget<FRotator, float>(StructPropertyHandle, CachedRotationRoll, LOCTEXT("RotationRoll", "X"), true, FLinearColor::White, SNumericEntryBox<float>::RedLabelBackgroundColor)
		]
		+ SHorizontalBox::Slot()
		.Padding(FMargin(0.0f, 2.0f, 3.0f, 2.0f))
		[
			MakeNumericProxyWidget<FRotator, float>(StructPropertyHandle, CachedRotationPitch, LOCTEXT("RotationPitch", "Y"), true, FLinearColor::White, SNumericEntryBox<float>::GreenLabelBackgroundColor)
		]
		+ SHorizontalBox::Slot()
		.Padding(FMargin(0.0f, 2.0f, 0.0f, 2.0f))
		[
			MakeNumericProxyWidget<FRotator, float>(StructPropertyHandle, CachedRotationYaw, LOCTEXT("RotationYaw", "Z"), true, FLinearColor::White, SNumericEntryBox<float>::BlueLabelBackgroundColor)
		]
	];
}


void FMatrixStructCustomization::CustomizeScale(TSharedRef<class IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& Row)
{
	TWeakPtr<IPropertyHandle> WeakHandlePtr = StructPropertyHandle;

	Row
	.CopyAction(FUIAction(FExecuteAction::CreateSP(this, &FMatrixStructCustomization::OnCopy, FTransformField::Scale, WeakHandlePtr)))
	.PasteAction(FUIAction(FExecuteAction::CreateSP(this, &FMatrixStructCustomization::OnPaste, FTransformField::Scale, WeakHandlePtr)))
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget(LOCTEXT("ScaleLabel", "Scale"))
	]
	.ValueContent()
	.MinDesiredWidth(375.0f)
	.MaxDesiredWidth(375.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(FMargin(0.0f, 2.0f, 3.0f, 2.0f))
		[
			MakeNumericProxyWidget<FVector, float>(StructPropertyHandle, CachedScaleX, LOCTEXT("ScaleX", "X"), false, FLinearColor::White, SNumericEntryBox<float>::RedLabelBackgroundColor)
		]
		+ SHorizontalBox::Slot()
		.Padding(FMargin(0.0f, 2.0f, 3.0f, 2.0f))
		[
			MakeNumericProxyWidget<FVector, float>(StructPropertyHandle, CachedScaleY, LOCTEXT("ScaleY", "Y"), false, FLinearColor::White, SNumericEntryBox<float>::GreenLabelBackgroundColor)
		]
		+ SHorizontalBox::Slot()
		.Padding(FMargin(0.0f, 2.0f, 0.0f, 2.0f))
		[
			MakeNumericProxyWidget<FVector, float>(StructPropertyHandle, CachedScaleZ, LOCTEXT("ScaleZ", "Z"), false, FLinearColor::White, SNumericEntryBox<float>::BlueLabelBackgroundColor)
		]
	];
}

void FMatrixStructCustomization::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	FMathStructProxyCustomization::CustomizeChildren(StructPropertyHandle, StructBuilder, StructCustomizationUtils);

	TWeakPtr<IPropertyHandle> WeakHandlePtr = StructPropertyHandle;

	CustomizeLocation(StructPropertyHandle, StructBuilder.AddCustomRow(LOCTEXT("RotationLabel", "Rotation")));
	CustomizeRotation(StructPropertyHandle, StructBuilder.AddCustomRow(LOCTEXT("LocationLabel", "Location")));
	CustomizeScale(StructPropertyHandle, StructBuilder.AddCustomRow(LOCTEXT("ScaleLabel", "Scale")));
}

void FMatrixStructCustomization::OnCopy(FTransformField::Type Type, TWeakPtr<IPropertyHandle> PropertyHandlePtr)
{
	auto PropertyHandle = PropertyHandlePtr.Pin();

	if (!PropertyHandle.IsValid())
	{
		return;
	}

	FString CopyStr;
	CacheValues(PropertyHandle);

	switch (Type)
	{
		case FTransformField::Location:
		{
			FVector Location = CachedTranslation->Get();
			CopyStr = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), Location.X, Location.Y, Location.Z);
			break;
		}

		case FTransformField::Rotation:
		{
			FRotator Rotation = CachedRotation->Get();
			CopyStr = FString::Printf(TEXT("(Pitch=%f,Yaw=%f,Roll=%f)"), Rotation.Pitch, Rotation.Yaw, Rotation.Roll);
			break;
		}

		case FTransformField::Scale:
		{
			FVector Scale = CachedScale->Get();
			CopyStr = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), Scale.X, Scale.Y, Scale.Z);
			break;
		}
	}

	if (!CopyStr.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*CopyStr);
	}
}

void FMatrixStructCustomization::OnPaste(FTransformField::Type Type, TWeakPtr<IPropertyHandle> PropertyHandlePtr)
{
	auto PropertyHandle = PropertyHandlePtr.Pin();

	if (!PropertyHandle.IsValid())
	{
		return;
	}

	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);

	switch (Type)
	{
		case FTransformField::Location:
		{
			FVector Location;
			if (Location.InitFromString(PastedText))
			{
				FScopedTransaction Transaction(LOCTEXT("PasteLocation", "Paste Location"));
				CachedTranslationX->Set(Location.X);
				CachedTranslationY->Set(Location.Y);
				CachedTranslationZ->Set(Location.Z);
				FlushValues(PropertyHandle);
			}
			break;
		}

		case FTransformField::Rotation:
		{
			FRotator Rotation;
			PastedText.ReplaceInline(TEXT("Pitch="), TEXT("P="));
			PastedText.ReplaceInline(TEXT("Yaw="), TEXT("Y="));
			PastedText.ReplaceInline(TEXT("Roll="), TEXT("R="));
			if (Rotation.InitFromString(PastedText))
			{
				FScopedTransaction Transaction(LOCTEXT("PasteRotation", "Paste Rotation"));
				CachedRotationPitch->Set(Rotation.Pitch);
				CachedRotationYaw->Set(Rotation.Yaw);
				CachedRotationRoll->Set(Rotation.Roll);
				FlushValues(PropertyHandle);
			}
			break;
		}

		case FTransformField::Scale:
		{
			FVector Scale;
			if (Scale.InitFromString(PastedText))
			{
				FScopedTransaction Transaction(LOCTEXT("PasteScale", "Paste Scale"));
				CachedScaleX->Set(Scale.X);
				CachedScaleY->Set(Scale.Y);
				CachedScaleZ->Set(Scale.Z);
				FlushValues(PropertyHandle);
			}
			break;
		}
	}
}

bool FMatrixStructCustomization::CacheValues( TWeakPtr<IPropertyHandle> PropertyHandlePtr ) const
{
	auto PropertyHandle = PropertyHandlePtr.Pin();

	if (!PropertyHandle.IsValid())
	{
		return false;
	}

	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	if (RawData.Num() == 1)
	{
		FMatrix* MatrixValue = reinterpret_cast<FMatrix*>(RawData[0]);
		if (MatrixValue != NULL)
		{
			CachedTranslation->Set(MatrixValue->GetOrigin());
			CachedRotation->Set(MatrixValue->Rotator());
			CachedScale->Set(MatrixValue->GetScaleVector());
			return true;
		}
	}

	return false;
}

bool FMatrixStructCustomization::FlushValues( TWeakPtr<IPropertyHandle> PropertyHandlePtr ) const
{
	auto PropertyHandle = PropertyHandlePtr.Pin();
	if (!PropertyHandle.IsValid())
	{
		return false;
	}

	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	// The object array should either be empty or the same size as the raw data array.
	check(!OuterObjects.Num() || OuterObjects.Num() == RawData.Num());

	// Persistent flag that's set when we're in the middle of an interactive change (note: assumes multiple interactive changes do not occur in parallel).
	static bool bIsInteractiveChangeInProgress = false;

	bool bNotifiedPreChange = false;
	for (int32 ValueIndex = 0; ValueIndex < RawData.Num(); ValueIndex++)
	{
		FMatrix* MatrixValue = reinterpret_cast<FMatrix*>(RawData[ValueIndex]);
		if (MatrixValue != NULL)
		{
			const FMatrix PreviousValue = *MatrixValue;
			const FRotator CurrentRotation = MatrixValue->Rotator();
			const FVector CurrentTranslation = MatrixValue->GetOrigin();
			const FVector CurrentScale = MatrixValue->GetScaleVector();

			FRotator Rotation(
				CachedRotationPitch->IsSet() ? CachedRotationPitch->Get() : CurrentRotation.Pitch,
				CachedRotationYaw->IsSet() ? CachedRotationYaw->Get() : CurrentRotation.Yaw,
				CachedRotationRoll->IsSet() ? CachedRotationRoll->Get() : CurrentRotation.Roll
				);
			FVector Translation(
				CachedTranslationX->IsSet() ? CachedTranslationX->Get() : CurrentTranslation.X,
				CachedTranslationY->IsSet() ? CachedTranslationY->Get() : CurrentTranslation.Y,
				CachedTranslationZ->IsSet() ? CachedTranslationZ->Get() : CurrentTranslation.Z
				);
			FVector Scale(
				CachedScaleX->IsSet() ? CachedScaleX->Get() : CurrentScale.X,
				CachedScaleY->IsSet() ? CachedScaleY->Get() : CurrentScale.Y,
				CachedScaleZ->IsSet() ? CachedScaleZ->Get() : CurrentScale.Z
				);

			const FMatrix NewValue = FScaleRotationTranslationMatrix(Scale, Rotation, Translation);

			if (!bNotifiedPreChange && (!MatrixValue->Equals(NewValue, 0.0f) || (!bIsUsingSlider && bIsInteractiveChangeInProgress)))
			{
				if (!bIsInteractiveChangeInProgress)
				{
					GEditor->BeginTransaction(FText::Format(LOCTEXT("SetPropertyValue", "Set {0}"), PropertyHandle->GetPropertyDisplayName()));
				}

				PropertyHandle->NotifyPreChange();
				bNotifiedPreChange = true;

				bIsInteractiveChangeInProgress = bIsUsingSlider;
			}

			// Set the new value.
			*MatrixValue = NewValue;

			// Propagate default value changes after updating, for archetypes. As per usual, we only propagate the change if the instance matches the archetype's value.
			// Note: We cannot use the "normal" PropertyNode propagation logic here, because that is string-based and the decision to propagate relies on an exact value match.
			// Here, we're dealing with conversions between FMatrix and FVector/FRotator values, so there is some precision loss that requires a tolerance when comparing values.
			if (ValueIndex < OuterObjects.Num() && OuterObjects[ValueIndex]->IsTemplate())
			{
				TArray<UObject*> ArchetypeInstances;
				OuterObjects[ValueIndex]->GetArchetypeInstances(ArchetypeInstances);
				for (UObject* ArchetypeInstance : ArchetypeInstances)
				{
					FMatrix* CurrentValue = reinterpret_cast<FMatrix*>(PropertyHandle->GetValueBaseAddress(reinterpret_cast<uint8*>(ArchetypeInstance)));
					if (CurrentValue && CurrentValue->Equals(PreviousValue))
					{
						*CurrentValue = NewValue;
					}
				}
			}
		}
	}

	if (bNotifiedPreChange)
	{
		PropertyHandle->NotifyPostChange(bIsUsingSlider ? EPropertyChangeType::Interactive : EPropertyChangeType::ValueSet);

		if (!bIsUsingSlider)
		{
			GEditor->EndTransaction();
			bIsInteractiveChangeInProgress = false;
		}
	}

	if (PropertyUtilities.IsValid() && !bIsInteractiveChangeInProgress)
	{
		FPropertyChangedEvent ChangeEvent(PropertyHandle->GetProperty(), EPropertyChangeType::ValueSet);
		PropertyUtilities->NotifyFinishedChangingProperties(ChangeEvent);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

TSharedRef<IPropertyTypeCustomization> FTransformStructCustomization::MakeInstance() 
{
	return MakeShareable( new FTransformStructCustomization );
}

bool FTransformStructCustomization::CacheValues( TWeakPtr<IPropertyHandle> PropertyHandlePtr ) const
{
	auto PropertyHandle = PropertyHandlePtr.Pin();

	if (!PropertyHandle.IsValid())
	{
		return false;
	}

	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	if (RawData.Num() == 1)
	{
		FTransform* TransformValue = reinterpret_cast<FTransform*>(RawData[0]);
		if (TransformValue != NULL)
		{
			CachedTranslation->Set(TransformValue->GetTranslation());
			CachedRotation->Set(TransformValue->GetRotation().Rotator());
			CachedScale->Set(TransformValue->GetScale3D());
			return true;
		}
	}

	return false;
}

bool FTransformStructCustomization::FlushValues( TWeakPtr<IPropertyHandle> PropertyHandlePtr ) const
{
	auto PropertyHandle = PropertyHandlePtr.Pin();

	if (!PropertyHandle.IsValid())
	{
		return false;
	}

	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	// The object array should either be empty or the same size as the raw data array.
	check(!OuterObjects.Num() || OuterObjects.Num() == RawData.Num());

	// Persistent flag that's set when we're in the middle of an interactive change (note: assumes multiple interactive changes do not occur in parallel).
	static bool bIsInteractiveChangeInProgress = false;

	bool bNotifiedPreChange = false;
	for (int32 ValueIndex = 0; ValueIndex < RawData.Num(); ValueIndex++)
	{
		FTransform* TransformValue = reinterpret_cast<FTransform*>(RawData[0]);
		if (TransformValue != NULL)
		{
			const FTransform PreviousValue = *TransformValue;
			const FRotator CurrentRotation = TransformValue->GetRotation().Rotator();
			const FVector CurrentTranslation = TransformValue->GetTranslation();
			const FVector CurrentScale = TransformValue->GetScale3D();

			FRotator Rotation(
				CachedRotationPitch->IsSet() ? CachedRotationPitch->Get() : CurrentRotation.Pitch,
				CachedRotationYaw->IsSet() ? CachedRotationYaw->Get() : CurrentRotation.Yaw,
				CachedRotationRoll->IsSet() ? CachedRotationRoll->Get() : CurrentRotation.Roll
				);
			FVector Translation(
				CachedTranslationX->IsSet() ? CachedTranslationX->Get() : CurrentTranslation.X,
				CachedTranslationY->IsSet() ? CachedTranslationY->Get() : CurrentTranslation.Y,
				CachedTranslationZ->IsSet() ? CachedTranslationZ->Get() : CurrentTranslation.Z
				);
			FVector Scale(
				CachedScaleX->IsSet() ? CachedScaleX->Get() : CurrentScale.X,
				CachedScaleY->IsSet() ? CachedScaleY->Get() : CurrentScale.Y,
				CachedScaleZ->IsSet() ? CachedScaleZ->Get() : CurrentScale.Z
				);

			const FTransform NewValue = FTransform(Rotation, Translation, Scale);

			if (!bNotifiedPreChange && (!TransformValue->Equals(NewValue, 0.0f) || (!bIsUsingSlider && bIsInteractiveChangeInProgress)))
			{
				if (!bIsInteractiveChangeInProgress)
				{
					GEditor->BeginTransaction(FText::Format(NSLOCTEXT("FTransformStructCustomization", "SetPropertyValue", "Set {0}"), PropertyHandle->GetPropertyDisplayName()));
				}

				PropertyHandle->NotifyPreChange();
				bNotifiedPreChange = true;

				bIsInteractiveChangeInProgress = bIsUsingSlider;
			}

			// Set the new value.
			*TransformValue = NewValue;

			// Propagate default value changes after updating, for archetypes. As per usual, we only propagate the change if the instance matches the archetype's value.
			// Note: We cannot use the "normal" PropertyNode propagation logic here, because that is string-based and the decision to propagate relies on an exact value match.
			// Here, we're dealing with conversions between FTransform and FVector/FRotator values, so there is some precision loss that requires a tolerance when comparing values.
			if (ValueIndex < OuterObjects.Num() && OuterObjects[ValueIndex]->IsTemplate())
			{
				TArray<UObject*> ArchetypeInstances;
				OuterObjects[ValueIndex]->GetArchetypeInstances(ArchetypeInstances);
				for (UObject* ArchetypeInstance : ArchetypeInstances)
				{
					FTransform* CurrentValue = reinterpret_cast<FTransform*>(PropertyHandle->GetValueBaseAddress(reinterpret_cast<uint8*>(ArchetypeInstance)));
					if (CurrentValue && CurrentValue->Equals(PreviousValue))
					{
						*CurrentValue = NewValue;
					}
				}
			}
		}
	}
	
	if (bNotifiedPreChange)
	{
		PropertyHandle->NotifyPostChange(bIsUsingSlider ? EPropertyChangeType::Interactive : EPropertyChangeType::ValueSet);

		if (!bIsUsingSlider)
		{
			GEditor->EndTransaction();
			bIsInteractiveChangeInProgress = false;
		}
	}

	if (PropertyUtilities.IsValid() && !bIsInteractiveChangeInProgress)
	{
		FPropertyChangedEvent ChangeEvent(PropertyHandle->GetProperty(), EPropertyChangeType::ValueSet);
		PropertyUtilities->NotifyFinishedChangingProperties(ChangeEvent);
	}

	return true;
}


TSharedRef<IPropertyTypeCustomization> FQuatStructCustomization::MakeInstance()
{
	return MakeShareable(new FQuatStructCustomization);
}


void FQuatStructCustomization::MakeHeaderRow(TSharedRef<class IPropertyHandle>& InStructPropertyHandle, FDetailWidgetRow& Row)
{
	CustomizeRotation(InStructPropertyHandle, Row);
}


void FQuatStructCustomization::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	FMathStructProxyCustomization::CustomizeChildren(StructPropertyHandle, StructBuilder, StructCustomizationUtils);
}


bool FQuatStructCustomization::CacheValues(TWeakPtr<IPropertyHandle> PropertyHandlePtr) const
{
	auto PropertyHandle = PropertyHandlePtr.Pin();

	if (!PropertyHandle.IsValid())
	{
		return false;
	}

	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	if (RawData.Num() == 1)
	{
		FQuat* QuatValue = reinterpret_cast<FQuat*>(RawData[0]);
		if (QuatValue != NULL)
		{
			CachedRotation->Set(QuatValue->Rotator());
			return true;
		}
	}

	return false;
}

bool FQuatStructCustomization::FlushValues(TWeakPtr<IPropertyHandle> PropertyHandlePtr) const
{
	auto PropertyHandle = PropertyHandlePtr.Pin();

	if (!PropertyHandle.IsValid())
	{
		return false;
	}

	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	// The object array should either be empty or the same size as the raw data array.
	check(!OuterObjects.Num() || OuterObjects.Num() == RawData.Num());

	// Persistent flag that's set when we're in the middle of an interactive change (note: assumes multiple interactive changes do not occur in parallel).
	static bool bIsInteractiveChangeInProgress = false;

	bool bNotifiedPreChange = false;
	for (int32 ValueIndex = 0; ValueIndex < RawData.Num(); ValueIndex++)
	{
		FQuat* QuatValue = reinterpret_cast<FQuat*>(RawData[0]);
		if (QuatValue != NULL)
		{
			const FQuat PreviousValue = *QuatValue;
			const FRotator CurrentRotation = QuatValue->Rotator();

			FRotator Rotation(
				CachedRotationPitch->IsSet() ? CachedRotationPitch->Get() : CurrentRotation.Pitch,
				CachedRotationYaw->IsSet() ? CachedRotationYaw->Get() : CurrentRotation.Yaw,
				CachedRotationRoll->IsSet() ? CachedRotationRoll->Get() : CurrentRotation.Roll
				);
			
			const FQuat NewValue = Rotation.Quaternion();

			if (!bNotifiedPreChange && (!QuatValue->Equals(NewValue, 0.0f) || (!bIsUsingSlider && bIsInteractiveChangeInProgress)))
			{
				if (!bIsInteractiveChangeInProgress)
				{
					GEditor->BeginTransaction(FText::Format(NSLOCTEXT("FQuatStructCustomization", "SetPropertyValue", "Set {0}"), PropertyHandle->GetPropertyDisplayName()));
				}

				PropertyHandle->NotifyPreChange();
				bNotifiedPreChange = true;

				bIsInteractiveChangeInProgress = bIsUsingSlider;
			}

			// Set the new value.
			*QuatValue = NewValue;

			// Propagate default value changes after updating, for archetypes. As per usual, we only propagate the change if the instance matches the archetype's value.
			// Note: We cannot use the "normal" PropertyNode propagation logic here, because that is string-based and the decision to propagate relies on an exact value match.
			// Here, we're dealing with conversions between FQuat and FRotator values, so there is some precision loss that requires a tolerance when comparing values.
			if (ValueIndex < OuterObjects.Num() && OuterObjects[ValueIndex]->IsTemplate())
			{
				TArray<UObject*> ArchetypeInstances;
				OuterObjects[ValueIndex]->GetArchetypeInstances(ArchetypeInstances);
				for (UObject* ArchetypeInstance : ArchetypeInstances)
				{
					FQuat* CurrentValue = reinterpret_cast<FQuat*>(PropertyHandle->GetValueBaseAddress(reinterpret_cast<uint8*>(ArchetypeInstance)));
					if (CurrentValue && CurrentValue->Equals(PreviousValue))
					{
						*CurrentValue = NewValue;
					}
				}
			}
		}
	}

	if (bNotifiedPreChange)
	{
		PropertyHandle->NotifyPostChange(bIsUsingSlider ? EPropertyChangeType::Interactive : EPropertyChangeType::ValueSet);

		if (!bIsUsingSlider)
		{
			GEditor->EndTransaction();
			bIsInteractiveChangeInProgress = false;
		}
	}

	if (PropertyUtilities.IsValid() && !bIsInteractiveChangeInProgress)
	{
		FPropertyChangedEvent ChangeEvent(PropertyHandle->GetProperty(), EPropertyChangeType::ValueSet);
		PropertyUtilities->NotifyFinishedChangingProperties(ChangeEvent);
	}

	return true;
}


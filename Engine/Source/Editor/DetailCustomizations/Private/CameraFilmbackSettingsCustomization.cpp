// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CameraFilmbackSettingsCustomization.h"
#include "PropertyHandle.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "ScopedTransaction.h"
#include "CineCameraComponent.h"

#define LOCTEXT_NAMESPACE "CameraFilmbackSettingsCustomization"

FCameraFilmbackSettingsCustomization::FCameraFilmbackSettingsCustomization()
{
	TArray<FNamedFilmbackPreset> const& Presets = UCineCameraComponent::GetFilmbackPresets();

	int32 const NumPresets = Presets.Num();
	// first create preset combo list
	PresetComboList.Empty(NumPresets + 1);

	// first one is default one
	PresetComboList.Add(MakeShareable(new FString(TEXT("Custom..."))));

	// put all presets in the list
	for (FNamedFilmbackPreset const& P : Presets)
	{
		PresetComboList.Add(MakeShareable(new FString(P.Name)));
	}
}

TSharedRef<IPropertyTypeCustomization> FCameraFilmbackSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FCameraFilmbackSettingsCustomization);
}

void FCameraFilmbackSettingsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow.
		NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(0.f)
		[
			SAssignNew(PresetComboBox, SComboBox< TSharedPtr<FString> >)
			.OptionsSource(&PresetComboList)
			.OnGenerateWidget(this, &FCameraFilmbackSettingsCustomization::MakePresetComboWidget)
			.OnSelectionChanged(this, &FCameraFilmbackSettingsCustomization::OnPresetChanged)
			.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
			.ContentPadding(2)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &FCameraFilmbackSettingsCustomization::GetPresetComboBoxContent)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ToolTipText(this, &FCameraFilmbackSettingsCustomization::GetPresetComboBoxContent)
			]
		];
}

void FCameraFilmbackSettingsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// Retrieve structure's child properties
	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren( NumChildren );	
	TMap<FName, TSharedPtr< IPropertyHandle > > PropertyHandles;	
	for( uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle( ChildIndex ).ToSharedRef();
		const FName PropertyName = ChildHandle->GetProperty()->GetFName();

		PropertyHandles.Add(PropertyName, ChildHandle);
	}
	
	// Retrieve special case properties
	SensorWidthHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FCameraFilmbackSettings, SensorWidth));
	SensorHeightHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FCameraFilmbackSettings, SensorHeight));

	for( auto Iter(PropertyHandles.CreateConstIterator()); Iter; ++Iter  )
	{
		IDetailPropertyRow& SettingsRow = ChildBuilder.AddProperty(Iter.Value().ToSharedRef());
	}	
}

TSharedRef<SWidget> FCameraFilmbackSettingsCustomization::MakePresetComboWidget(TSharedPtr<FString> InItem)
{
	return
		SNew(STextBlock)
		.Text(FText::FromString(*InItem))
		.Font(IDetailLayoutBuilder::GetDetailFont());
}

void FCameraFilmbackSettingsCustomization::OnPresetChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	// if it's set from code, we did that on purpose
	if (SelectInfo != ESelectInfo::Direct)
	{
		FString const NewPresetName = *NewSelection.Get();

		// search presets for one that matches
		TArray<FNamedFilmbackPreset> const& Presets = UCineCameraComponent::GetFilmbackPresets();
		int32 const NumPresets = Presets.Num();
		for (int32 PresetIdx = 0; PresetIdx < NumPresets; ++PresetIdx)
		{
			FNamedFilmbackPreset const& P = Presets[PresetIdx];
			if (P.Name == NewPresetName)
			{
				const FScopedTransaction Transaction(LOCTEXT("ChangeFilmbackPreset", "Change Filmback Preset"));
				
				// copy data from preset into properties
				// all SetValues except the last set to Interactive so we don't rerun construction scripts and invalidate subsequent property handles
				ensure(SensorHeightHandle->SetValue(P.FilmbackSettings.SensorHeight, EPropertyValueSetFlags::InteractiveChange | EPropertyValueSetFlags::NotTransactable) == FPropertyAccess::Result::Success);
				ensure(SensorWidthHandle->SetValue(P.FilmbackSettings.SensorWidth, EPropertyValueSetFlags::NotTransactable) == FPropertyAccess::Result::Success);
				
				break;
			}
		}

		// if none of them found, do nothing
	}
}


FText FCameraFilmbackSettingsCustomization::GetPresetComboBoxContent() const
{
	// just test one variable for multiple selection
	float CurSensorWidth;
	if (SensorWidthHandle->GetValue(CurSensorWidth) == FPropertyAccess::Result::MultipleValues)
	{
		// multiple selection
		return LOCTEXT("MultipleValues", "Multiple Values");
	}

	return FText::FromString(*GetPresetString().Get());
}


TSharedPtr<FString> FCameraFilmbackSettingsCustomization::GetPresetString() const
{
	float CurSensorWidth;
	SensorWidthHandle->GetValue(CurSensorWidth);

	float CurSensorHeight;
	SensorHeightHandle->GetValue(CurSensorHeight);

	// search presets for one that matches
	TArray<FNamedFilmbackPreset> const& Presets = UCineCameraComponent::GetFilmbackPresets();
	int32 const NumPresets = Presets.Num();
	for (int32 PresetIdx = 0; PresetIdx < NumPresets; ++PresetIdx)
	{
		FNamedFilmbackPreset const& P = Presets[PresetIdx];
		if ((P.FilmbackSettings.SensorWidth == CurSensorWidth) && (P.FilmbackSettings.SensorHeight == CurSensorHeight))
		{
			// this is the one
			if (PresetComboList.IsValidIndex(PresetIdx + 1))
			{
				return PresetComboList[PresetIdx + 1];
			}
		}
	}

	return PresetComboList[0];
}


#undef LOCTEXT_NAMESPACE // CameraFilmbackSettingsCustomization

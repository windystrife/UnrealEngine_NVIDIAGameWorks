// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AbcImportSettingsCustomization.h"

#include "AbcImportSettings.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "PropertyRestriction.h"

void FAbcImportSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& LayoutBuilder)
{
	TSharedRef<IPropertyHandle> ImportType = LayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAbcImportSettings, ImportType));
	
	uint8 EnumValue;
	ImportType->GetValue(EnumValue);
	IDetailCategoryBuilder& CompressionBuilder = LayoutBuilder.EditCategory("Compression");
	CompressionBuilder.SetCategoryVisibility(EnumValue == (uint8)EAlembicImportType::Skeletal);

	IDetailCategoryBuilder& StaticMeshBuilder = LayoutBuilder.EditCategory("StaticMesh");
	StaticMeshBuilder.SetCategoryVisibility(EnumValue == (uint8)EAlembicImportType::StaticMesh);

	FSimpleDelegate OnImportTypeChangedDelegate = FSimpleDelegate::CreateSP(this, &FAbcImportSettingsCustomization::OnImportTypeChanged, &LayoutBuilder);
	ImportType->SetOnPropertyValueChanged(OnImportTypeChangedDelegate);

	if (UAbcImportSettings::Get()->bReimport)
	{
		UEnum* ImportTypeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EAlembicImportType"));		
		static FText RestrictReason = NSLOCTEXT("AlembicImportFactory", "ReimportRestriction", "Unable to change type while reimporting");
		TSharedPtr<FPropertyRestriction> EnumRestriction = MakeShareable(new FPropertyRestriction(RestrictReason));

		for (uint8 EnumIndex = 0; EnumIndex < (ImportTypeEnum->GetMaxEnumValue() + 1); ++EnumIndex)
		{
			if (EnumValue != EnumIndex)
			{
				EnumRestriction->AddDisabledValue(ImportTypeEnum->GetNameByValue(EnumIndex).ToString());
			}
		}		
		ImportType->AddRestriction(EnumRestriction.ToSharedRef());
	}	
}

TSharedRef<IDetailCustomization> FAbcImportSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FAbcImportSettingsCustomization());
}

void FAbcImportSettingsCustomization::OnImportTypeChanged(IDetailLayoutBuilder* LayoutBuilder)
{
	LayoutBuilder->ForceRefreshDetails();
}

TSharedRef<IPropertyTypeCustomization> FAbcSamplingSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FAbcSamplingSettingsCustomization);
}

void FAbcSamplingSettingsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	Settings = UAbcImportSettings::Get();

	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);
	
	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
		IDetailPropertyRow& Property = StructBuilder.AddProperty(ChildHandle);
		static const FName EditConditionName = "EnumCondition";
		int32 EnumCondition = ChildHandle->GetINTMetaData(EditConditionName);
		Property.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FAbcSamplingSettingsCustomization::ArePropertiesVisible, EnumCondition)));
	}
}

EVisibility FAbcSamplingSettingsCustomization::ArePropertiesVisible(const int32 VisibleType) const
{
	return (Settings->SamplingSettings.SamplingType == (EAlembicSamplingType)VisibleType || VisibleType == 0) ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<IPropertyTypeCustomization> FAbcCompressionSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FAbcCompressionSettingsCustomization);
}

void FAbcCompressionSettingsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	Settings = UAbcImportSettings::Get();

	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
		IDetailPropertyRow& Property = StructBuilder.AddProperty(ChildHandle);
		static const FName EditConditionName = "EnumCondition";
		int32 EnumCondition = ChildHandle->GetINTMetaData(EditConditionName);
		Property.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FAbcCompressionSettingsCustomization::ArePropertiesVisible, EnumCondition)));
	}
}

EVisibility FAbcCompressionSettingsCustomization::ArePropertiesVisible(const int32 VisibleType) const
{
	return (Settings->CompressionSettings.BaseCalculationType == (EBaseCalculationType)VisibleType || VisibleType == 0) ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<IPropertyTypeCustomization> FAbcConversionSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FAbcConversionSettingsCustomization);
}

void FAbcConversionSettingsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	Settings = UAbcImportSettings::Get();
	FSimpleDelegate OnPresetChanged = FSimpleDelegate::CreateSP(this, &FAbcConversionSettingsCustomization::OnConversionPresetChanged);
	FSimpleDelegate OnValueChanged = FSimpleDelegate::CreateSP(this, &FAbcConversionSettingsCustomization::OnConversionValueChanged);

	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);
	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();

		if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FAbcConversionSettings, Preset))
		{			
			ChildHandle->SetOnPropertyValueChanged(OnPresetChanged);
		}
		else
		{
			ChildHandle->SetOnPropertyValueChanged(OnValueChanged);
			ChildHandle->SetOnChildPropertyValueChanged(OnValueChanged);			
		}

		IDetailPropertyRow& Property = StructBuilder.AddProperty(ChildHandle);
	}
}

void FAbcConversionSettingsCustomization::OnConversionPresetChanged()
{
	// Set values to specified preset
	switch (Settings->ConversionSettings.Preset)
	{
		case EAbcConversionPreset::Maya:
		{
			Settings->ConversionSettings.bFlipU = false;
			Settings->ConversionSettings.bFlipV = true;
			Settings->ConversionSettings.Scale = FVector(1.0f, -1.0f, 1.0f);
			Settings->ConversionSettings.Rotation = FVector::ZeroVector;
			break;
		}

		case EAbcConversionPreset::Max:
		{
			Settings->ConversionSettings.bFlipU = false;
			Settings->ConversionSettings.bFlipV = true;
			Settings->ConversionSettings.Scale = FVector(1.0f, -1.0f, 1.0f);
			Settings->ConversionSettings.Rotation = FVector(90.0f, 0.0f, 0);
			break;
		}
	}
}

void FAbcConversionSettingsCustomization::OnConversionValueChanged()
{
	// Set conversion preset to custom
	Settings->ConversionSettings.Preset = EAbcConversionPreset::Custom;
}

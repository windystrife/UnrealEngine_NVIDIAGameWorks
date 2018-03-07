// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshMergingSettingsCustomization.h"
#include "Engine/MeshMerging.h"
#include "Misc/Attribute.h"
#include "UObject/UnrealType.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "PropertyRestriction.h"

#define LOCTEXT_NAMESPACE "FMeshMergingSettingCustomization"

FMeshMergingSettingsObjectCustomization::~FMeshMergingSettingsObjectCustomization()
{
}

void FMeshMergingSettingsObjectCustomization::CustomizeDetails(IDetailLayoutBuilder& LayoutBuilder)
{
	TSharedRef<IPropertyHandle> SettingsHandle = LayoutBuilder.GetProperty(FName("UMeshMergingSettingsObject.Settings"));
	
	FName MeshCategory("MeshSettings");
	IDetailCategoryBuilder& MeshCategoryBuilder = LayoutBuilder.EditCategory(MeshCategory);

	TArray<TSharedRef<IPropertyHandle>> SimpleDefaultProperties;
	MeshCategoryBuilder.GetDefaultProperties(SimpleDefaultProperties, true, true);
	MeshCategoryBuilder.AddProperty(SettingsHandle);

	FName CategoryMetaData("Category");
	for (TSharedRef<IPropertyHandle> Property: SimpleDefaultProperties)
	{
		const FString& CategoryName = Property->GetMetaData(CategoryMetaData);

		IDetailCategoryBuilder& CategoryBuilder = LayoutBuilder.EditCategory(*CategoryName);
		IDetailPropertyRow& PropertyRow = CategoryBuilder.AddProperty(Property);

		if (Property->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FMeshMergingSettings, SpecificLOD))
		{
			static const FName EditConditionName = "EnumCondition";
			int32 EnumCondition = Property->GetINTMetaData(EditConditionName);
			PropertyRow.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FMeshMergingSettingsObjectCustomization::ArePropertiesVisible, EnumCondition)));
		}
		else if (Property->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FMeshMergingSettings, LODSelectionType))
		{
			EnumProperty = Property;
						
			TSharedPtr<FPropertyRestriction> EnumRestriction = MakeShareable(new FPropertyRestriction(LOCTEXT("NoSupport","Unable to support this option in Merge Actor")));
			const UEnum* const MeshLODSelectionTypeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EMeshLODSelectionType"));		
			EnumRestriction->AddDisabledValue(MeshLODSelectionTypeEnum->GetNameStringByValue((uint8)EMeshLODSelectionType::CalculateLOD));
			EnumProperty->AddRestriction(EnumRestriction.ToSharedRef());
		}
	}

	FName MaterialCategory("MaterialSettings");
	IDetailCategoryBuilder& MaterialCategoryBuilder = LayoutBuilder.EditCategory(MaterialCategory);
	SimpleDefaultProperties.Empty();
	MaterialCategoryBuilder.GetDefaultProperties(SimpleDefaultProperties, true, true);

	for (TSharedRef<IPropertyHandle> Property : SimpleDefaultProperties)
	{
		const FString& CategoryName = Property->GetMetaData(CategoryMetaData);

		IDetailCategoryBuilder& CategoryBuilder = LayoutBuilder.EditCategory(*CategoryName);
		IDetailPropertyRow& PropertyRow = CategoryBuilder.AddProperty(Property);

		// Disable material settings if we are exporting all LODs (no support for material baking in this case)
		if (CategoryName.Compare("MaterialSettings") == 0)
		{
			PropertyRow.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FMeshMergingSettingsObjectCustomization::AreMaterialPropertiesEnabled)));
		}
	}
}

TSharedRef<IDetailCustomization> FMeshMergingSettingsObjectCustomization::MakeInstance()
{
	return MakeShareable(new FMeshMergingSettingsObjectCustomization);
}

EVisibility FMeshMergingSettingsObjectCustomization::ArePropertiesVisible(const int32 VisibleType) const
{
	uint8 CurrentEnumValue = 0;
	EnumProperty->GetValue(CurrentEnumValue);
	return (CurrentEnumValue == VisibleType) ? EVisibility::Visible : EVisibility::Collapsed;
}

bool FMeshMergingSettingsObjectCustomization::AreMaterialPropertiesEnabled() const
{
	uint8 CurrentEnumValue = 0;
	EnumProperty->GetValue(CurrentEnumValue);

	return !(CurrentEnumValue == (uint8)EMeshLODSelectionType::AllLODs);
}

#undef LOCTEXT_NAMESPACE

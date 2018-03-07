// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PrimaryAssetIdCustomization.h"
#include "AssetManagerEditorModule.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Engine/AssetManager.h"
#include "PropertyHandle.h"
#include "PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "PrimaryAssetIdCustomization"

void FPrimaryAssetIdCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if (!UAssetManager::IsValid())
	{
		HeaderRow
		.NameContent()
		[
			InStructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.0f)
		.MaxDesiredWidth(0.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoAssetManager", "Enable Asset Manager to edit Primary Asset Ids"))
		];

		return;
	}

	StructPropertyHandle = InStructPropertyHandle;

	const FString& TypeFilterString = StructPropertyHandle->GetMetaData("AllowedTypes");
	if( !TypeFilterString.IsEmpty() )
	{
		TArray<FString> CustomTypeFilterNames;
		TypeFilterString.ParseIntoArray(CustomTypeFilterNames, TEXT(","), true);

		for(auto It = CustomTypeFilterNames.CreateConstIterator(); It; ++It)
		{
			const FString& TypeName = *It;

			AllowedTypes.Add(*TypeName);
		}
	}

	FOnShouldFilterAsset AssetFilter = FOnShouldFilterAsset::CreateStatic(&IAssetManagerEditorModule::OnShouldFilterPrimaryAsset, AllowedTypes);

	// Can the field be cleared
	const bool bAllowClear = !(StructPropertyHandle->GetMetaDataProperty()->PropertyFlags & CPF_NoClear);

	HeaderRow
	.NameContent()
	[
		InStructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(250.0f)
	.MaxDesiredWidth(0.0f)
	[
		// Add an object entry box.  Even though this isn't an object entry, we will simulate one
		SNew( SObjectPropertyEntryBox )
		.ObjectPath(this, &FPrimaryAssetIdCustomization::OnGetObjectPath)
		.PropertyHandle(InStructPropertyHandle)
		.ThumbnailPool(StructCustomizationUtils.GetThumbnailPool())
		.OnShouldFilterAsset(AssetFilter)
		.OnObjectChanged(this, &FPrimaryAssetIdCustomization::OnSetObject)
		.AllowClear(bAllowClear)
	];
}

FString FPrimaryAssetIdCustomization::OnGetObjectPath() const
{
	FString StringReference;
	if (StructPropertyHandle.IsValid())
	{
		StructPropertyHandle->GetValueAsFormattedString(StringReference);
	}
	else
	{
		StringReference = FPrimaryAssetId().ToString();
	}

	UAssetManager& Manager = UAssetManager::Get();

	FSoftObjectPath FoundPath = Manager.GetPrimaryAssetPath(FPrimaryAssetId(StringReference));

	return FoundPath.ToString();
}

void FPrimaryAssetIdCustomization::OnSetObject(const FAssetData& AssetData)
{
	UAssetManager& Manager = UAssetManager::Get();

	if (StructPropertyHandle.IsValid() && StructPropertyHandle->IsValidHandle())
	{
		FPrimaryAssetId AssetId;
		if (AssetData.IsValid())
		{
			AssetId = Manager.GetPrimaryAssetIdForData(AssetData);
			ensure(AssetId.IsValid());
		}

		StructPropertyHandle->SetValueFromFormattedString(AssetId.ToString());
	}
}

void SPrimaryAssetIdGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SPrimaryAssetIdGraphPin::GetDefaultValueWidget()
{
	FString DefaultString = GraphPinObj->GetDefaultAsString();
	CurrentId = FPrimaryAssetId(DefaultString);

	return SNew(SVerticalBox)
		.Visibility(this, &SGraphPin::GetDefaultValueVisibility)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			IAssetManagerEditorModule::MakePrimaryAssetIdSelector(
				FOnGetPrimaryAssetDisplayText::CreateSP(this, &SPrimaryAssetIdGraphPin::GetDisplayText),
				FOnSetPrimaryAssetId::CreateSP(this, &SPrimaryAssetIdGraphPin::OnIdSelected),
				true)
		];
}

void SPrimaryAssetIdGraphPin::OnIdSelected(FPrimaryAssetId AssetId)
{
	CurrentId = AssetId;
	GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, CurrentId.ToString());
}

FText SPrimaryAssetIdGraphPin::GetDisplayText() const
{
	return FText::AsCultureInvariant(CurrentId.ToString());
}


#undef LOCTEXT_NAMESPACE

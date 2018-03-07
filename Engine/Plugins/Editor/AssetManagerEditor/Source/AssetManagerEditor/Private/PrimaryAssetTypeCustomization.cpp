// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PrimaryAssetTypeCustomization.h"
#include "AssetManagerEditorModule.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"
#include "Engine/AssetManager.h"
#include "PropertyHandle.h"
#include "PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "PrimaryAssetTypeCustomization"

void FPrimaryAssetTypeCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
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
			.Text(LOCTEXT("NoAssetManager", "Enable Asset Manager to edit Primary Asset Types"))
		];

		return;
	}

	StructPropertyHandle = InStructPropertyHandle;
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
		PropertyCustomizationHelpers::MakePropertyComboBox(InStructPropertyHandle, FOnGetPropertyComboBoxStrings::CreateStatic(&IAssetManagerEditorModule::GeneratePrimaryAssetTypeComboBoxStrings, bAllowClear))
	];
}

void SPrimaryAssetTypeGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SPrimaryAssetTypeGraphPin::GetDefaultValueWidget()
{
	FString DefaultString = GraphPinObj->GetDefaultAsString();
	CurrentType = FPrimaryAssetType(*DefaultString);

	return SNew(SVerticalBox)
		.Visibility(this, &SGraphPin::GetDefaultValueVisibility)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			IAssetManagerEditorModule::MakePrimaryAssetTypeSelector(
				FOnGetPrimaryAssetDisplayText::CreateSP(this, &SPrimaryAssetTypeGraphPin::GetDisplayText),
				FOnSetPrimaryAssetType::CreateSP(this, &SPrimaryAssetTypeGraphPin::OnTypeSelected),
				true)
		];
}

void SPrimaryAssetTypeGraphPin::OnTypeSelected(FPrimaryAssetType AssetType)
{
	CurrentType = AssetType;
	GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, CurrentType.ToString());
}

FText SPrimaryAssetTypeGraphPin::GetDisplayText() const
{
	return FText::AsCultureInvariant(CurrentType.ToString());
}

#undef LOCTEXT_NAMESPACE

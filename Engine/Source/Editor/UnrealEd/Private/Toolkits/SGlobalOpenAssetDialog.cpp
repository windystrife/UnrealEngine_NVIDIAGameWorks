// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Toolkits/SGlobalOpenAssetDialog.h"
#include "AssetData.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"

#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Toolkits/AssetEditorManager.h"

#define LOCTEXT_NAMESPACE "SGlobalOpenAssetDialog"

//////////////////////////////////////////////////////////////////////////
// SGlobalOpenAssetDialog

void SGlobalOpenAssetDialog::Construct(const FArguments& InArgs, FVector2D InSize)
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.OnAssetDoubleClicked = FOnAssetSelected::CreateSP(this, &SGlobalOpenAssetDialog::OnAssetSelectedFromPicker);
	AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateSP(this, &SGlobalOpenAssetDialog::OnPressedEnterOnAssetsInPicker);
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	AssetPickerConfig.bAllowNullSelection = false;
	AssetPickerConfig.bShowBottomToolbar = true;
	AssetPickerConfig.bAutohideSearchBar = false;
	AssetPickerConfig.bCanShowClasses = false;
	AssetPickerConfig.SaveSettingsName = TEXT("GlobalAssetPicker");

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(InSize.X)
		.HeightOverride(InSize.Y)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
			]
		]
	];
}


FReply SGlobalOpenAssetDialog::OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		FSlateApplication::Get().DismissAllMenus();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SGlobalOpenAssetDialog::OnAssetSelectedFromPicker(const FAssetData& AssetData)
{
	if (UObject* ObjectToEdit = AssetData.GetAsset())
	{
		FAssetEditorManager::Get().OpenEditorForAsset(ObjectToEdit);
	}
}

void SGlobalOpenAssetDialog::OnPressedEnterOnAssetsInPicker(const TArray<FAssetData>& SelectedAssets)
{
	for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
	{
		if (UObject* ObjectToEdit = AssetIt->GetAsset())
		{
			FAssetEditorManager::Get().OpenEditorForAsset(ObjectToEdit);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE

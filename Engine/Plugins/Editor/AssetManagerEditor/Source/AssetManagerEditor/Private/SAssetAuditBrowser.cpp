// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SAssetAuditBrowser.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Toolkits/AssetEditorManager.h"
#include "EditorStyleSet.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SViewport.h"
#include "FileHelpers.h"
#include "ARFilter.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"

#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "AssetRegistryModule.h"
#include "Toolkits/GlobalEditorCommonCommands.h"
#include "FrontendFilterBase.h"
#include "Slate/SceneViewport.h"
#include "ObjectEditorUtils.h"
#include "Engine/AssetManager.h"
#include "Widgets/Input/SComboBox.h"
#include "ReferenceViewer.h"
#include "SlateApplication.h"

#define LOCTEXT_NAMESPACE "AssetManagementBrowser"

const FString SAssetAuditBrowser::SettingsIniSection = TEXT("AssetManagementBrowser");
const int32 SAssetAuditBrowser::MaxAssetsHistory = 10;

SAssetAuditBrowser::~SAssetAuditBrowser()
{
}

TSharedPtr<SWidget> SAssetAuditBrowser::OnGetAssetContextMenu(const TArray<FAssetData>& SelectedAssets)
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/ true, Commands);

	MenuBuilder.BeginSection("AssetOptions", LOCTEXT("OptionsHeading", "Options") );
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("EditAsset", "Edit..."),
			LOCTEXT("EditAssetTooltip", "Opens the selected asset(s) for edit."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.Edit"),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAssetAuditBrowser::EditSelectedAssets, SelectedAssets),
				FCanExecuteAction::CreateSP(this, &SAssetAuditBrowser::IsAnythingSelected)
			)
		);

		MenuBuilder.AddMenuEntry(FGlobalEditorCommonCommands::Get().FindInContentBrowser);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("Load", "Load..."),
			LOCTEXT("LoadTooltip", "Loads selected assets into memory."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.OpenLevel"),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAssetAuditBrowser::LoadSelectedAssets, SelectedAssets),
				FCanExecuteAction::CreateSP(this, &SAssetAuditBrowser::IsAnythingSelected)
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SaveSelectedAssets", "Save..."),
			LOCTEXT("SaveSelectedAssets_ToolTip", "Save the selected assets."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Save"),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAssetAuditBrowser::SaveSelectedAssets, SelectedAssets),
				FCanExecuteAction::CreateSP(this, &SAssetAuditBrowser::CanSaveSelectedAssets, SelectedAssets)
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ReferenceViewer", "Find References..."),
			LOCTEXT("ReferenceViewerTooltip", "Shows a graph of references for this asset."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAssetAuditBrowser::FindReferencesForSelectedAssets, SelectedAssets),
				FCanExecuteAction::CreateSP(this, &SAssetAuditBrowser::IsAnythingSelected)
			)
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SAssetAuditBrowser::FindInContentBrowser() const
{
	TArray<FAssetData> CurrentSelection = GetCurrentSelectionDelegate.Execute();
	if (CurrentSelection.Num() > 0)
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets(CurrentSelection);
	}
}

bool SAssetAuditBrowser::IsAnythingSelected() const
{
	TArray<FAssetData> CurrentSelection = GetCurrentSelectionDelegate.Execute();
	return CurrentSelection.Num() > 0;
}

void SAssetAuditBrowser::GetSelectedPackages(const TArray<FAssetData>& Assets, TArray<UPackage*>& OutPackages) const
{
	for (int32 AssetIdx = 0; AssetIdx < Assets.Num(); ++AssetIdx)
	{
		UPackage* Package = FindPackage(NULL, *Assets[AssetIdx].PackageName.ToString());

		if ( Package )
		{
			OutPackages.Add(Package);
		}
	}
}

void SAssetAuditBrowser::EditSelectedAssets(TArray<FAssetData> Assets) const
{
	TArray<FName> AssetNames;

	for (FAssetData& AssetData : Assets)
	{
		AssetNames.Add(AssetData.ObjectPath);
	}

	FAssetEditorManager::Get().OpenEditorsForAssets(AssetNames);
}

void SAssetAuditBrowser::OnRequestOpenAsset(const FAssetData& AssetData) const
{
	EditSelectedAssets(TArray<FAssetData>{AssetData});
}

void SAssetAuditBrowser::SaveSelectedAssets(TArray<FAssetData> Assets) const
{
	TArray<UPackage*> PackagesToSave;
	GetSelectedPackages(Assets, PackagesToSave);

	const bool bCheckDirty = false;
	const bool bPromptToSave = false;
	const FEditorFileUtils::EPromptReturnCode Return = FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, bCheckDirty, bPromptToSave);
}

bool SAssetAuditBrowser::CanSaveSelectedAssets(TArray<FAssetData> Assets) const
{
	TArray<UPackage*> Packages;
	GetSelectedPackages(Assets, Packages);
	// Don't offer save option if none of the packages are loaded
	return Packages.Num() > 0;
}

void SAssetAuditBrowser::FindReferencesForSelectedAssets(TArray<FAssetData> Assets) const
{
	TArray<FName> PackageNames;
	for (FAssetData& AssetData : Assets)
	{
		PackageNames.Add(AssetData.PackageName);
	}

	if (PackageNames.Num() > 0)
	{
		IReferenceViewerModule::Get().InvokeReferenceViewerTab(PackageNames);
	}
}

void SAssetAuditBrowser::LoadSelectedAssets(TArray<FAssetData> Assets) const
{
	for (FAssetData& AssetData : Assets)
	{
		AssetData.GetAsset();
	}
}

bool SAssetAuditBrowser::CanShowColumnForAssetRegistryTag(FName AssetType, FName TagName) const
{
	return !AssetRegistryTagsToIgnore.Contains(TagName);
}

FString SAssetAuditBrowser::GetValueForCustomColumn(FAssetData& AssetData, FName ColumnName) const
{
	return EditorModule->GetValueForCustomColumn(AssetData, ColumnName, CurrentTargetPlatform, CurrentPlatformState);
}

void SAssetAuditBrowser::Construct(const FArguments& InArgs)
{
	if (!UAssetManager::IsValid())
	{
		return;
	}

	IAssetManagerEditorModule& ManagerEditorModule = IAssetManagerEditorModule::Get();
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistry = &AssetRegistryModule.Get();
	AssetManager = &UAssetManager::Get();
	EditorModule = &IAssetManagerEditorModule::Get();

	AssetManager->UpdateManagementDatabase();

	PlatformComboList.Add(MakeShared<FString>(TEXT("Editor")));
	PlatformList.Add(nullptr);

	TArray<ITargetPlatform*> ValidPlatforms;
	ManagerEditorModule.GetAvailableTargetPlatforms(ValidPlatforms);

	for (ITargetPlatform* ValidPlatform : ValidPlatforms)
	{
		PlatformList.Add(ValidPlatform);
		PlatformComboList.Add(MakeShared<FString>(ValidPlatform->PlatformName()));
	}

	CurrentPlatformString = TEXT("Editor");
	CurrentTargetPlatform = nullptr;
	CurrentPlatformState = nullptr;

	Commands = MakeShareable(new FUICommandList());
	Commands->MapAction(FGlobalEditorCommonCommands::Get().FindInContentBrowser, FUIAction(
		FExecuteAction::CreateSP(this, &SAssetAuditBrowser::FindInContentBrowser),
		FCanExecuteAction::CreateSP(this, &SAssetAuditBrowser::IsAnythingSelected)
		));

	CurrentAssetHistoryIndex = 0;
	AssetHistory.Add(TSet<FName>());

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	// Configure filter for asset picker
	FAssetPickerConfig Config;
	Config.InitialAssetViewType = EAssetViewType::Column;
	Config.bAddFilterUI = true;
	Config.bShowPathInColumnView = true;
	Config.bSortByPathInColumnView = true;

	// Configure response to click and double-click
	Config.OnAssetDoubleClicked = FOnAssetDoubleClicked::CreateSP(this, &SAssetAuditBrowser::OnRequestOpenAsset);
	Config.OnGetAssetContextMenu = FOnGetAssetContextMenu::CreateSP(this, &SAssetAuditBrowser::OnGetAssetContextMenu);
	Config.OnAssetTagWantsToBeDisplayed = FOnShouldDisplayAssetTag::CreateSP(this, &SAssetAuditBrowser::CanShowColumnForAssetRegistryTag);
	Config.SyncToAssetsDelegates.Add(&SyncToAssetsDelegate);
	Config.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SAssetAuditBrowser::HandleFilterAsset);
	Config.GetCurrentSelectionDelegates.Add(&GetCurrentSelectionDelegate);
	Config.SetFilterDelegates.Add(&SetFilterDelegate);
	Config.bFocusSearchBoxWhenOpened = false;
	Config.bPreloadAssetsForContextMenu = false;

	Config.SaveSettingsName = SettingsIniSection;
	
	// Hide path and type by default
	Config.HiddenColumnNames.Add(TEXT("Class"));
	Config.HiddenColumnNames.Add(TEXT("Path"));

	// Add custom columns
	Config.CustomColumns.Emplace(FPrimaryAssetId::PrimaryAssetTypeTag, LOCTEXT("AssetType", "Primary Type"), LOCTEXT("AssetTypeTooltip", "Primary Asset Type of this asset, if set"), UObject::FAssetRegistryTag::TT_Alphabetical, FOnGetCustomAssetColumnData::CreateSP(this, &SAssetAuditBrowser::GetValueForCustomColumn));
	Config.CustomColumns.Emplace(FPrimaryAssetId::PrimaryAssetNameTag, LOCTEXT("AssetName", "Primary Name"), LOCTEXT("AssetNameTooltip", "Primary Asset Name of this asset, if set"), UObject::FAssetRegistryTag::TT_Alphabetical, FOnGetCustomAssetColumnData::CreateSP(this, &SAssetAuditBrowser::GetValueForCustomColumn));
	Config.CustomColumns.Emplace(IAssetManagerEditorModule::ManagedResourceSizeName, LOCTEXT("ManagedResourceSize", "Memory Kb"), LOCTEXT("ManagedResourceSizeTooltip", "Memory used by both this asset and any other assets it manages, in kilobytes"), UObject::FAssetRegistryTag::TT_Numerical, FOnGetCustomAssetColumnData::CreateSP(this, &SAssetAuditBrowser::GetValueForCustomColumn));
	Config.CustomColumns.Emplace(IAssetManagerEditorModule::ResourceSizeName, LOCTEXT("ResourceSize", "Exclusive Memory Kb"), LOCTEXT("ResourceSizeTooltip", "Memory used exclusively by this asset, in kilobytes"), UObject::FAssetRegistryTag::TT_Numerical, FOnGetCustomAssetColumnData::CreateSP(this, &SAssetAuditBrowser::GetValueForCustomColumn));
	Config.CustomColumns.Emplace(IAssetManagerEditorModule::ManagedDiskSizeName, LOCTEXT("ManagedDiskSize", "Disk Kb"), LOCTEXT("ManagedDiskSizeTooltip", "Total disk space used by both this and all managed assets, in kilobytes"), UObject::FAssetRegistryTag::TT_Numerical, FOnGetCustomAssetColumnData::CreateSP(this, &SAssetAuditBrowser::GetValueForCustomColumn));
	Config.CustomColumns.Emplace(IAssetManagerEditorModule::DiskSizeName, LOCTEXT("DiskSize", "Exclusive Disk Kb"), LOCTEXT("DiskSizeTooltip", "Size of saved file on disk for only this asset, in kilobytes"), UObject::FAssetRegistryTag::TT_Numerical, FOnGetCustomAssetColumnData::CreateSP(this, &SAssetAuditBrowser::GetValueForCustomColumn));
	Config.CustomColumns.Emplace(IAssetManagerEditorModule::TotalUsageName, LOCTEXT("TotalUsage", "Total Usage"), LOCTEXT("TotalUsageTooltip", "Weighted count of Primary Assets that use this, higher usage means it's more likely to be in memory at runtime"), UObject::FAssetRegistryTag::TT_Numerical, FOnGetCustomAssetColumnData::CreateSP(this, &SAssetAuditBrowser::GetValueForCustomColumn));
	Config.CustomColumns.Emplace(IAssetManagerEditorModule::CookRuleName, LOCTEXT("CookRule", "Cook Rule"), LOCTEXT("CookRuleTooltip", "Rather this asset will be cooked or not"), UObject::FAssetRegistryTag::TT_Alphabetical, FOnGetCustomAssetColumnData::CreateSP(this, &SAssetAuditBrowser::GetValueForCustomColumn));
	Config.CustomColumns.Emplace(IAssetManagerEditorModule::ChunksName, LOCTEXT("Chunks", "Chunks"), LOCTEXT("ChunksTooltip", "List of chunks this will be added to when cooked"), UObject::FAssetRegistryTag::TT_Alphabetical, FOnGetCustomAssetColumnData::CreateSP(this, &SAssetAuditBrowser::GetValueForCustomColumn));
	
	// Ignore these tags as we added them as custom columns
	AssetRegistryTagsToIgnore.Add(FPrimaryAssetId::PrimaryAssetTypeTag);
	AssetRegistryTagsToIgnore.Add(FPrimaryAssetId::PrimaryAssetNameTag);

	// Ignore blueprint tags
	AssetRegistryTagsToIgnore.Add("ParentClass");
	AssetRegistryTagsToIgnore.Add("BlueprintType");
	AssetRegistryTagsToIgnore.Add("NumReplicatedProperties");
	AssetRegistryTagsToIgnore.Add("NativeParentClass");
	AssetRegistryTagsToIgnore.Add("IsDataOnly");
	AssetRegistryTagsToIgnore.Add("NativeComponents");
	AssetRegistryTagsToIgnore.Add("BlueprintComponents");

	static const FName DefaultForegroundName("DefaultForeground");

	TSharedRef< SMenuAnchor > BackMenuAnchorPtr = SNew(SMenuAnchor)
		.Placement(MenuPlacement_BelowAnchor)
		.OnGetMenuContent(this, &SAssetAuditBrowser::CreateHistoryMenu, true)
		[
			SNew(SButton)
			.OnClicked(this, &SAssetAuditBrowser::OnGoBackInHistory)
			.ForegroundColor(FEditorStyle::GetSlateColor(DefaultForegroundName))
			.ButtonStyle(FEditorStyle::Get(), "FlatButton")
			.ContentPadding(FMargin(1, 0))
			.IsEnabled(this, &SAssetAuditBrowser::CanStepBackwardInHistory)
			.ToolTipText(LOCTEXT("Backward_Tooltip", "Step backward in the asset history. Right click to see full history."))
			[
				SNew(STextBlock)
				.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
				.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.11"))
				.Text(FText::FromString(FString(TEXT("\xf060"))) /*fa-arrow-left*/)
			]
		];

	TSharedRef< SMenuAnchor > FwdMenuAnchorPtr = SNew(SMenuAnchor)
		.Placement(MenuPlacement_BelowAnchor)
		.OnGetMenuContent(this, &SAssetAuditBrowser::CreateHistoryMenu, false)
		[
			SNew(SButton)
			.OnClicked(this, &SAssetAuditBrowser::OnGoForwardInHistory)
			.ForegroundColor(FEditorStyle::GetSlateColor(DefaultForegroundName))
			.ButtonStyle(FEditorStyle::Get(), "FlatButton")
			.ContentPadding(FMargin(1, 0))
			.IsEnabled(this, &SAssetAuditBrowser::CanStepForwardInHistory)
			.ToolTipText(LOCTEXT("Forward_Tooltip", "Step forward in the asset history. Right click to see full history."))
			[
				SNew(STextBlock)
				.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
				.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.11"))
				.Text(FText::FromString(FString(TEXT("\xf061"))) /*fa-arrow-right*/)
			]
		];

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.Visibility(this, &SAssetAuditBrowser::GetHistoryVisibility)
			.Padding(FMargin(3))
			.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBorder)
						.OnMouseButtonDown(this, &SAssetAuditBrowser::OnMouseDownHistory, TWeakPtr<SMenuAnchor>(BackMenuAnchorPtr))
						.BorderImage( FEditorStyle::GetBrush("NoBorder") )
						[
							BackMenuAnchorPtr
						]
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBorder)
						.OnMouseButtonDown(this, &SAssetAuditBrowser::OnMouseDownHistory, TWeakPtr<SMenuAnchor>(FwdMenuAnchorPtr))
						.BorderImage( FEditorStyle::GetBrush("NoBorder") )
						[
							FwdMenuAnchorPtr
						]
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Text(LOCTEXT("ClearAssets", "Clear Assets"))
						.OnClicked(this, &SAssetAuditBrowser::ClearAssets)
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						IAssetManagerEditorModule::MakePrimaryAssetTypeSelector(
							FOnGetPrimaryAssetDisplayText::CreateLambda([] { return LOCTEXT("AddAssetsOfType", "Add Primary Asset Type"); }),
							FOnSetPrimaryAssetType::CreateSP(this, &SAssetAuditBrowser::AddAssetsOfType),
							false)
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SComboButton)
						.MenuContent()
						[
							CreateClassPicker()
						]
						.ButtonContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AddAssetClass", "Add Asset Class"))
						]
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						IAssetManagerEditorModule::MakePrimaryAssetIdSelector(
							FOnGetPrimaryAssetDisplayText::CreateLambda([] { return LOCTEXT("AddManagedAssets", "Add Managed Assets"); }),
							FOnSetPrimaryAssetId::CreateSP(this, &SAssetAuditBrowser::AddManagedAssets),
							false)
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Text(LOCTEXT("RefreshAssets", "Refresh"))
						.OnClicked(this, &SAssetAuditBrowser::RefreshAssets)
					]
					
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.HAlign(HAlign_Fill)
					[
						SNew(SHorizontalBox)						
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.ToolTipText(LOCTEXT("Platform_Tooltip", "Select which platform to display data for. Platforms are only available if a cooked AssetRegistry.bin is available in Saved/Cooked/Platform or Build/Platform."))
							.Text(LOCTEXT("PlatformLabel", "Selected Platform: "))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Right)
						[
							SNew(SComboBox<TSharedPtr<FString>>)							
							.OptionsSource(&PlatformComboList)
							.OnGenerateWidget(this, &SAssetAuditBrowser::GeneratePlatformComboItem)
							.OnSelectionChanged(this, &SAssetAuditBrowser::HandlePlatformComboChanged)
							[
								SNew(STextBlock)
								.Text(this, &SAssetAuditBrowser::GetPlatformComboText)
							]
						]
					]
				]
			]
		]
		+SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SBorder)
			.Padding(FMargin(3))
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				ContentBrowserModule.Get().CreateAssetPicker(Config)
			]
		]
	];

	RefreshAssetView();
}

FReply SAssetAuditBrowser::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (Commands->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SAssetAuditBrowser::AddAssetsToList(const TArray<FSoftObjectPath>& AssetsToView, bool bReplaceExisting)
{
	TArray<FName> AssetNames;

	for (const FSoftObjectPath& AssetToView : AssetsToView)
	{
		AssetNames.Add(FName(*AssetToView.GetLongPackageName()));
	}

	AddAssetsToList(AssetNames, bReplaceExisting);
}

void SAssetAuditBrowser::AddAssetsToList(const TArray<FAssetData>& AssetsToView, bool bReplaceExisting)
{
	TArray<FName> AssetNames;

	for (const FAssetData& AssetToView : AssetsToView)
	{
		AssetNames.Add(AssetToView.PackageName);
	}

	AddAssetsToList(AssetNames, bReplaceExisting);
}

void SAssetAuditBrowser::AddAssetsToList(const TArray<FName>& PackageNamesToView, bool bReplaceExisting)
{
	AssetHistory.Insert(TSet<FName>(), ++CurrentAssetHistoryIndex);
	TSet<FName>& AssetSet = AssetHistory[CurrentAssetHistoryIndex];

	if (!bReplaceExisting && CurrentAssetHistoryIndex > 0)
	{
		// Copy old set
		AssetSet.Append(AssetHistory[CurrentAssetHistoryIndex - 1]);
	}

	AssetSet.Append(PackageNamesToView);

	RefreshAssetView();
}

FReply SAssetAuditBrowser::OnMouseDownHistory( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, TWeakPtr< SMenuAnchor > InMenuAnchor )
{
	if(MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		InMenuAnchor.Pin()->SetIsOpen(true);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TSharedRef<SWidget> SAssetAuditBrowser::CreateHistoryMenu(bool bInBackHistory) const
{
	FMenuBuilder MenuBuilder(true, NULL);
	if(bInBackHistory)
	{
		int32 HistoryIdx = CurrentAssetHistoryIndex - 1;
		while( HistoryIdx >= 0 )
		{
			const FText DisplayName = FText::Format(LOCTEXT("HistoryStringFormat", "{0} assets"), FText::AsNumber(AssetHistory[HistoryIdx].Num()));

			MenuBuilder.AddMenuEntry(DisplayName, DisplayName, FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateRaw(this, &SAssetAuditBrowser::GoToHistoryIndex, HistoryIdx)
				),
				NAME_None, EUserInterfaceActionType::Button);
			--HistoryIdx;
		}
	}
	else
	{
		int32 HistoryIdx = CurrentAssetHistoryIndex + 1;
		while( HistoryIdx < AssetHistory.Num() )
		{
			const FText DisplayName = FText::Format(LOCTEXT("HistoryStringFormat", "{0} assets"), FText::AsNumber(AssetHistory[HistoryIdx].Num()));

			MenuBuilder.AddMenuEntry(DisplayName, DisplayName, FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateRaw(this, &SAssetAuditBrowser::GoToHistoryIndex, HistoryIdx)
				),
				NAME_None, EUserInterfaceActionType::Button);
			++HistoryIdx;
		}
	}

	return MenuBuilder.MakeWidget();
}

bool SAssetAuditBrowser::CanStepBackwardInHistory() const
{
	if (CurrentAssetHistoryIndex > 0)
	{
		return true;
	}
	return false;
}

bool SAssetAuditBrowser::CanStepForwardInHistory() const
{
	if (CurrentAssetHistoryIndex < AssetHistory.Num() - 1)
	{
		return true;
	}

	return false;
}

FReply SAssetAuditBrowser::OnGoForwardInHistory()
{
	GoToHistoryIndex(CurrentAssetHistoryIndex + 1);
	return FReply::Handled();
}

FReply SAssetAuditBrowser::OnGoBackInHistory()
{
	GoToHistoryIndex(CurrentAssetHistoryIndex - 1);
	return FReply::Handled();
}

void SAssetAuditBrowser::GoToHistoryIndex(int32 InHistoryIdx)
{
	if(AssetHistory.IsValidIndex(InHistoryIdx))
	{
		CurrentAssetHistoryIndex = InHistoryIdx;
		RefreshAssetView();
	}
}

void SAssetAuditBrowser::RefreshAssetView()
{
	FARFilter Filter;

	// Add manual package list
	const TSet<FName>& AssetSet = AssetHistory[CurrentAssetHistoryIndex];

	for (FName PackageName : AssetSet)
	{
		Filter.PackageNames.Add(PackageName);
	}

	if (Filter.PackageNames.Num() == 0)
	{
		// Add a bad name to force it to display nothing
		Filter.PackageNames.Add("/Temp/FakePackageNameToMakeNothingShowUp");
	}

	SetFilterDelegate.Execute(Filter);
}

FReply SAssetAuditBrowser::ClearAssets()
{
	AddAssetsToList(TArray<FName>(), true);

	return FReply::Handled();
}

FReply SAssetAuditBrowser::RefreshAssets()
{
	AssetManager->UpdateManagementDatabase(true);
	RefreshAssetView();

	return FReply::Handled();
}

void SAssetAuditBrowser::AddAssetsOfType(FPrimaryAssetType AssetType)
{
	if (AssetType.IsValid())
	{
		TArray<FSoftObjectPath> AssetArray;
		
		AssetManager->GetPrimaryAssetPathList(AssetType, AssetArray);

		AddAssetsToList(AssetArray, false);
	}
}

void SAssetAuditBrowser::AddManagedAssets(FPrimaryAssetId AssetId)
{
	if (AssetId.IsValid())
	{
		TArray<FName> AssetPackageArray;

		AssetManager->GetManagedPackageList(AssetId, AssetPackageArray);

		AddAssetsToList(AssetPackageArray, false);
	}
}

void SAssetAuditBrowser::AddAssetsOfClass(UClass* AssetClass)
{
	FSlateApplication::Get().DismissAllMenus();

	if (AssetClass)
	{
		TArray<FAssetData> FoundData;
		FARFilter AssetFilter;
		AssetFilter.ClassNames.Add(AssetClass->GetFName());
		AssetFilter.bRecursiveClasses = true;

		if (AssetRegistry->GetAssets(AssetFilter, FoundData) && FoundData.Num() > 0)
		{
			TArray<FName> AssetPackageArray;

			for (FAssetData& AssetData : FoundData)
			{
				AssetPackageArray.Add(AssetData.PackageName);
			}

			AddAssetsToList(AssetPackageArray, false);
		}
	}
}

TSharedRef<SWidget> SAssetAuditBrowser::CreateClassPicker()
{
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.DisplayMode = EClassViewerDisplayMode::ListView;
	Options.bShowObjectRootClass = true;
	Options.bShowNoneOption = false;

	// This will allow unloaded blueprints to be shown.
	Options.bShowUnloadedBlueprints = true;

	return FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &SAssetAuditBrowser::AddAssetsOfClass));
}

EVisibility SAssetAuditBrowser::GetHistoryVisibility() const
{
	return EVisibility::Visible;
}

bool SAssetAuditBrowser::HandleFilterAsset(const FAssetData& InAssetData) const
{
	if (CurrentPlatformState)
	{
		const FAssetPackageData* FoundPackageData = CurrentPlatformState->GetAssetPackageData(InAssetData.PackageName);

		// Check if it was actually cooked for this platform, negative size means it was not cooked
		if (!FoundPackageData || FoundPackageData->DiskSize < 0)
		{
			return true;
		}
	}

	return false;
}

TSharedRef<SWidget> SAssetAuditBrowser::GeneratePlatformComboItem(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock)
		.Text(FText::FromString(*InItem));
}

void SAssetAuditBrowser::HandlePlatformComboChanged(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo)
{
	for (int32 i = 0; i < PlatformComboList.Num(); i++)
	{
		if (Item == PlatformComboList[i])
		{
			CurrentPlatformString = *Item.Get();
			CurrentTargetPlatform = PlatformList[i];

			// Cache registry, may be null
			CurrentPlatformState = EditorModule->GetAssetRegistryStateForTargetPlatform(CurrentTargetPlatform);

			// Refresh
			RefreshAssetView();
		}
	}
}

FText SAssetAuditBrowser::GetPlatformComboText() const
{
	return FText::FromString(CurrentPlatformString);
}

#undef LOCTEXT_NAMESPACE

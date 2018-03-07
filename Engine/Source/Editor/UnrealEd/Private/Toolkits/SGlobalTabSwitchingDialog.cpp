// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Toolkits/SGlobalTabSwitchingDialog.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/Views/SListView.h"
#include "Framework/Docking/TabManager.h"
#include "EditorStyleSet.h"
#include "Widgets/Docking/SDockTab.h"

#include "EngineGlobals.h"
#include "Editor.h"
#include "Toolkits/AssetEditorManager.h"
#include "LevelEditor.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructure.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"

#if PLATFORM_MAC
#include "MacApplication.h"
#endif

#define LOCTEXT_NAMESPACE "SGlobalTabSwitchingDialog"

//////////////////////////////////////////////////////////////////////////
// FTabSwitchingListItemBase

class FTabSwitchingListItemBase
{
public:
	FTabSwitchingListItemBase()
		: LastAccessTime(0.0)
	{
	}

	virtual ~FTabSwitchingListItemBase() {}

	virtual TSharedRef<SWidget> CreateWidget(TSharedPtr<class FAssetThumbnailPool> AssetThumbnailPool)
	{
		return SNullWidget::NullWidget;
	}

	virtual FText GetTypeString() const
	{
		return FText::GetEmpty();
	}

	virtual FText GetPathString() const
	{
		return FText::GetEmpty();
	}

	virtual void ActivateTab() { }

	virtual void ShowInContentBrowser()
	{
	}

	virtual TSharedPtr<FTabManager> GetAssociatedTabManager()
	{
		return TSharedPtr<FTabManager>();
	}
public:
	double LastAccessTime;
};

//////////////////////////////////////////////////////////////////////////
// FTabSwitchingListItem_Asset

class FTabSwitchingListItem_Asset : public FTabSwitchingListItemBase
{
public:
	FTabSwitchingListItem_Asset(UObject* InAsset)
		: MyAsset(InAsset)
	{
		if (IAssetEditorInstance* EditorInstance = FAssetEditorManager::Get().FindEditorForAsset(MyAsset, /*bFocusIfOpen=*/ false))
		{
			LastAccessTime = EditorInstance->GetLastActivationTime();
		}
	}

	virtual TSharedRef<SWidget> CreateWidget(TSharedPtr<class FAssetThumbnailPool> AssetThumbnailPool) override
	{
		// Create a label for the asset name
		const bool bDirtyState = MyAsset->GetOutermost()->IsDirty();
		FFormatNamedArguments Args;
		Args.Add(TEXT("AssetName"), FText::AsCultureInvariant(MyAsset->GetName()));
		Args.Add(TEXT("DirtyState"), bDirtyState ? LOCTEXT("AssetModified", " [Modified]") : LOCTEXT("AssetNotModified", ""));
		FText AssetText = FText::Format(LOCTEXT("AssetEntryLabel", "{AssetName}{DirtyState}"), Args);

		// Create a thumbnail to represent the asset type
		const int32 ThumbnailSize = 48;

		FAssetData AssetData(MyAsset);
		Thumbnail = MakeShareable(new FAssetThumbnail(MyAsset, ThumbnailSize, ThumbnailSize, AssetThumbnailPool));

		FAssetThumbnailConfig ThumbnailConfig;

		return SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 4.0f)
			[
				SNew(SBox)
				.WidthOverride(ThumbnailSize)
				.HeightOverride(ThumbnailSize)
				[
					Thumbnail->MakeThumbnailWidget(ThumbnailConfig)
				]
			]
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(STextBlock)
				.TextStyle(FEditorStyle::Get(), "ControlTabMenu.AssetNameStyle")
				.Text(AssetText)
			];
	}

	virtual void ShowInContentBrowser() override
	{
		TArray<UObject*> ObjectsToSync;
		ObjectsToSync.Add(MyAsset);
		GEditor->SyncBrowserToObjects(ObjectsToSync);
	}

	virtual FText GetTypeString() const override
	{
		return MyAsset->GetClass()->GetDisplayNameText();
	}

	virtual FText GetPathString() const override
	{
		return FText::AsCultureInvariant(MyAsset->GetOutermost()->GetName());
	}

	virtual void ActivateTab() override
	{
		FAssetEditorManager::Get().FindEditorForAsset(MyAsset, /*bFocusIfOpen=*/ true);
	}

	virtual TSharedPtr<FTabManager> GetAssociatedTabManager() override
	{
		IAssetEditorInstance* Instance = FAssetEditorManager::Get().FindEditorForAsset(MyAsset, /*bFocusIfOpen=*/ false);
		if (Instance)
		{
			return Instance->GetAssociatedTabManager();
		}

		return nullptr;
	}

protected:
	UObject* MyAsset;
	TSharedPtr<class FAssetThumbnail> Thumbnail;
};

//////////////////////////////////////////////////////////////////////////
// FTabSwitchingListItem_World

class FTabSwitchingListItem_World : public FTabSwitchingListItem_Asset
{
public:
	static TSharedPtr<FTabSwitchingListItem_World> MakeWorldItem()
	{
		UWorld* MyWorld = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE)
			{
				MyWorld = Context.World();
				break;
			}
			else if (Context.WorldType == EWorldType::Editor)
			{
				MyWorld = Context.World();
			}
		}

		check(MyWorld != nullptr);
		return MakeShareable(new FTabSwitchingListItem_World(MyWorld));
	}

	virtual void ActivateTab() override
	{
		const TSharedPtr<FTabManager> LevelEditorTabManager = GetAssociatedTabManager();
		FGlobalTabmanager::Get()->DrawAttention(LevelEditorTabManager->GetOwnerTab().ToSharedRef());
	}

	virtual TSharedPtr<FTabManager> GetAssociatedTabManager() override
	{
		return FModuleManager::Get().GetModuleChecked<FLevelEditorModule>("LevelEditor").GetLevelEditorTabManager();
	}

protected:
	FTabSwitchingListItem_World(UWorld* InWorld)
		: FTabSwitchingListItem_Asset(InWorld)
	{
		const TSharedPtr<SDockTab> LevelEditorTabPtr = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>("LevelEditor").GetLevelEditorTab();
		LastAccessTime = LevelEditorTabPtr->GetLastActivationTime();
	}
};

//////////////////////////////////////////////////////////////////////////
// FTabSwitchingListItem_Asset

bool SGlobalTabSwitchingDialog::bIsAlreadyOpen = false;

SGlobalTabSwitchingDialog::~SGlobalTabSwitchingDialog()
{
	bIsAlreadyOpen = false;

#if PLATFORM_MAC
	MacApplication->SetIsRightClickEmulationEnabled(true);
#endif
}

SGlobalTabSwitchingDialog::FTabListItemPtr SGlobalTabSwitchingDialog::GetMainTabListSelectedItem() const
{
	TArray<FTabListItemPtr> SelectedItems = MainTabsListWidget->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		return SelectedItems[0];
	}
	else
	{
		return FTabListItemPtr();
	}
}

FReply SGlobalTabSwitchingDialog::OnBrowseToSelectedAsset()
{
	FTabListItemPtr SelectedItem = GetMainTabListSelectedItem();
	if (SelectedItem.IsValid())
	{
		SelectedItem->ShowInContentBrowser();

		FSlateApplication::Get().DismissAllMenus();
	}
	return FReply::Handled();
}

void SGlobalTabSwitchingDialog::OnMainTabListSelectionChanged(FTabListItemPtr InItem, ESelectInfo::Type SelectInfo)
{
	TSharedRef<SWidget> NewTopContents = SNullWidget::NullWidget;
	TSharedRef<SWidget> NewBottomContents = SNullWidget::NullWidget;
	TSharedRef<SWidget> NewToolTabsContent = SNullWidget::NullWidget;

	TArray<FTabListItemPtr> SelectedItems = MainTabsListWidget->GetSelectedItems();

	if (SelectedItems.Num() > 0)
	{
		FTabListItemPtr SelectedItem = SelectedItems[0];

		NewTopContents = SelectedItem->CreateWidget(AssetThumbnailPool);

		NewBottomContents =
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "HoverOnlyHyperlinkButton")
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.OnClicked(this, &SGlobalTabSwitchingDialog::OnBrowseToSelectedAsset)
				.ToolTipText(LOCTEXT("BrowseButtonToolTipText", "Browse to Asset in Content Browser"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.Button_Browse"))
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
					[
						SNew(STextBlock)
						.TextStyle(FEditorStyle::Get(), "ControlTabMenu.AssetPathStyle")
						.Text(SelectedItem->GetPathString())
					]
				]
			]
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNullWidget::NullWidget
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.TextStyle(FEditorStyle::Get(), "ControlTabMenu.AssetTypeStyle")
				.Text(SelectedItem->GetTypeString())
			];

		// Create the list of the tool tabs in the current active tab
		TSharedPtr<FTabManager> SelectedAssetTabManager = SelectedItem->GetAssociatedTabManager();
		if (SelectedAssetTabManager.IsValid())
		{
			FMenuBuilder ToolTabMenuBuilder(/*bShouldCloseAfterSelection=*/ true, /*CommandList=*/ nullptr);
			ToolTabMenuBuilder.GetMultiBox()->SetStyle(&FEditorStyle::Get(), "ToolBar");
			
			// Local editor tabs
			SelectedAssetTabManager->PopulateLocalTabSpawnerMenu(ToolTabMenuBuilder);
			
			// General tabs
			const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();
			SelectedAssetTabManager->PopulateTabSpawnerMenu(ToolTabMenuBuilder, MenuStructure.GetStructureRoot());

			// Turn the builder into a widget
			NewToolTabsContent = ToolTabMenuBuilder.MakeWidget();
		}
	}

	NewTabItemToActivateDisplayBox->SetContent(NewTopContents);
	NewTabItemToActivatePathBox->SetContent(NewBottomContents);
	ToolTabsListBox->SetContent(NewToolTabsContent);
}

void SGlobalTabSwitchingDialog::OnMainTabListItemClicked(FTabListItemPtr InItem)
{
	DismissDialog();
}

void SGlobalTabSwitchingDialog::CycleSelection(bool bForwards)
{
	// This is done here each time in case someone clicks off of the selected item (and to prime the pump at startup),
	// otherwise the code below wouldn't cycle back into an item if nothing was selected
	if ((MainTabsListWidget->GetNumItemsSelected() == 0) && (MainTabsListDataSource.Num() > 0))
	{
		MainTabsListWidget->SetSelection(MainTabsListDataSource[0]);
	}

	// Move to the next/previous item
	FTabListItemPtr OldSelectedItem = GetMainTabListSelectedItem();
	if (OldSelectedItem.IsValid())
	{
		int32 OldSelectionIndex;
		if (MainTabsListDataSource.Find(OldSelectedItem, /*out*/ OldSelectionIndex))
		{
			const int32 NewSelectionIndex = (OldSelectionIndex + MainTabsListDataSource.Num() + (bForwards ? 1 : -1)) % MainTabsListDataSource.Num();
			if (NewSelectionIndex != OldSelectionIndex)
			{
				FTabListItemPtr NewSelectedItem = MainTabsListDataSource[NewSelectionIndex];
				MainTabsListWidget->SetSelection(NewSelectedItem);
				MainTabsListWidget->RequestScrollIntoView(NewSelectedItem);
			}
		}
	}
}

void SGlobalTabSwitchingDialog::DismissDialog()
{
	FTabListItemPtr SelectedItem = GetMainTabListSelectedItem();
	if (SelectedItem.IsValid())
	{
		SelectedItem->ActivateTab();
	}

	FSlateApplication::Get().DismissAllMenus();
}

void SGlobalTabSwitchingDialog::Construct(const FArguments& InArgs, FVector2D InSize, FInputChord InTriggerChord)
{
	check(!bIsAlreadyOpen);
	bIsAlreadyOpen = true;

#if PLATFORM_MAC
	// On Mac we emulate right click with ctrl+left click. This needs to be disabled for tab navigator, so that users can click on its widgets while they keep the ctrl key pressed
	MacApplication->SetIsRightClickEmulationEnabled(false);
#endif

	TriggerChord = InTriggerChord;

	AssetThumbnailPool = MakeShareable(new FAssetThumbnailPool(128));

	// Populate the list with open asset editors
	TArray<UObject*> OpenAssetList = FAssetEditorManager::Get().GetAllEditedAssets();
	for (UObject* OpenAsset : OpenAssetList)
	{
		MainTabsListDataSource.Add(MakeShareable(new FTabSwitchingListItem_Asset(OpenAsset)));
	}

	MainTabsListDataSource.Add(FTabSwitchingListItem_World::MakeWorldItem());

	// Sort the list by access time
	MainTabsListDataSource.Sort([](const FTabListItemPtr& A, const FTabListItemPtr& B) { return (A->LastAccessTime > B->LastAccessTime); });

	// Create the widgets
	NewTabItemToActivateDisplayBox =
		SNew(SBox)
		.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
		.HeightOverride(70.0f)
		.VAlign(VAlign_Top);

	NewTabItemToActivatePathBox =
		SNew(SBox)
		.Padding(FMargin(0.0f, 10.0f, 10.0f, 10.0f))
		.HeightOverride(40.0f)
		.VAlign(VAlign_Center);

	ToolTabsListBox =
		SNew(SBox)
		.Padding(FMargin(0.0f, 0.0f, 15.0f, 0.0f));

	MainTabsListWidget = SNew(STabListWidget)
		.ItemHeight(64)
		.ListItemsSource(&MainTabsListDataSource)
		.OnGenerateRow(this, &SGlobalTabSwitchingDialog::OnGenerateTabSwitchListItemWidget)
		.OnSelectionChanged(this, &SGlobalTabSwitchingDialog::OnMainTabListSelectionChanged)
		.OnMouseButtonClick(this, &SGlobalTabSwitchingDialog::OnMainTabListItemClicked)
		.SelectionMode(ESelectionMode::Single);

	TSharedRef<SWidget> ToolTabList = SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.TextStyle(FEditorStyle::Get(), "ControlTabMenu.HeadingStyle")
			.Text(LOCTEXT("ChangeToolsHeading", "Tool Windows"))
		]
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			ToolTabsListBox.ToSharedRef()
		];

	TSharedRef<SWidget> DocumentTabList = SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.TextStyle(FEditorStyle::Get(), "ControlTabMenu.HeadingStyle")
			.Text(LOCTEXT("OpenAssetsHeading", "Active Files"))
		]
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SScrollBorder, MainTabsListWidget.ToSharedRef())
			[
				MainTabsListWidget.ToSharedRef()
			]
		];

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::Get().GetBrush("ControlTabMenu.Background"))
		.ForegroundColor(FCoreStyle::Get().GetSlateColor("DefaultForeground"))
		[
			SNew(SBox)
			.WidthOverride(InSize.X)
			.HeightOverride(InSize.Y)
			.Padding(FMargin(12.0f, 12.0f, 12.0f, 0.0f))
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					NewTabItemToActivateDisplayBox.ToSharedRef()
				]
				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(0.4f)
					.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
					[
						ToolTabList
					]

					+SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						DocumentTabList
					]

				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					NewTabItemToActivatePathBox.ToSharedRef()
				]
			]
		]
	];

	// Pick the second most recent or least recent file based on whether Shift was held down when we were summoned
	if (MainTabsListDataSource.Num() > 0)
	{
		CycleSelection(/*bForwards=*/ !FSlateApplication::Get().GetModifierKeys().IsShiftDown());
	}
}

FReply SGlobalTabSwitchingDialog::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// Check to see if the trigger modifier key was released, which should close the dialog
	const bool bCloseViaControl = TriggerChord.NeedsControl() && ((InKeyEvent.GetKey() == EKeys::LeftControl) || (InKeyEvent.GetKey() == EKeys::RightControl));
	const bool bCloseViaCommand = TriggerChord.NeedsCommand() && ((InKeyEvent.GetKey() == EKeys::LeftCommand) || (InKeyEvent.GetKey() == EKeys::RightCommand));
	const bool bCloseViaAlt = TriggerChord.NeedsAlt() && ((InKeyEvent.GetKey() == EKeys::LeftAlt) || (InKeyEvent.GetKey() == EKeys::RightAlt));
	
	if (bCloseViaControl || bCloseViaCommand || bCloseViaAlt)
	{
		DismissDialog();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SGlobalTabSwitchingDialog::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (TriggerChord.Key == InKeyEvent.GetKey())
	{
		const bool bCycleForward = !InKeyEvent.IsShiftDown();
		CycleSelection(bCycleForward);
	}

	return FReply::Unhandled();
}

FReply SGlobalTabSwitchingDialog::OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		FSlateApplication::Get().DismissAllMenus();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TSharedRef<ITableRow> SGlobalTabSwitchingDialog::OnGenerateTabSwitchListItemWidget(FTabListItemPtr InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<FTabListItemPtr>, OwnerTable)[InItem->CreateWidget(AssetThumbnailPool)];
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE

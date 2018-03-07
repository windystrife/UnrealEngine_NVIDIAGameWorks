// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SCollectionView.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SOverlay.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Images/SImage.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "CollectionManagerModule.h"
#include "ContentBrowserUtils.h"
#include "HistoryManager.h"

#include "CollectionAssetManagement.h"
#include "CollectionContextMenu.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "DragAndDrop/CollectionDragDropOp.h"
#include "SourcesViewWidgets.h"
#include "ContentBrowserModule.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Input/SSearchBox.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

namespace CollectionViewFilter
{

void GetBasicStrings(const FCollectionItem& InCollection, TArray<FString>& OutBasicStrings)
{
	OutBasicStrings.Add(InCollection.CollectionName.ToString());
}

bool TestComplexExpression(const FCollectionItem& InCollection, const FName& InKey, const FTextFilterString& InValue, ETextFilterComparisonOperation InComparisonOperation, ETextFilterTextComparisonMode InTextComparisonMode)
{
	static const FName NameKeyName = "Name";
	static const FName TypeKeyName = "Type";

	// Handle the collection name
	if (InKey == NameKeyName)
	{
		// Names can only work with Equal or NotEqual type tests
		if (InComparisonOperation != ETextFilterComparisonOperation::Equal && InComparisonOperation != ETextFilterComparisonOperation::NotEqual)
		{
			return false;
		}

		const bool bIsMatch = TextFilterUtils::TestBasicStringExpression(InCollection.CollectionName.ToString(), InValue, InTextComparisonMode);
		return (InComparisonOperation == ETextFilterComparisonOperation::Equal) ? bIsMatch : !bIsMatch;
	}

	// Handle the collection type
	if (InKey == TypeKeyName)
	{
		// Types can only work with Equal or NotEqual type tests
		if (InComparisonOperation != ETextFilterComparisonOperation::Equal && InComparisonOperation != ETextFilterComparisonOperation::NotEqual)
		{
			return false;
		}

		const bool bIsMatch = TextFilterUtils::TestBasicStringExpression(ECollectionShareType::ToString(InCollection.CollectionType), InValue, InTextComparisonMode);
		return (InComparisonOperation == ETextFilterComparisonOperation::Equal) ? bIsMatch : !bIsMatch;
	}

	return false;
}

} // namespace CollectionViewFilter

void SCollectionView::Construct( const FArguments& InArgs )
{
	OnCollectionSelected = InArgs._OnCollectionSelected;
	bAllowCollectionButtons = InArgs._AllowCollectionButtons;
	bAllowRightClickMenu = InArgs._AllowRightClickMenu;
	bAllowCollectionDrag = InArgs._AllowCollectionDrag;
	bDraggedOver = false;

	bQueueCollectionItemsUpdate = false;
	bQueueSCCRefresh = true;

	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
	CollectionManagerModule.Get().OnCollectionCreated().AddSP( this, &SCollectionView::HandleCollectionCreated );
	CollectionManagerModule.Get().OnCollectionRenamed().AddSP( this, &SCollectionView::HandleCollectionRenamed );
	CollectionManagerModule.Get().OnCollectionReparented().AddSP( this, &SCollectionView::HandleCollectionReparented );
	CollectionManagerModule.Get().OnCollectionDestroyed().AddSP( this, &SCollectionView::HandleCollectionDestroyed );
	CollectionManagerModule.Get().OnCollectionUpdated().AddSP( this, &SCollectionView::HandleCollectionUpdated );
	CollectionManagerModule.Get().OnAssetsAdded().AddSP( this, &SCollectionView::HandleAssetsAddedToCollection );
	CollectionManagerModule.Get().OnAssetsRemoved().AddSP( this, &SCollectionView::HandleAssetsRemovedFromCollection );

	ISourceControlModule::Get().RegisterProviderChanged(FSourceControlProviderChanged::FDelegate::CreateSP(this, &SCollectionView::HandleSourceControlProviderChanged));
	SourceControlStateChangedDelegateHandle = ISourceControlModule::Get().GetProvider().RegisterSourceControlStateChanged_Handle(FSourceControlStateChanged::FDelegate::CreateSP(this, &SCollectionView::HandleSourceControlStateChanged));

	Commands = TSharedPtr< FUICommandList >(new FUICommandList);
	CollectionContextMenu = MakeShareable(new FCollectionContextMenu( SharedThis(this) ));
	CollectionContextMenu->BindCommands(Commands);

	CollectionItemTextFilter = MakeShareable(new FCollectionItemTextFilter(
		FCollectionItemTextFilter::FItemToStringArray::CreateStatic(&CollectionViewFilter::GetBasicStrings), 
		FCollectionItemTextFilter::FItemTestComplexExpression::CreateStatic(&CollectionViewFilter::TestComplexExpression)
		));
	CollectionItemTextFilter->OnChanged().AddSP(this, &SCollectionView::UpdateFilteredCollectionItems);

	if ( InArgs._AllowQuickAssetManagement )
	{
		QuickAssetManagement = MakeShareable(new FCollectionAssetManagement());
	}

	FOnContextMenuOpening CollectionListContextMenuOpening;
	if ( InArgs._AllowContextMenu )
	{
		CollectionListContextMenuOpening = FOnContextMenuOpening::CreateSP( this, &SCollectionView::MakeCollectionTreeContextMenu );
	}

	PreventSelectionChangedDelegateCount = 0;

	TSharedRef< SWidget > HeaderContent = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				[
					SNew(STextBlock)
					.Font( FEditorStyle::GetFontStyle("ContentBrowser.SourceTitleFont") )
					.Text( LOCTEXT("CollectionsListTitle", "Collections") )
					.Visibility( this, &SCollectionView::GetCollectionsTitleTextVisibility )
				]

				+ SHorizontalBox::Slot()
				[
					SAssignNew(SearchBoxPtr, SSearchBox)
					.HintText( LOCTEXT( "CollectionsViewSearchBoxHint", "Search Collections" ) )
					.OnTextChanged( this, &SCollectionView::SetCollectionsSearchFilterText )
					.Visibility( this, &SCollectionView::GetCollectionsSearchBoxVisibility )
				]
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "FlatButton")
				.ToolTipText(LOCTEXT("AddCollectionButtonTooltip", "Add a collection."))
				.OnClicked(this, &SCollectionView::MakeAddCollectionMenu)
				.ContentPadding( FMargin(2, 2) )
				.Visibility(this, &SCollectionView::GetAddCollectionButtonVisibility)
				[
					SNew(SImage) .Image( FEditorStyle::GetBrush("ContentBrowser.AddCollectionButtonIcon") )
				]
			];

	TSharedRef< SWidget > BodyContent = SNew(SVerticalBox)
			// Separator
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
			]

			// Collections tree
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SAssignNew(CollectionTreePtr, STreeView< TSharedPtr<FCollectionItem> >)
				.TreeItemsSource(&VisibleRootCollectionItems)
				.OnGenerateRow(this, &SCollectionView::GenerateCollectionRow)
				.OnGetChildren(this, &SCollectionView::GetCollectionItemChildren)
				.ItemHeight(18)
				.SelectionMode(ESelectionMode::Multi)
				.OnSelectionChanged(this, &SCollectionView::CollectionSelectionChanged)
				.OnContextMenuOpening(CollectionListContextMenuOpening)
				.OnItemScrolledIntoView(this, &SCollectionView::CollectionItemScrolledIntoView)
				.ClearSelectionOnClick(false)
				.Visibility(this, &SCollectionView::GetCollectionTreeVisibility)
			];

	TSharedPtr< SWidget > Content;
	if ( InArgs._AllowCollapsing )
	{
		Content = SAssignNew(CollectionsExpandableAreaPtr, SExpandableArea)
			.MaxHeight(200)
			.BorderImage( FEditorStyle::GetBrush("NoBorder") )
			.HeaderPadding( FMargin(4.0f, 0.0f, 0.0f, 0.0f) )
			.HeaderContent()
			[
				SNew(SBox)
				.Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
				[
					HeaderContent
				]
			]
			.BodyContent()
			[
				BodyContent
			];
	}
	else
	{
		Content = SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			HeaderContent
		]

		+SVerticalBox::Slot()
		[
			BodyContent
		];
	}

	ChildSlot
	[
		SNew(SOverlay)

		// Main content
		+SOverlay::Slot()
		[
			Content.ToSharedRef()
		]

		// Drop target overlay
		+SOverlay::Slot()
		[
			SNew(SBorder)
			.Padding(0)
			.Visibility(EVisibility::HitTestInvisible)
			.BorderImage(this, &SCollectionView::GetCollectionViewDropTargetBorder)
			.BorderBackgroundColor(FLinearColor::Yellow)
			[
				SNullWidget::NullWidget
			]
		]
	];

	UpdateCollectionItems();
}

void SCollectionView::HandleCollectionCreated( const FCollectionNameType& Collection )
{
	bQueueCollectionItemsUpdate = true;
}

void SCollectionView::HandleCollectionRenamed( const FCollectionNameType& OriginalCollection, const FCollectionNameType& NewCollection )
{
	bQueueCollectionItemsUpdate = true;

	// Rename the item in-place so we can maintain its expansion and selection states correctly once the view is refreshed on the next Tick
	TSharedPtr<FCollectionItem> CollectionItem = AvailableCollections.FindRef(OriginalCollection);
	if (CollectionItem.IsValid())
	{
		CollectionItem->CollectionName = NewCollection.Name;
		CollectionItem->CollectionType = NewCollection.Type;

		AvailableCollections.Remove(OriginalCollection);
		AvailableCollections.Add(NewCollection, CollectionItem);
	}
}

void SCollectionView::HandleCollectionReparented( const FCollectionNameType& Collection, const TOptional<FCollectionNameType>& OldParent, const TOptional<FCollectionNameType>& NewParent )
{
	bQueueCollectionItemsUpdate = true;
}

void SCollectionView::HandleCollectionDestroyed( const FCollectionNameType& Collection )
{
	bQueueCollectionItemsUpdate = true;
}

void SCollectionView::HandleCollectionUpdated( const FCollectionNameType& Collection )
{
	TSharedPtr<FCollectionItem> CollectionItemToUpdate = AvailableCollections.FindRef(Collection);
	if (CollectionItemToUpdate.IsValid())
	{
		bQueueSCCRefresh = true;
		UpdateCollectionItemStatus(CollectionItemToUpdate.ToSharedRef());
	}
}

void SCollectionView::HandleAssetsAddedToCollection( const FCollectionNameType& Collection, const TArray<FName>& AssetsAdded )
{
	HandleCollectionUpdated(Collection);
}

void SCollectionView::HandleAssetsRemovedFromCollection( const FCollectionNameType& Collection, const TArray<FName>& AssetsRemoved )
{
	HandleCollectionUpdated(Collection);
}

void SCollectionView::HandleSourceControlProviderChanged(ISourceControlProvider& OldProvider, ISourceControlProvider& NewProvider)
{
	OldProvider.UnregisterSourceControlStateChanged_Handle(SourceControlStateChangedDelegateHandle);
	SourceControlStateChangedDelegateHandle = NewProvider.RegisterSourceControlStateChanged_Handle(FSourceControlStateChanged::FDelegate::CreateSP(this, &SCollectionView::HandleSourceControlStateChanged));
	
	bQueueSCCRefresh = true;
	HandleSourceControlStateChanged();
}

void SCollectionView::HandleSourceControlStateChanged()
{
	// Update the status of each collection
	for (const auto& AvailableCollectionInfo : AvailableCollections)
	{
		UpdateCollectionItemStatus(AvailableCollectionInfo.Value.ToSharedRef());
	}
}

void SCollectionView::UpdateCollectionItemStatus( const TSharedRef<FCollectionItem>& CollectionItem )
{
	TOptional<ECollectionItemStatus> NewStatus;

	// Check IsModuleAvailable as we might be in the process of shutting down, and were notified due to the SCC provider being nulled out...
	if (FCollectionManagerModule::IsModuleAvailable())
	{
		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

		FCollectionStatusInfo StatusInfo;
		if (CollectionManagerModule.Get().GetCollectionStatusInfo(CollectionItem->CollectionName, CollectionItem->CollectionType, StatusInfo))
		{
			// Test the SCC state first as this should take priority when reporting the status back to the user
			if (StatusInfo.bUseSCC)
			{
				if (StatusInfo.SCCState.IsValid() && StatusInfo.SCCState->IsSourceControlled())
				{
					if (StatusInfo.SCCState->IsCheckedOutOther())
					{
						NewStatus = ECollectionItemStatus::IsCheckedOutByAnotherUser;
					}
					else if (StatusInfo.SCCState->IsConflicted())
					{
						NewStatus = ECollectionItemStatus::IsConflicted;
					}
					else if (!StatusInfo.SCCState->IsCurrent())
					{
						NewStatus = ECollectionItemStatus::IsOutOfDate;
					}
					else if (StatusInfo.SCCState->IsModified())
					{
						NewStatus = ECollectionItemStatus::HasLocalChanges;
					}
				}
				else
				{
					NewStatus = ECollectionItemStatus::IsMissingSCCProvider;
				}
			}

			// Not set by the SCC status, so check just use the local state
			if (!NewStatus.IsSet())
			{
				if (StatusInfo.bIsDirty)
				{
					NewStatus = ECollectionItemStatus::HasLocalChanges;
				}
				else if (StatusInfo.bIsEmpty)
				{
					NewStatus = ECollectionItemStatus::IsUpToDateAndEmpty;
				}
				else
				{
					NewStatus = ECollectionItemStatus::IsUpToDateAndPopulated;
				}
			}
		}
	}

	CollectionItem->CurrentStatus = NewStatus.Get(ECollectionItemStatus::IsUpToDateAndEmpty);
}

void SCollectionView::UpdateCollectionItems()
{
	struct FGatherCollectionItems
	{
		FGatherCollectionItems()
			: CollectionManagerModule(FCollectionManagerModule::GetModule())
		{
		}

		void GatherCollectionItems(FAvailableCollectionsMap& OutAvailableCollections)
		{
			OutAvailableCollections.Reset();

			TArray<FCollectionNameType> RootCollections;
			CollectionManagerModule.Get().GetRootCollections(RootCollections);

			ProcessGatheredCollectionsAndRecurse(RootCollections, nullptr, OutAvailableCollections);
		}

		void GatherChildCollectionItems(const TSharedPtr<FCollectionItem>& InParentCollectionItem, FAvailableCollectionsMap& OutAvailableCollections)
		{
			TArray<FCollectionNameType> ChildCollections;
			CollectionManagerModule.Get().GetChildCollections(InParentCollectionItem->CollectionName, InParentCollectionItem->CollectionType, ChildCollections);

			ProcessGatheredCollectionsAndRecurse(ChildCollections, InParentCollectionItem, OutAvailableCollections);
		}

		void ProcessGatheredCollectionsAndRecurse(const TArray<FCollectionNameType>& InCollections, const TSharedPtr<FCollectionItem>& InParentCollectionItem, FAvailableCollectionsMap& OutAvailableCollections)
		{
			for (const FCollectionNameType& Collection : InCollections)
			{
				// Never display system collections
				if (Collection.Type == ECollectionShareType::CST_System)
				{
					continue;
				}

				TSharedRef<FCollectionItem> CollectionItem = MakeShareable(new FCollectionItem(Collection.Name, Collection.Type));
				OutAvailableCollections.Add(Collection, CollectionItem);

				CollectionManagerModule.Get().GetCollectionStorageMode(Collection.Name, Collection.Type, CollectionItem->StorageMode);

				SCollectionView::UpdateCollectionItemStatus(CollectionItem);

				if (InParentCollectionItem.IsValid())
				{
					// Fixup the parent and child pointers
					InParentCollectionItem->ChildCollections.Add(CollectionItem);
					CollectionItem->ParentCollection = InParentCollectionItem;
				}

				// Recurse
				GatherChildCollectionItems(CollectionItem, OutAvailableCollections);
			}
		}

		FCollectionManagerModule& CollectionManagerModule;
	};

	// Backup the current selection and expansion state of our collections
	// We're about to re-create the tree, so we'll need to re-apply this again afterwards
	TArray<FCollectionNameType> SelectedCollections;
	TArray<FCollectionNameType> ExpandedCollections;
	{
		const auto SelectedCollectionItems = CollectionTreePtr->GetSelectedItems();
		SelectedCollections.Reserve(SelectedCollectionItems.Num());
		for (const TSharedPtr<FCollectionItem>& SelectedCollectionItem : SelectedCollectionItems)
		{
			SelectedCollections.Add(FCollectionNameType(SelectedCollectionItem->CollectionName, SelectedCollectionItem->CollectionType));
		}
	}
	{
		TSet<TSharedPtr<FCollectionItem>> ExpandedCollectionItems;
		CollectionTreePtr->GetExpandedItems(ExpandedCollectionItems);
		ExpandedCollections.Reserve(ExpandedCollectionItems.Num());
		for (const TSharedPtr<FCollectionItem>& ExpandedCollectionItem : ExpandedCollectionItems)
		{
			ExpandedCollections.Add(FCollectionNameType(ExpandedCollectionItem->CollectionName, ExpandedCollectionItem->CollectionType));
		}
	}

	FGatherCollectionItems GatherCollectionItems;
	GatherCollectionItems.GatherCollectionItems(AvailableCollections);

	UpdateFilteredCollectionItems();

	// Restore selection and expansion
	SetSelectedCollections(SelectedCollections, false);
	SetExpandedCollections(ExpandedCollections);

	bQueueSCCRefresh = true;
}

void SCollectionView::UpdateFilteredCollectionItems()
{
	VisibleCollections.Reset();
	VisibleRootCollectionItems.Reset();

	auto AddVisibleCollection = [&](const TSharedPtr<FCollectionItem>& InCollectionItem)
	{
		VisibleCollections.Add(FCollectionNameType(InCollectionItem->CollectionName, InCollectionItem->CollectionType));
		if (!InCollectionItem->ParentCollection.IsValid())
		{
			VisibleRootCollectionItems.AddUnique(InCollectionItem);
		}
	};

	auto AddVisibleCollectionRecursive = [&](const TSharedPtr<FCollectionItem>& InCollectionItem)
	{
		TSharedPtr<FCollectionItem> CollectionItemToAdd = InCollectionItem;
		do
		{
			AddVisibleCollection(CollectionItemToAdd);
			CollectionItemToAdd = CollectionItemToAdd->ParentCollection.Pin();
		}
		while(CollectionItemToAdd.IsValid());
	};

	// Do we have an active filter to test against?
	if (CollectionItemTextFilter->GetRawFilterText().IsEmpty())
	{
		// No filter, just mark everything as visible
		for (const auto& AvailableCollectionInfo : AvailableCollections)
		{
			AddVisibleCollection(AvailableCollectionInfo.Value);
		}
	}
	else
	{
		TArray<TSharedRef<FCollectionItem>> CollectionsToExpandTo;

		// Test everything against the filter - a visible child needs to make sure its parents are also marked as visible
		for (const auto& AvailableCollectionInfo : AvailableCollections)
		{
			const TSharedPtr<FCollectionItem>& CollectionItem = AvailableCollectionInfo.Value;
			if (CollectionItemTextFilter->PassesFilter(*CollectionItem))
			{
				AddVisibleCollectionRecursive(CollectionItem);
				CollectionsToExpandTo.Add(CollectionItem.ToSharedRef());
			}
		}

		// Make sure all matching items have their parents expanded so they can be seen
		for (const TSharedRef<FCollectionItem>& CollectionItem : CollectionsToExpandTo)
		{
			ExpandParentItems(CollectionItem);
		}
	}

	VisibleRootCollectionItems.Sort(FCollectionItem::FCompareFCollectionItemByName());
	CollectionTreePtr->RequestTreeRefresh();
}

void SCollectionView::SetCollectionsSearchFilterText( const FText& InSearchText )
{
	CollectionItemTextFilter->SetRawFilterText( InSearchText );
	SearchBoxPtr->SetError( CollectionItemTextFilter->GetFilterErrorText() );
}

FText SCollectionView::GetCollectionsSearchFilterText() const
{
	return CollectionItemTextFilter->GetRawFilterText();
}

void SCollectionView::SetSelectedCollections(const TArray<FCollectionNameType>& CollectionsToSelect, const bool bEnsureVisible)
{
	// Prevent the selection changed delegate since the invoking code requested it
	FScopedPreventSelectionChangedDelegate DelegatePrevention( SharedThis(this) );

	// Expand the collections area if we are indeed selecting at least one collection
	if ( bEnsureVisible && CollectionsToSelect.Num() > 0 && CollectionsExpandableAreaPtr.IsValid() )
	{
		CollectionsExpandableAreaPtr->SetExpanded(true);
	}

	// Clear the selection to start, then add the selected items as they are found
	CollectionTreePtr->ClearSelection();

	for (const FCollectionNameType& CollectionToSelect : CollectionsToSelect)
	{
		TSharedPtr<FCollectionItem> CollectionItemToSelect = AvailableCollections.FindRef(CollectionToSelect);
		if (CollectionItemToSelect.IsValid())
		{
			if (bEnsureVisible)
			{
				ExpandParentItems(CollectionItemToSelect.ToSharedRef());
				CollectionTreePtr->RequestScrollIntoView(CollectionItemToSelect);
			}

			CollectionTreePtr->SetItemSelection(CollectionItemToSelect, true);

			// If the selected collection doesn't pass our current filter, we need to clear it
			if (bEnsureVisible && !CollectionItemTextFilter->PassesFilter(*CollectionItemToSelect))
			{
				SearchBoxPtr->SetText(FText::GetEmpty());
			}
		}
	}
}

void SCollectionView::SetExpandedCollections(const TArray<FCollectionNameType>& CollectionsToExpand)
{
	// Clear the expansion to start, then add the expanded items as they are found
	CollectionTreePtr->ClearExpandedItems();

	for (const FCollectionNameType& CollectionToExpand : CollectionsToExpand)
	{
		TSharedPtr<FCollectionItem> CollectionItemToExpand = AvailableCollections.FindRef(CollectionToExpand);
		if (CollectionItemToExpand.IsValid())
		{
			CollectionTreePtr->SetItemExpansion(CollectionItemToExpand, true);
		}
	}
}

void SCollectionView::ClearSelection()
{
	// Prevent the selection changed delegate since the invoking code requested it
	FScopedPreventSelectionChangedDelegate DelegatePrevention( SharedThis(this) );

	// Clear the selection to start, then add the selected paths as they are found
	CollectionTreePtr->ClearSelection();
}

TArray<FCollectionNameType> SCollectionView::GetSelectedCollections() const
{
	TArray<FCollectionNameType> RetArray;

	TArray<TSharedPtr<FCollectionItem>> Items = CollectionTreePtr->GetSelectedItems();
	for ( int32 ItemIdx = 0; ItemIdx < Items.Num(); ++ItemIdx )
	{
		const TSharedPtr<FCollectionItem>& Item = Items[ItemIdx];
		RetArray.Add(FCollectionNameType(Item->CollectionName, Item->CollectionType));
	}

	return RetArray;
}

void SCollectionView::SetSelectedAssets(const TArray<FAssetData>& SelectedAssets)
{
	if ( QuickAssetManagement.IsValid() )
	{
		QuickAssetManagement->SetCurrentAssets(SelectedAssets);
	}
}

void SCollectionView::ApplyHistoryData( const FHistoryData& History )
{	
	// Prevent the selection changed delegate because it would add more history when we are just setting a state
	FScopedPreventSelectionChangedDelegate DelegatePrevention( SharedThis(this) );

	CollectionTreePtr->ClearSelection();
	for (const FCollectionNameType& HistoryCollection : History.SourcesData.Collections)
	{
		TSharedPtr<FCollectionItem> CollectionHistoryItem = AvailableCollections.FindRef(HistoryCollection);
		if (CollectionHistoryItem.IsValid())
		{
			ExpandParentItems(CollectionHistoryItem.ToSharedRef());
			CollectionTreePtr->RequestScrollIntoView(CollectionHistoryItem);
			CollectionTreePtr->SetItemSelection(CollectionHistoryItem, true);
		}
	}
}

void SCollectionView::SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const
{
	auto SaveCollectionsArrayToIni = [&](const FString& InSubKey, const TArray<TSharedPtr<FCollectionItem>>& InCollectionItems)
	{
		FString CollectionsString;

		for (const TSharedPtr<FCollectionItem>& CollectionItem : InCollectionItems)
		{
			if (CollectionsString.Len() > 0)
			{
				CollectionsString += TEXT(",");
			}

			CollectionsString += CollectionItem->CollectionName.ToString();
			CollectionsString += TEXT("?");
			CollectionsString += FString::FromInt(CollectionItem->CollectionType);
		}

		GConfig->SetString(*IniSection, *(SettingsString + InSubKey), *CollectionsString, IniFilename);
	};

	const bool IsCollectionsExpanded = CollectionsExpandableAreaPtr.IsValid() ? CollectionsExpandableAreaPtr->IsExpanded() : true;
	GConfig->SetBool(*IniSection, *(SettingsString + TEXT(".CollectionsExpanded")), IsCollectionsExpanded, IniFilename);
	SaveCollectionsArrayToIni(TEXT(".SelectedCollections"), CollectionTreePtr->GetSelectedItems());
	{
		TSet<TSharedPtr<FCollectionItem>> ExpandedCollectionItems;
		CollectionTreePtr->GetExpandedItems(ExpandedCollectionItems);
		SaveCollectionsArrayToIni(TEXT(".ExpandedCollections"), ExpandedCollectionItems.Array());
	}
}

void SCollectionView::LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString)
{
	auto LoadCollectionsArrayFromIni = [&](const FString& InSubKey) -> TArray<FCollectionNameType>
	{
		TArray<FCollectionNameType> RetCollectionsArray;

		FString CollectionsArrayString;
		if (GConfig->GetString(*IniSection, *(SettingsString + InSubKey), CollectionsArrayString, IniFilename))
		{
			TArray<FString> CollectionStrings;
			CollectionsArrayString.ParseIntoArray(CollectionStrings, TEXT(","), /*bCullEmpty*/true);

			for (const FString& CollectionString : CollectionStrings)
			{
				FString CollectionName;
				FString CollectionTypeString;
				if (CollectionString.Split(TEXT("?"), &CollectionName, &CollectionTypeString))
				{
					const int32 CollectionType = FCString::Atoi(*CollectionTypeString);
					if (CollectionType >= 0 && CollectionType < ECollectionShareType::CST_All)
					{
						RetCollectionsArray.Add(FCollectionNameType(FName(*CollectionName), ECollectionShareType::Type(CollectionType)));
					}
				}
			}
		}

		return RetCollectionsArray;
	};

	// Collection expansion state
	bool bCollectionsExpanded = false;
	if (CollectionsExpandableAreaPtr.IsValid() && GConfig->GetBool(*IniSection, *(SettingsString + TEXT(".CollectionsExpanded")), bCollectionsExpanded, IniFilename))
	{
		CollectionsExpandableAreaPtr->SetExpanded(bCollectionsExpanded);
	}

	// Selected Collections
	TArray<FCollectionNameType> NewSelectedCollections = LoadCollectionsArrayFromIni(TEXT(".SelectedCollections"));
	if (NewSelectedCollections.Num() > 0)
	{
		SetSelectedCollections(NewSelectedCollections);

		const TArray<TSharedPtr<FCollectionItem>> SelectedCollectionItems = CollectionTreePtr->GetSelectedItems();
		if (SelectedCollectionItems.Num() > 0)
		{
			CollectionSelectionChanged(SelectedCollectionItems[0], ESelectInfo::Direct);
		}
	}

	// Expanded Collections
	TArray<FCollectionNameType> NewExpandedCollections = LoadCollectionsArrayFromIni(TEXT(".ExpandedCollections"));
	if (NewExpandedCollections.Num() > 0)
	{
		SetExpandedCollections(NewExpandedCollections);
	}
}

void SCollectionView::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bQueueCollectionItemsUpdate)
	{
		bQueueCollectionItemsUpdate = false;
		UpdateCollectionItems();
	}

	if (bQueueSCCRefresh && FCollectionManagerModule::IsModuleAvailable() && ISourceControlModule::Get().IsEnabled())
	{
		bQueueSCCRefresh = false;

		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

		TArray<FString> CollectionFilesToRefresh;
		for (const auto& AvailableCollectionInfo : AvailableCollections)
		{
			FCollectionStatusInfo StatusInfo;
			if (CollectionManagerModule.Get().GetCollectionStatusInfo(AvailableCollectionInfo.Value->CollectionName, AvailableCollectionInfo.Value->CollectionType, StatusInfo))
			{
				if (StatusInfo.bUseSCC && StatusInfo.SCCState.IsValid() && StatusInfo.SCCState->IsSourceControlled())
				{
					CollectionFilesToRefresh.Add(StatusInfo.SCCState->GetFilename());
				}
			}
		}

		if (CollectionFilesToRefresh.Num() > 0)
		{
			ISourceControlModule::Get().QueueStatusUpdate(CollectionFilesToRefresh);
		}
	}
}

FReply SCollectionView::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if( Commands->ProcessCommandBindings( InKeyEvent ) )
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SCollectionView::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	ValidateDragDropOnCollectionTree(MyGeometry, DragDropEvent, bDraggedOver); // updates bDraggedOver
}

void SCollectionView::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	bDraggedOver = false;
}

FReply SCollectionView::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	ValidateDragDropOnCollectionTree(MyGeometry, DragDropEvent, bDraggedOver); // updates bDraggedOver
	return (bDraggedOver) ? FReply::Handled() : FReply::Unhandled();
}

FReply SCollectionView::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	if (ValidateDragDropOnCollectionTree(MyGeometry, DragDropEvent, bDraggedOver)) // updates bDraggedOver
	{
		bDraggedOver = false;
		return HandleDragDropOnCollectionTree(MyGeometry, DragDropEvent);
	}

	if (bDraggedOver)
	{
		// We were able to handle this operation, but could not due to another error - still report this drop as handled so it doesn't fall through to other widgets
		bDraggedOver = false;
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SCollectionView::MakeSaveDynamicCollectionMenu(FText InQueryString)
{
	// Get all menu extenders for this context menu from the content browser module
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	TArray<FContentBrowserMenuExtender> MenuExtenderDelegates = ContentBrowserModule.GetAllCollectionViewContextMenuExtenders();

	TArray<TSharedPtr<FExtender>> Extenders;
	for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
	{
		if (MenuExtenderDelegates[i].IsBound())
		{
			Extenders.Add(MenuExtenderDelegates[i].Execute());
		}
	}
	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, NULL, MenuExtender, true);

	CollectionContextMenu->UpdateProjectSourceControl();

	CollectionContextMenu->MakeSaveDynamicCollectionSubMenu(MenuBuilder, InQueryString);

	FWidgetPath WidgetPath;
	if (FSlateApplication::Get().GeneratePathToWidgetUnchecked(AsShared(), WidgetPath, EVisibility::All)) // since the collection window can be hidden, we need to manually search the path with a EVisibility::All instead of the default EVisibility::Visible
	{
		FSlateApplication::Get().PushMenu(
			AsShared(),
			WidgetPath,
			MenuBuilder.MakeWidget(),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::TopMenu)
		);
	}
}

bool SCollectionView::ShouldAllowSelectionChangedDelegate() const
{
	return PreventSelectionChangedDelegateCount == 0;
}

FReply SCollectionView::MakeAddCollectionMenu()
{
	// Get all menu extenders for this context menu from the content browser module
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	TArray<FContentBrowserMenuExtender> MenuExtenderDelegates = ContentBrowserModule.GetAllCollectionViewContextMenuExtenders();

	TArray<TSharedPtr<FExtender>> Extenders;
	for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
	{
		if (MenuExtenderDelegates[i].IsBound())
		{
			Extenders.Add(MenuExtenderDelegates[i].Execute());
		}
	}
	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, NULL, MenuExtender, true);

	CollectionContextMenu->UpdateProjectSourceControl();

	CollectionContextMenu->MakeNewCollectionSubMenu(MenuBuilder, ECollectionStorageMode::Static, SCollectionView::FCreateCollectionPayload());

	FSlateApplication::Get().PushMenu(
		AsShared(),
		FWidgetPath(),
		MenuBuilder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect( FPopupTransitionEffect::TopMenu )
		);

	return FReply::Handled();
}

EVisibility SCollectionView::GetCollectionsTitleTextVisibility() const
{
	// Only show the title text if we have an expansion area, but are collapsed
	return (CollectionsExpandableAreaPtr.IsValid() && !CollectionsExpandableAreaPtr->IsExpanded()) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SCollectionView::GetCollectionsSearchBoxVisibility() const
{
	// Only show the search box if we have an expanded expansion area, or aren't currently using an expansion area
	return (!CollectionsExpandableAreaPtr.IsValid() || CollectionsExpandableAreaPtr->IsExpanded()) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SCollectionView::GetAddCollectionButtonVisibility() const
{
	return (bAllowCollectionButtons && ( !CollectionsExpandableAreaPtr.IsValid() || CollectionsExpandableAreaPtr->IsExpanded() ) ) ? EVisibility::Visible : EVisibility::Collapsed;
}

void SCollectionView::CreateCollectionItem( ECollectionShareType::Type CollectionType, ECollectionStorageMode::Type StorageMode, const FCreateCollectionPayload& InCreationPayload )
{
	if ( ensure(CollectionType != ECollectionShareType::CST_All) )
	{
		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

		const FName BaseCollectionName = *LOCTEXT("NewCollectionName", "NewCollection").ToString();
		FName CollectionName;
		CollectionManagerModule.Get().CreateUniqueCollectionName(BaseCollectionName, CollectionType, CollectionName);
		TSharedPtr<FCollectionItem> NewItem = MakeShareable(new FCollectionItem(CollectionName, CollectionType));
		NewItem->StorageMode = StorageMode;

		// Adding a new collection now, so clear any filter we may have applied
		SearchBoxPtr->SetText(FText::GetEmpty());

		if ( InCreationPayload.ParentCollection.IsSet() )
		{
			TSharedPtr<FCollectionItem> ParentCollectionItem = AvailableCollections.FindRef(InCreationPayload.ParentCollection.GetValue());
			if ( ParentCollectionItem.IsValid() )
			{
				ParentCollectionItem->ChildCollections.Add(NewItem);
				NewItem->ParentCollection = ParentCollectionItem;

				// Make sure the parent is expanded so we can see its newly added child item
				CollectionTreePtr->SetItemExpansion(ParentCollectionItem, true);
			}
		}

		// Mark the new collection for rename and that it is new so it will be created upon successful rename
		NewItem->bRenaming = true;
		NewItem->bNewCollection = true;
		NewItem->OnCollectionCreatedEvent = InCreationPayload.OnCollectionCreatedEvent;

		AvailableCollections.Add( FCollectionNameType(NewItem->CollectionName, NewItem->CollectionType), NewItem );
		UpdateFilteredCollectionItems();
		CollectionTreePtr->RequestScrollIntoView(NewItem);
		CollectionTreePtr->SetSelection( NewItem );
	}
}

void SCollectionView::RenameCollectionItem( const TSharedPtr<FCollectionItem>& ItemToRename )
{
	if ( ensure(ItemToRename.IsValid()) )
	{
		ItemToRename->bRenaming = true;
		CollectionTreePtr->RequestScrollIntoView(ItemToRename);
	}
}

void SCollectionView::DeleteCollectionItems( const TArray<TSharedPtr<FCollectionItem>>& ItemsToDelete )
{
	if (ItemsToDelete.Num() == 0)
	{
		return;
	}

	// Before we delete anything (as this will trigger a tree update) we need to work out what our new selection should be in the case that 
	// all of the selected items are removed
	const TArray<TSharedPtr<FCollectionItem>> PreviouslySelectedItems = CollectionTreePtr->GetSelectedItems();

	// Get the first selected item that will be deleted so we can find a suitable new selection
	TSharedPtr<FCollectionItem> FirstSelectedItemDeleted;
	for (const auto& ItemToDelete : ItemsToDelete)
	{
		if (PreviouslySelectedItems.Contains(ItemToDelete))
		{
			FirstSelectedItemDeleted = ItemToDelete;
			break;
		}
	}

	// Build up an array of potential new selections (in the case that we're deleting everything that's selected)
	// Earlier items should be considered first, we base this list on the first selected item that will be deleted, and include previous siblings, and then all parents and roots
	TArray<FCollectionNameType> PotentialNewSelections;
	if (FirstSelectedItemDeleted.IsValid())
	{
		TSharedPtr<FCollectionItem> RootSelectedItemDeleted = FirstSelectedItemDeleted;
		TSharedPtr<FCollectionItem> ParentCollectionItem = FirstSelectedItemDeleted->ParentCollection.Pin();

		if (ParentCollectionItem.IsValid())
		{
			// Add all the siblings until we find the item that will be deleted
			for (const auto& ChildItemWeakPtr : ParentCollectionItem->ChildCollections)
			{
				TSharedPtr<FCollectionItem> ChildItem = ChildItemWeakPtr.Pin();
				if (ChildItem.IsValid())
				{
					if (ChildItem == FirstSelectedItemDeleted)
					{
						break;
					}
					
					// We add siblings as the start, as the closest sibling should be the first match
					PotentialNewSelections.Insert(FCollectionNameType(ChildItem->CollectionName, ChildItem->CollectionType), 0);
				}
			}

			// Now add this parent, and all other parents too
			do
			{
				PotentialNewSelections.Add(FCollectionNameType(ParentCollectionItem->CollectionName, ParentCollectionItem->CollectionType));
				RootSelectedItemDeleted = ParentCollectionItem;
				ParentCollectionItem = ParentCollectionItem->ParentCollection.Pin();
			}
			while (ParentCollectionItem.IsValid());
		}

		if (RootSelectedItemDeleted.IsValid())
		{
			// Add all the root level items before this one
			const int32 InsertionPoint = PotentialNewSelections.Num();
			for (const auto& RootItem : VisibleRootCollectionItems)
			{
				if (RootItem == RootSelectedItemDeleted)
				{
					break;
				}
				
				// Add each root item at the insertion point, as the closest item should be a better match
				PotentialNewSelections.Insert(FCollectionNameType(RootItem->CollectionName, RootItem->CollectionType), InsertionPoint);
			}
		}
	}

	// Delete all given collections
	int32 NumSelectedItemsDeleted = 0;
	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
	for (const TSharedPtr<FCollectionItem>& ItemToDelete : ItemsToDelete)
	{
		if (CollectionManagerModule.Get().DestroyCollection(ItemToDelete->CollectionName, ItemToDelete->CollectionType))
		{
			if (PreviouslySelectedItems.Contains(ItemToDelete))
			{
				++NumSelectedItemsDeleted;
			}
		}
		else
		{
			// Display a warning
			const FVector2D& CursorPos = FSlateApplication::Get().GetCursorPos();
			FSlateRect MessageAnchor(CursorPos.X, CursorPos.Y, CursorPos.X, CursorPos.Y);
			ContentBrowserUtils::DisplayMessage(
				FText::Format( LOCTEXT("CollectionDestroyFailed", "Failed to destroy collection. {0}"), CollectionManagerModule.Get().GetLastError() ),
				MessageAnchor,
				CollectionTreePtr.ToSharedRef()
				);
		}
	}

	// DestroyCollection will have triggered a notification that will have updated the tree, we now need to apply a suitable selection...

	// Did this delete change the list of selected items?
	if (NumSelectedItemsDeleted > 0 || PreviouslySelectedItems.Num() == 0)
	{
		// If we removed everything that was selected, we need to try and find a suitable replacement...
		if (NumSelectedItemsDeleted >= PreviouslySelectedItems.Num() && VisibleCollections.Num() > 1)
		{
			// Include the first visible item as an absolute last resort should everything else suitable have been removed from the tree
			PotentialNewSelections.Add(*VisibleCollections.CreateConstIterator());

			// Check the potential new selections array and try and select the first one that's still visible in the tree
			TArray<FCollectionNameType> NewItemSelection;
			for (const FCollectionNameType& PotentialNewSelection : PotentialNewSelections)
			{
				if (VisibleCollections.Contains(PotentialNewSelection))
				{
					NewItemSelection.Add(PotentialNewSelection);
					break;
				}
			}

			SetSelectedCollections(NewItemSelection, true);
		}

		// Broadcast the new selection
		const TArray<TSharedPtr<FCollectionItem>> UpdatedSelectedItems = CollectionTreePtr->GetSelectedItems();
		CollectionSelectionChanged((UpdatedSelectedItems.Num() > 0) ? UpdatedSelectedItems[0] : nullptr, ESelectInfo::Direct);
	}
}

EVisibility SCollectionView::GetCollectionTreeVisibility() const
{
	return AvailableCollections.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

const FSlateBrush* SCollectionView::GetCollectionViewDropTargetBorder() const
{
	return bDraggedOver ? FEditorStyle::GetBrush("ContentBrowser.CollectionTreeDragDropBorder") : FEditorStyle::GetBrush("NoBorder");
}

TSharedRef<ITableRow> SCollectionView::GenerateCollectionRow( TSharedPtr<FCollectionItem> CollectionItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	check(CollectionItem.IsValid());

	// Only bind the check box callbacks if we're allowed to show check boxes
	TAttribute<bool> IsCollectionCheckBoxEnabledAttribute;
	TAttribute<ECheckBoxState> IsCollectionCheckedAttribute;
	FOnCheckStateChanged OnCollectionCheckStateChangedDelegate;
	if ( QuickAssetManagement.IsValid() )
	{
		// Can only manage assets for static collections
		if (CollectionItem->StorageMode == ECollectionStorageMode::Static)
		{
			IsCollectionCheckBoxEnabledAttribute.Bind(TAttribute<bool>::FGetter::CreateSP(this, &SCollectionView::IsCollectionCheckBoxEnabled, CollectionItem));
			IsCollectionCheckedAttribute.Bind(TAttribute<ECheckBoxState>::FGetter::CreateSP(this, &SCollectionView::IsCollectionChecked, CollectionItem));
			OnCollectionCheckStateChangedDelegate.BindSP(this, &SCollectionView::OnCollectionCheckStateChanged, CollectionItem);
		}
		else
		{
			IsCollectionCheckBoxEnabledAttribute.Bind(TAttribute<bool>::FGetter::CreateLambda([]{ return false; }));
			IsCollectionCheckedAttribute.Bind(TAttribute<ECheckBoxState>::FGetter::CreateLambda([]{ return ECheckBoxState::Unchecked; }));
		}
	}

	TSharedPtr< STableRow< TSharedPtr<FCollectionItem> > > TableRow = SNew( STableRow< TSharedPtr<FCollectionItem> >, OwnerTable )
		.OnDragDetected(this, &SCollectionView::OnCollectionDragDetected);

	TableRow->SetContent
		(
			SNew(SCollectionTreeItem)
			.ParentWidget(SharedThis(this))
			.CollectionItem(CollectionItem)
			.OnNameChangeCommit(this, &SCollectionView::CollectionNameChangeCommit)
			.OnVerifyRenameCommit(this, &SCollectionView::CollectionVerifyRenameCommit)
			.OnValidateDragDrop(this, &SCollectionView::ValidateDragDropOnCollectionItem)
			.OnHandleDragDrop(this, &SCollectionView::HandleDragDropOnCollectionItem)
			.IsSelected(TableRow.Get(), &STableRow< TSharedPtr<FCollectionItem> >::IsSelectedExclusively)
			.IsReadOnly(this, &SCollectionView::IsCollectionNameReadOnly)
			.HighlightText(this, &SCollectionView::GetCollectionsSearchFilterText)
			.IsCheckBoxEnabled(IsCollectionCheckBoxEnabledAttribute)
			.IsCollectionChecked(IsCollectionCheckedAttribute)
			.OnCollectionCheckStateChanged(OnCollectionCheckStateChangedDelegate)
		);

	return TableRow.ToSharedRef();
}

void SCollectionView::GetCollectionItemChildren( TSharedPtr<FCollectionItem> InParentItem, TArray< TSharedPtr<FCollectionItem> >& OutChildItems ) const
{
	for (const auto& ChildItemWeakPtr : InParentItem->ChildCollections)
	{
		TSharedPtr<FCollectionItem> ChildItem = ChildItemWeakPtr.Pin();
		if (ChildItem.IsValid() && VisibleCollections.Contains(FCollectionNameType(ChildItem->CollectionName, ChildItem->CollectionType)))
		{
			OutChildItems.Add(ChildItem);
		}
	}
	OutChildItems.Sort(FCollectionItem::FCompareFCollectionItemByName());
}

FReply SCollectionView::OnCollectionDragDetected(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if (bAllowCollectionDrag && MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		const TArray<FCollectionNameType> SelectedCollections = GetSelectedCollections();
		if (SelectedCollections.Num() > 0)
		{
			TSharedRef<FCollectionDragDropOp> DragDropOp = FCollectionDragDropOp::New(SelectedCollections);
			CurrentCollectionDragDropOp = DragDropOp;
			return FReply::Handled().BeginDragDrop(DragDropOp);
		}
	}

	return FReply::Unhandled();
}

bool SCollectionView::ValidateDragDropOnCollectionTree(const FGeometry& Geometry, const FDragDropEvent& DragDropEvent, bool& OutIsKnownDragOperation)
{
	OutIsKnownDragOperation = false;

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (!Operation.IsValid())
	{
		return false;
	}

	if (Operation->IsOfType<FCollectionDragDropOp>())
	{
		OutIsKnownDragOperation = true;
		return true;
	}

	return false;
}

FReply SCollectionView::HandleDragDropOnCollectionTree(const FGeometry& Geometry, const FDragDropEvent& DragDropEvent)
{
	// Should have already called ValidateDragDropOnCollectionTree prior to calling this...
	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	check(Operation.IsValid());

	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

	if (Operation->IsOfType<FCollectionDragDropOp>())
	{
		TSharedPtr<FCollectionDragDropOp> DragDropOp = StaticCastSharedPtr<FCollectionDragDropOp>(Operation);
		
		// Reparent all of the collections in the drag drop so that they are root level items
		for (const FCollectionNameType& NewChildCollection : DragDropOp->Collections)
		{
			if (!CollectionManagerModule.Get().ReparentCollection(
					NewChildCollection.Name, NewChildCollection.Type,
					NAME_None, ECollectionShareType::CST_All
					))
			{
				ContentBrowserUtils::DisplayMessage(CollectionManagerModule.Get().GetLastError(), Geometry.GetLayoutBoundingRect(), SharedThis(this));
			}
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

bool SCollectionView::ValidateDragDropOnCollectionItem(TSharedRef<FCollectionItem> CollectionItem, const FGeometry& Geometry, const FDragDropEvent& DragDropEvent, bool& OutIsKnownDragOperation)
{
	OutIsKnownDragOperation = false;

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (!Operation.IsValid())
	{
		return false;
	}

	bool bIsValidDrag = false;
	TOptional<EMouseCursor::Type> NewDragCursor;

	if (Operation->IsOfType<FCollectionDragDropOp>())
	{
		TSharedPtr<FCollectionDragDropOp> DragDropOp = StaticCastSharedPtr<FCollectionDragDropOp>(Operation);

		OutIsKnownDragOperation = true;

		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
		
		bIsValidDrag = true;
		for (const FCollectionNameType& PotentialChildCollection : DragDropOp->Collections)
		{
			bIsValidDrag = CollectionManagerModule.Get().IsValidParentCollection(
				PotentialChildCollection.Name, PotentialChildCollection.Type,
				CollectionItem->CollectionName, CollectionItem->CollectionType
				);

			if (!bIsValidDrag)
			{
				DragDropOp->SetToolTip(CollectionManagerModule.Get().GetLastError(), FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error")));
				break;
			}
		}

		// If we are dragging over a child collection item, then this view as a whole should not be marked as dragged over
		bDraggedOver = false;
	}
	else if (Operation->IsOfType<FAssetDragDropOp>())
	{
		TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>(Operation);
		OutIsKnownDragOperation = true;
		bIsValidDrag = DragDropOp->HasAssets();
	}

	// Set the default slashed circle if this drag is invalid and a drag operation hasn't set NewDragCursor to something custom
	if (!bIsValidDrag && !NewDragCursor.IsSet())
	{
		NewDragCursor = EMouseCursor::SlashedCircle;
	}
	Operation->SetCursorOverride(NewDragCursor);

	return bIsValidDrag;
}

FReply SCollectionView::HandleDragDropOnCollectionItem(TSharedRef<FCollectionItem> CollectionItem, const FGeometry& Geometry, const FDragDropEvent& DragDropEvent)
{
	// Should have already called ValidateDragDropOnCollectionItem prior to calling this...
	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	check(Operation.IsValid());

	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

	if (Operation->IsOfType<FCollectionDragDropOp>())
	{
		TSharedPtr<FCollectionDragDropOp> DragDropOp = StaticCastSharedPtr<FCollectionDragDropOp>(Operation);
		
		// Make sure our drop item is marked as expanded so that we'll be able to see the newly added children
		CollectionTreePtr->SetItemExpansion(CollectionItem, true);

		// Reparent all of the collections in the drag drop so that they are our immediate children
		for (const FCollectionNameType& NewChildCollection : DragDropOp->Collections)
		{
			if (!CollectionManagerModule.Get().ReparentCollection(
					NewChildCollection.Name, NewChildCollection.Type,
					CollectionItem->CollectionName, CollectionItem->CollectionType
					))
			{
				ContentBrowserUtils::DisplayMessage(CollectionManagerModule.Get().GetLastError(), Geometry.GetLayoutBoundingRect(), SharedThis(this));
			}
		}

		return FReply::Handled();
	}
	else if (Operation->IsOfType<FAssetDragDropOp>())
	{
		TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>(Operation);
		const TArray<FAssetData>& DroppedAssets = DragDropOp->GetAssets();

		TArray<FName> ObjectPaths;
		ObjectPaths.Reserve(DroppedAssets.Num());
		for (const FAssetData& AssetData : DroppedAssets)
		{
			ObjectPaths.Add(AssetData.ObjectPath);
		}

		int32 NumAdded = 0;
		FText Message;
		if (CollectionManagerModule.Get().AddToCollection(CollectionItem->CollectionName, CollectionItem->CollectionType, ObjectPaths, &NumAdded))
		{
			if (DroppedAssets.Num() == 1)
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("AssetName"), FText::FromName(DroppedAssets[0].AssetName));
				Args.Add(TEXT("CollectionName"), FText::FromName(CollectionItem->CollectionName));
				Message = FText::Format(LOCTEXT("CollectionAssetAdded", "Added {AssetName} to {CollectionName}"), Args);
			}
			else
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("Number"), NumAdded);
				Args.Add(TEXT("CollectionName"), FText::FromName(CollectionItem->CollectionName));
				Message = FText::Format(LOCTEXT("CollectionAssetsAdded", "Added {Number} asset(s) to {CollectionName}"), Args);
			}
		}
		else
		{
			Message = CollectionManagerModule.Get().GetLastError();
		}

		// Added items to the collection or failed. Either way, display the message.
		ContentBrowserUtils::DisplayMessage(Message, Geometry.GetLayoutBoundingRect(), SharedThis(this));

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SCollectionView::ExpandParentItems(const TSharedRef<FCollectionItem>& InCollectionItem)
{
	for (TSharedPtr<FCollectionItem> CollectionItemToExpand = InCollectionItem->ParentCollection.Pin(); 
		CollectionItemToExpand.IsValid(); 
		CollectionItemToExpand = CollectionItemToExpand->ParentCollection.Pin()
		)
	{
		CollectionTreePtr->SetItemExpansion(CollectionItemToExpand, true);
	}
}

TSharedPtr<SWidget> SCollectionView::MakeCollectionTreeContextMenu()
{
	if ( !bAllowRightClickMenu )
	{
		return NULL;
	}

	return CollectionContextMenu->MakeCollectionTreeContextMenu(Commands);
}

bool SCollectionView::IsCollectionCheckBoxEnabled( TSharedPtr<FCollectionItem> CollectionItem ) const
{
	return QuickAssetManagement.IsValid() && QuickAssetManagement->IsCollectionEnabled(FCollectionNameType(CollectionItem->CollectionName, CollectionItem->CollectionType));
}

ECheckBoxState SCollectionView::IsCollectionChecked( TSharedPtr<FCollectionItem> CollectionItem ) const
{
	if ( QuickAssetManagement.IsValid() )
	{
		return QuickAssetManagement->GetCollectionCheckState(FCollectionNameType(CollectionItem->CollectionName, CollectionItem->CollectionType));
	}
	return ECheckBoxState::Unchecked;
}

void SCollectionView::OnCollectionCheckStateChanged( ECheckBoxState NewState, TSharedPtr<FCollectionItem> CollectionItem )
{
	if ( QuickAssetManagement.IsValid() )
	{
		switch(NewState)
		{
		case ECheckBoxState::Checked:
			QuickAssetManagement->AddCurrentAssetsToCollection(FCollectionNameType(CollectionItem->CollectionName, CollectionItem->CollectionType));
			break;

		case ECheckBoxState::Unchecked:
			QuickAssetManagement->RemoveCurrentAssetsFromCollection(FCollectionNameType(CollectionItem->CollectionName, CollectionItem->CollectionType));
			break;

		default:
			break;
		}
	}
}

void SCollectionView::CollectionSelectionChanged( TSharedPtr< FCollectionItem > CollectionItem, ESelectInfo::Type /*SelectInfo*/ )
{
	if ( ShouldAllowSelectionChangedDelegate() && OnCollectionSelected.IsBound() )
	{
		if ( CollectionItem.IsValid() )
		{
			OnCollectionSelected.Execute(FCollectionNameType(CollectionItem->CollectionName, CollectionItem->CollectionType));
		}
		else
		{
			OnCollectionSelected.Execute(FCollectionNameType(NAME_None, ECollectionShareType::CST_All));
		}
	}
}

void SCollectionView::CollectionItemScrolledIntoView( TSharedPtr<FCollectionItem> CollectionItem, const TSharedPtr<ITableRow>& Widget )
{
	if ( CollectionItem->bRenaming && Widget.IsValid() && Widget->GetContent().IsValid() )
	{
		CollectionItem->OnRenamedRequestEvent.Broadcast();
	}
}

bool SCollectionView::IsCollectionNameReadOnly() const
{
	// We can't rename collections while they're being dragged
	TSharedPtr<FCollectionDragDropOp> DragDropOp = CurrentCollectionDragDropOp.Pin();
	if (DragDropOp.IsValid())
	{
		TArray<TSharedPtr<FCollectionItem>> SelectedCollectionItems = CollectionTreePtr->GetSelectedItems();
		for (const auto& SelectedCollectionItem : SelectedCollectionItems)
		{
			if (DragDropOp->Collections.Contains(FCollectionNameType(SelectedCollectionItem->CollectionName, SelectedCollectionItem->CollectionType)))
			{
				return true;
			}
		}
	}

	CollectionContextMenu->UpdateProjectSourceControl();
	return !CollectionContextMenu->CanRenameSelectedCollections();
}

bool SCollectionView::CollectionNameChangeCommit( const TSharedPtr< FCollectionItem >& CollectionItem, const FString& NewName, bool bChangeConfirmed, FText& OutWarningMessage )
{
	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

	// If new name is empty, set it back to the original name
	const FName NewNameFinal( NewName.IsEmpty() ? CollectionItem->CollectionName : FName(*NewName) );

	if ( CollectionItem->bNewCollection )
	{
		CollectionItem->bNewCollection = false;
		
		// Cache this here as CreateCollection will invalidate the current parent pointer
		TOptional<FCollectionNameType> NewCollectionParentKey;
		TSharedPtr<FCollectionItem> ParentCollectionItem = CollectionItem->ParentCollection.Pin();
		if ( ParentCollectionItem.IsValid() )
		{
			NewCollectionParentKey = FCollectionNameType(ParentCollectionItem->CollectionName, ParentCollectionItem->CollectionType);
		}

		// If we canceled the name change when creating a new asset, we want to silently remove it
		if ( !bChangeConfirmed )
		{
			AvailableCollections.Remove(FCollectionNameType(CollectionItem->CollectionName, CollectionItem->CollectionType));
			UpdateFilteredCollectionItems();
			return false;
		}

		if ( !CollectionManagerModule.Get().CreateCollection(NewNameFinal, CollectionItem->CollectionType, CollectionItem->StorageMode) )
		{
			// Failed to add the collection, remove it from the list
			AvailableCollections.Remove(FCollectionNameType(CollectionItem->CollectionName, CollectionItem->CollectionType));
			UpdateFilteredCollectionItems();

			OutWarningMessage = FText::Format( LOCTEXT("CreateCollectionFailed", "Failed to create the collection. {0}"), CollectionManagerModule.Get().GetLastError());
			return false;
		}

		// Since we're really adding a new collection (as our placeholder item is currently transient), we don't get a rename event from the collections manager
		// We'll spoof one here that so that our placeholder tree item is updated with the final name - this will preserve its expansion and selection state
		HandleCollectionRenamed(
			FCollectionNameType(CollectionItem->CollectionName, CollectionItem->CollectionType), 
			FCollectionNameType(NewNameFinal, CollectionItem->CollectionType)
			);

		if ( NewCollectionParentKey.IsSet() )
		{
			// Try and set the parent correctly (if this fails for any reason, the collection will still be added, but will just appear at the root)
			CollectionManagerModule.Get().ReparentCollection(NewNameFinal, CollectionItem->CollectionType, NewCollectionParentKey->Name, NewCollectionParentKey->Type);
		}

		// Notify anything that cares that this collection has been created now
		if ( CollectionItem->OnCollectionCreatedEvent.IsBound())
		{
			CollectionItem->OnCollectionCreatedEvent.Execute(FCollectionNameType(NewNameFinal, CollectionItem->CollectionType));
			CollectionItem->OnCollectionCreatedEvent.Unbind();
		}
	}
	else
	{
		// If the old name is the same as the new name, just early exit here.
		if ( CollectionItem->CollectionName == NewNameFinal )
		{
			return true;
		}

		// If the new name doesn't pass our current filter, we need to clear it
		if ( !CollectionItemTextFilter->PassesFilter( FCollectionItem(NewNameFinal, CollectionItem->CollectionType) ) )
		{
			SearchBoxPtr->SetText(FText::GetEmpty());
		}

		// Otherwise perform the rename
		if ( !CollectionManagerModule.Get().RenameCollection(CollectionItem->CollectionName, CollectionItem->CollectionType, NewNameFinal, CollectionItem->CollectionType) )
		{
			// Failed to rename the collection
			OutWarningMessage = FText::Format( LOCTEXT("RenameCollectionFailed", "Failed to rename the collection. {0}"), CollectionManagerModule.Get().GetLastError());
			return false;
		}
	}

	// At this point CollectionItem is no longer a member of the CollectionItems list (as the list is repopulated by
	// UpdateCollectionItems, which is called by a broadcast from CollectionManagerModule::RenameCollection, above).
	// So search again for the item by name and type.
	auto NewCollectionItemPtr = AvailableCollections.Find( FCollectionNameType(NewNameFinal, CollectionItem->CollectionType) );

	// Reselect the path to notify that the selection has changed
	{
		FScopedPreventSelectionChangedDelegate DelegatePrevention( SharedThis(this) );
		CollectionTreePtr->ClearSelection();
	}

	// Set the selection
	if (NewCollectionItemPtr)
	{
		const auto& NewCollectionItem = *NewCollectionItemPtr;
		CollectionTreePtr->RequestScrollIntoView(NewCollectionItem);
		CollectionTreePtr->SetItemSelection(NewCollectionItem, true);
	}

	return true;
}

bool SCollectionView::CollectionVerifyRenameCommit(const TSharedPtr< FCollectionItem >& CollectionItem, const FString& NewName, const FSlateRect& MessageAnchor, FText& OutErrorMessage)
{
	// If the new name is the same as the old name, consider this to be unchanged, and accept it.
	if (CollectionItem->CollectionName.ToString() == NewName)
	{
		return true;
	}

	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

	if (!CollectionManagerModule.Get().IsValidCollectionName(NewName, ECollectionShareType::CST_Shared))
	{
		OutErrorMessage = CollectionManagerModule.Get().GetLastError();
		return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "ActorPlacementInfo.h"
#include "IPlacementModeModule.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Misc/TextFilter.h"

/**
 * A tile representation of the class or the asset.  These are embedded into the views inside
 * of each tab.
 */
class SPlacementAssetEntry : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPlacementAssetEntry){}

		/** Highlight this text in the text block */
		SLATE_ATTRIBUTE(FText, HighlightText)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedPtr<const FPlaceableItem>& InItem);

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	bool IsPressed() const;

	TSharedPtr<const FPlaceableItem> Item;

private:
	const FSlateBrush* GetBorder() const;

	bool bIsPressed;

	/** Brush resource that represents a button */
	const FSlateBrush* NormalImage;
	/** Brush resource that represents a button when it is hovered */
	const FSlateBrush* HoverImage;
	/** Brush resource that represents a button when it is pressed */
	const FSlateBrush* PressedImage;
};

class SPlacementModeTools : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SPlacementModeTools ){}
	SLATE_END_ARGS();

	void Construct( const FArguments& InArgs );

	virtual ~SPlacementModeTools();

private:

	// Begin SWidget
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	// End SWidget

private:

	/** Creates a tab widget to show on the left that when clicked sets the currently active tab */
	TSharedRef< SWidget > CreatePlacementGroupTab( const FPlacementCategoryInfo& Info );

	/** Generates a widget for the specified item */
	TSharedRef<ITableRow> OnGenerateWidgetForItem(TSharedPtr<FPlaceableItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Get the identifier of the currently active tab */
	FName GetActiveTab() const;

	/** Check if a search is active */
	bool IsSearchActive() const;

	/** Update the list of filtered items */
	void UpdateFilteredItems();

	/** Called when the recently placed assets changes. */
	void UpdateRecentlyPlacedAssets( const TArray< FActorPlacementInfo >& RecentlyPlaced );

	/** Called when the list of placeable assets changes. */
	void UpdatePlaceableAssets();

private:

	/** Gets the border image for the tab, this is the 'active' orange bar. */
	const FSlateBrush* PlacementGroupBorderImage( FName CategoryName ) const;

	/** When the tab is clicked we adjust the check state, so that the right style is displayed. */
	void OnPlacementTabChanged( ECheckBoxState NewState, FName CategoryName );

	/** Gets the tab 'active' state, so that we can show the active style */
	ECheckBoxState GetPlacementTabCheckedState( FName CategoryName ) const;

private:

	/** Gets the visibility for the failed search text */
	EVisibility GetFailedSearchVisibility() const;

	/** Gets the visibility for the list view */
	EVisibility GetListViewVisibility() const;

	/** Gets the visibility for tabs */
	EVisibility GetTabsVisibility() const;

private:

	/** Called when the search text changes */
	void OnSearchChanged(const FText& InFilterText);
	void OnSearchCommitted(const FText& InFilterText, ETextCommit::Type InCommitType);

	/** Get the text that should be highlighted on any items */
	FText GetHighlightText() const;

private:
	/** Flags to invoke updates of particular caregories on tick */
	bool bRecentlyPlacedRefreshRequested;
	bool bPlaceablesFullRefreshRequested;

	/** Flag to indicate that we need to update the filtered items array */
	bool bNeedsUpdate;

	// The text filter used to filter the classes
	typedef TTextFilter<const FPlaceableItem&> FPlacementAssetEntryTextFilter;
	TSharedPtr<FPlacementAssetEntryTextFilter> SearchTextFilter;

	/** Custom content slot, where a category has a custom generator */
	TSharedPtr<SBox> CustomContent;
	
	/** Content container for any data driven content */
	TSharedPtr<SBox> DataDrivenContent;

	/* The search box used to update the filter text */
	TSharedPtr<SSearchBox> SearchBoxPtr;

	/** Array of filtered items to show in the list view */
	TArray<TSharedPtr<FPlaceableItem>> FilteredItems;

	/** The name of the currently active tab (where no search is active) */
	FName ActiveTabName;

	/** List view that shows placeable items */
	TSharedPtr<SListView<TSharedPtr<FPlaceableItem>>> ListView;
};

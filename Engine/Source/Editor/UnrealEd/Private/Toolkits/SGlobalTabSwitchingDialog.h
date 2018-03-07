// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Framework/Commands/InputChord.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "AssetThumbnail.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"

class FTabSwitchingListItemBase;
template <typename ItemType> class SListView;

//////////////////////////////////////////////////////////////////////////
// SGlobalTabSwitchingDialog

class SGlobalTabSwitchingDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGlobalTabSwitchingDialog){}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, FVector2D InSize, FInputChord InTriggerChord);

	~SGlobalTabSwitchingDialog();

	// SWidget interface
	virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	// End of SWidget interface

	// Is an instance already open?
	static bool IsAlreadyOpen()
	{
		return bIsAlreadyOpen;
	}

private:
	typedef TSharedPtr<class FTabSwitchingListItemBase> FTabListItemPtr;
	typedef SListView<FTabListItemPtr> STabListWidget;

private:
	TSharedRef<ITableRow> OnGenerateTabSwitchListItemWidget(FTabListItemPtr InItem, const TSharedRef<STableViewBase>& OwnerTable);

	void CycleSelection(bool bForwards);
	void OnMainTabListSelectionChanged(FTabListItemPtr InItem, ESelectInfo::Type SelectInfo);
	void OnMainTabListItemClicked(FTabListItemPtr InItem);
	void DismissDialog();
	FReply OnBrowseToSelectedAsset();

	FTabListItemPtr GetMainTabListSelectedItem() const;

private:
	// The chord that triggered the dialog (so we can handle the correct Tab/`/etc... key repeat, and dismiss on the correct control/command modifier release
	FInputChord TriggerChord;

	// Pool for maintaining and rendering thumbnails
	TSharedPtr<class FAssetThumbnailPool> AssetThumbnailPool;

	// The array of 'document' items
	TArray<FTabListItemPtr> MainTabsListDataSource;

	// The widget representing the list of 'document' items
	TSharedPtr<STabListWidget> MainTabsListWidget;

	// The container widget for the indication of the asset that will be activated when the dialog closes
	TSharedPtr<SBox> NewTabItemToActivateDisplayBox;

	// The container widget for the indication of the path to the asset that will be activated when the dialog closes
	TSharedPtr<SBox> NewTabItemToActivatePathBox;

	// The container widget for the tool tabs list
	TSharedPtr<SBox> ToolTabsListBox;

	static bool bIsAlreadyOpen;
};

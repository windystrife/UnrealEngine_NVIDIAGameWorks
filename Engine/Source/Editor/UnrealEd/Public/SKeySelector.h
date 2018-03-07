// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Misc/Attribute.h"
#include "InputCoreTypes.h"
#include "Types/SlateStructs.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "EditorStyleSet.h"

class FKeyTreeInfo;
class SComboButton;

DECLARE_DELEGATE_OneParam(FOnKeyChanged, TSharedPtr<FKey>)

//////////////////////////////////////////////////////////////////////////
// SKeySelector

typedef TSharedPtr<FKeyTreeInfo> FKeyTreeItem;
typedef STreeView<FKeyTreeItem> SKeyTreeView;

/** Widget for selecting an input key */
class UNREALED_API SKeySelector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SKeySelector )
		: _CurrentKey(FKey())
		, _TreeViewWidth(300.f)
		, _TreeViewHeight(400.f)
		, _Font( FEditorStyle::GetFontStyle( TEXT("NormalFont") ) )
		, _FilterBlueprintBindable( true )
		, _AllowClear( true )
		{}
		SLATE_ATTRIBUTE( TOptional<FKey>, CurrentKey )
		SLATE_ATTRIBUTE( FOptionalSize, TreeViewWidth )
		SLATE_ATTRIBUTE( FOptionalSize, TreeViewHeight )
		SLATE_EVENT( FOnKeyChanged, OnKeyChanged )
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )
		SLATE_ARGUMENT( bool, FilterBlueprintBindable )
		SLATE_ARGUMENT( bool, AllowClear )
	SLATE_END_ARGS()
public:
	void Construct(const FArguments& InArgs);

protected:
	/** Gets the icon for the key being manipulated */
	const FSlateBrush* GetKeyIconImage() const;
	/** Gets a succinct description for the key being manipulated */
	FText GetKeyDescription() const;

	/** Treeview support functions */
	virtual TSharedRef<ITableRow> GenerateKeyTreeRow(FKeyTreeItem InItem, const TSharedRef<STableViewBase>& OwnerTree);
	void OnKeySelectionChanged(FKeyTreeItem Selection, ESelectInfo::Type SelectInfo);
	void GetKeyChildren(FKeyTreeItem InItem, TArray<FKeyTreeItem>& OutChildren);

	/** Gets the Menu Content, setting it up if necessary */
	virtual TSharedRef<SWidget>	GetMenuContent();

	/** Key searching support */
	void OnFilterTextChanged(const FText& NewText);
	void OnFilterTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);
	void GetSearchTokens(const FString& SearchString, TArray<FString>& OutTokens) const;

	/** Helper to generate the filtered list of keys, based on the search string matching */
	bool GetChildrenMatchingSearch(const TArray<FString>& SearchTokens, const TArray<FKeyTreeItem>& UnfilteredList, TArray<FKeyTreeItem>& OutFilteredList);

	/** 
	 * Determine the best icon to represent the given key.
	 *
	 * @param Key		The key to get the icon for.
	 * @param returns a brush that best represents the icon
	 */
	const FSlateBrush* GetIconFromKey(FKey Key) const;

protected:
	/** Combo Button that shows current key and icon */
	TSharedPtr<SComboButton>	KeyComboButton;

	/** Reference to the menu content that's displayed when the key button is clicked on */
	TSharedPtr<SWidget>			MenuContent;
	TSharedPtr<SSearchBox>		FilterTextBox;
	TSharedPtr<SKeyTreeView>	KeyTreeView;
	FText						SearchText;

	/** The key attribute that we're modifying with this widget, or an empty optional if the key contains multiple values */
	TAttribute<TOptional<FKey>>	CurrentKey;

	/** Delegate that is called every time the key changes. */
	FOnKeyChanged				OnKeyChanged;

	/** Desired width of the tree view widget */
	TAttribute<FOptionalSize>	TreeViewWidth;
	/** Desired height of the tree view widget */
	TAttribute<FOptionalSize>	TreeViewHeight;

	/** Font used for category tree entries */
	FSlateFontInfo				CategoryFont;
	/** Font used for key tree entries */
	FSlateFontInfo				KeyFont;

	/** Array containing the unfiltered list of all values this key could possibly have */
	TArray<FKeyTreeItem>		KeyTreeRoot;
	/** Array containing a filtered list, according to the text in the searchbox */
	TArray<FKeyTreeItem>		FilteredKeyTreeRoot;
};

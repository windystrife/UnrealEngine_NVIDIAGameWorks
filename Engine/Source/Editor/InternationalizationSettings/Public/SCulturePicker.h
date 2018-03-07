// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/SlateDelegates.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"

struct FCultureEntry
{
	FCulturePtr Culture;
	TArray< TSharedPtr<FCultureEntry> > Children;
	bool IsSelectable;

	FCultureEntry(const FCulturePtr& InCulture, const bool InIsSelectable = true)
		: Culture(InCulture)
		, IsSelectable(InIsSelectable)
	{}

	FCultureEntry(const FCultureEntry& Source)
		: Culture(Source.Culture)
		, IsSelectable(Source.IsSelectable)
	{
		Children.Reserve(Source.Children.Num());
		for (const auto& Child : Source.Children)
		{
			Children.Add( MakeShareable( new FCultureEntry(*Child) ) );
		}
	}
};

class INTERNATIONALIZATIONSETTINGS_API SCulturePicker : public SCompoundWidget
{
public:
	/** A delegate type invoked when a selection changes somewhere. */
	DECLARE_DELEGATE_RetVal_OneParam(bool, FIsCulturePickable, FCulturePtr);
	typedef TSlateDelegates< FCulturePtr >::FOnSelectionChanged FOnSelectionChanged;

	/** Different display name formats that can be used for a culture */
	enum class ECultureDisplayFormat
	{
		/** Should the culture display the name used by the active culture? */
		ActiveCultureDisplayName,
		/** Should the culture display the name used by the given culture? (ie, localized in their own native culture) */
		NativeCultureDisplayName,
		/** Should the culture display both the active and native cultures name? (will appear as "<ActiveName> (<NativeName>)") */
		ActiveAndNativeCultureDisplayName,
		/** Should the culture display both the native and active cultures name? (will appear as "<NativeName> (<ActiveName>)") */
		NativeAndActiveCultureDisplayName,
	};

public:
	SLATE_BEGIN_ARGS( SCulturePicker )
		: _DisplayNameFormat(ECultureDisplayFormat::ActiveCultureDisplayName)
		, _CanSelectNone(false)
	{}
		SLATE_EVENT( FOnSelectionChanged, OnSelectionChanged )
		SLATE_EVENT( FIsCulturePickable, IsCulturePickable )
		SLATE_ARGUMENT( FCulturePtr, InitialSelection )
		SLATE_ARGUMENT(ECultureDisplayFormat, DisplayNameFormat)
		SLATE_ARGUMENT(bool, CanSelectNone)
	SLATE_END_ARGS()

	SCulturePicker()
	: DisplayNameFormat(ECultureDisplayFormat::ActiveCultureDisplayName)
	, CanSelectNone(false)
	, SuppressSelectionCallback(false)
	{}

	void Construct( const FArguments& InArgs );
	void RequestTreeRefresh();

private:
	void BuildStockEntries();
	void RebuildEntries();

	void OnFilterStringChanged(const FText& InFilterString);
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FCultureEntry> Entry, const TSharedRef<STableViewBase>& Table);
	void OnGetChildren(TSharedPtr<FCultureEntry> Entry, TArray< TSharedPtr<FCultureEntry> >& Children);
	void OnSelectionChanged(TSharedPtr<FCultureEntry> Entry, ESelectInfo::Type SelectInfo);

private:
	FString GetCultureDisplayName(const FCultureRef& Culture, const bool bIsRootItem) const;

	TSharedPtr< STreeView< TSharedPtr<FCultureEntry> > > TreeView;

	/* The top level culture entries for all possible stock cultures. */
	TArray< TSharedPtr<FCultureEntry> > StockEntries;

	/* The top level culture entries to be displayed in the tree view. */
	TArray< TSharedPtr<FCultureEntry> > RootEntries;

	/* The string by which to filter cultures names. */
	FString FilterString;

	/** Delegate to invoke when selection changes. */
	FOnSelectionChanged OnCultureSelectionChanged;

	/** Delegate to invoke to set whether a culture is "pickable". */
	FIsCulturePickable IsCulturePickable;

	/** How should we display culture names? */
	ECultureDisplayFormat DisplayNameFormat;

	/** Should a null culture option be available? */
	bool CanSelectNone;

	/* Flags that the selection callback shouldn't be called when selecting - useful for initial selection, for instance. */
	bool SuppressSelectionCallback;
};

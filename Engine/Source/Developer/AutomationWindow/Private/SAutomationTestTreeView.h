// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/SlateDelegates.h"
#include "Widgets/Views/STableViewBase.h"
#include "Framework/Views/TableViewTypeTraits.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STreeView.h"

/**
 * Implements the automation test tree view.  Same slate interface as STreeView but adds the ability to clear the internal widget cache.
 */
template<typename ItemType>
class SAutomationTestTreeView
	: public STreeView< ItemType >
{
public:
		typedef typename TListTypeTraits< ItemType >::NullableType NullableItemType;

		typedef typename TSlateDelegates< ItemType >::FOnGetChildren FOnGetChildren;
		typedef typename TSlateDelegates< ItemType >::FOnGenerateRow FOnGenerateRow;
		typedef typename TSlateDelegates< ItemType >::FOnSetExpansionRecursive FOnSetExpansionRecursive;
		typedef typename TSlateDelegates< ItemType >::FOnItemScrolledIntoView FOnItemScrolledIntoView;
		typedef typename TSlateDelegates< NullableItemType >::FOnSelectionChanged FOnSelectionChanged;
		typedef typename TSlateDelegates< ItemType >::FOnMouseButtonDoubleClick FOnMouseButtonDoubleClick;
		typedef typename TSlateDelegates< ItemType >::FOnExpansionChanged FOnExpansionChanged;
public:
	
		SLATE_BEGIN_ARGS( SAutomationTestTreeView )
			: _OnGenerateRow()
			, _OnGetChildren()
			, _OnSetExpansionRecursive()
			, _TreeItemsSource( static_cast< TArray<ItemType>* >(NULL) ) //@todo Slate Syntax: Initializing from NULL without a cast
			, _ItemHeight(16)
			, _OnContextMenuOpening()
			, _OnMouseButtonDoubleClick()
			, _OnSelectionChanged()
			, _OnExpansionChanged()
			, _SelectionMode(ESelectionMode::Multi)
			, _ClearSelectionOnClick(true)
			, _ExternalScrollbar()
			{}

			SLATE_EVENT( FOnGenerateRow, OnGenerateRow )

			SLATE_EVENT( FOnTableViewScrolled, OnTreeViewScrolled )

			SLATE_EVENT( FOnItemScrolledIntoView, OnItemScrolledIntoView )

			SLATE_EVENT( FOnGetChildren, OnGetChildren )

			SLATE_EVENT( FOnSetExpansionRecursive, OnSetExpansionRecursive )

			SLATE_ARGUMENT( TArray<ItemType>* , TreeItemsSource )

			SLATE_ATTRIBUTE( float, ItemHeight )

			SLATE_EVENT( FOnContextMenuOpening, OnContextMenuOpening )

			SLATE_EVENT( FOnMouseButtonDoubleClick, OnMouseButtonDoubleClick )

			SLATE_EVENT( FOnSelectionChanged, OnSelectionChanged )

			SLATE_EVENT( FOnExpansionChanged, OnExpansionChanged )

			SLATE_ATTRIBUTE( ESelectionMode::Type, SelectionMode )

			SLATE_ARGUMENT( TSharedPtr<SHeaderRow>, HeaderRow )

			SLATE_ARGUMENT ( bool, ClearSelectionOnClick )

			SLATE_ARGUMENT( TSharedPtr<SScrollBar>, ExternalScrollbar )

		SLATE_END_ARGS()

		
		/**
		 * Construct this widget
		 *
		 * @param	InArgs	The declaration data for this widget
		 */
		void Construct( const FArguments& InArgs )
		{
			this->OnGenerateRow = InArgs._OnGenerateRow;
			this->OnItemScrolledIntoView = InArgs._OnItemScrolledIntoView;
			this->OnGetChildren = InArgs._OnGetChildren;
			this->OnSetExpansionRecursive = InArgs._OnSetExpansionRecursive;
			this->TreeItemsSource = InArgs._TreeItemsSource;

			this->OnContextMenuOpening = InArgs._OnContextMenuOpening;
			this->OnDoubleClick = InArgs._OnMouseButtonDoubleClick;
			this->OnSelectionChanged = InArgs._OnSelectionChanged;
			this->OnExpansionChanged = InArgs._OnExpansionChanged;
			this->SelectionMode = InArgs._SelectionMode;

			this->bClearSelectionOnClick = InArgs._ClearSelectionOnClick;

			// Check for any parameters that the coder forgot to specify.
			FString ErrorString;
			{
				if ( !this->OnGenerateRow.IsBound() )
				{
					ErrorString += TEXT("Please specify an OnGenerateRow. \n");
				}

				if ( this->TreeItemsSource == NULL )
				{
					ErrorString += TEXT("Please specify a TreeItemsSource. \n");
				}

				if ( !this->OnGetChildren.IsBound() )
				{
					ErrorString += TEXT("Please specify an OnGetChildren. \n");
				}
			}

			if (ErrorString.Len() > 0)
			{
				// Let the coder know what they forgot
				this->ChildSlot
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(ErrorString))
				];
			}
			else
			{
				// Make the TableView
				this->ConstructChildren( 0, InArgs._ItemHeight, EListItemAlignment::LeftAligned, InArgs._HeaderRow, InArgs._ExternalScrollbar, InArgs._OnTreeViewScrolled );
			}
		}

		SAutomationTestTreeView()
			: STreeView< ItemType >()
		{
		}

public:

	/**
	 * Clears the internal widget cache and recreates the tree
	 */
	void ReCreateTreeView()
	{
		this->WidgetGenerator.Clear();
		this->RequestTreeRefresh();
	}
};

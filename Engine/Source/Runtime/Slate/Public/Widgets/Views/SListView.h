// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Styling/SlateTypes.h"
#include "Framework/SlateDelegates.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Views/ITypedTableView.h"
#include "Widgets/Views/STableViewBase.h"
#include "Framework/Views/TableViewTypeTraits.h"
#include "Widgets/Views/STableRow.h"
#include "Types/SlateConstants.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Framework/Layout/Overscroll.h"

/**
 * A ListView widget observes an array of data items and creates visual representations of these items.
 * ListView relies on the property that holding a reference to a value ensures its existence. In other words,
 * neither SListView<FString> nor SListView<FString*> are valid, while SListView< TSharedPtr<FString> > and
 * SListView< UObject* > are valid.
 *
 * A trivial use case appear below:
 *
 *   Given: TArray< TSharedPtr<FString> > Items;
 *
 *   SNew( SListView< TSharedPtr<FString> > )
 *     .ItemHeight(24)
 *     .ListItemsSource( &Items )
 *     .OnGenerateRow( SListView< TSharedPtr<FString> >::MakeOnGenerateWidget( this, &MyClass::OnGenerateRowForList ) )
 *
 * In the example we make all our widgets be 24 screen units tall. The ListView will create widgets based on data items
 * in the Items TArray. When the ListView needs to generate an item, it will do so using the OnGenerateWidgetForList method.
 *
 * A sample implementation of OnGenerateWidgetForList would simply return a TextBlock with the corresponding text:
 *
 * TSharedRef<ITableRow> OnGenerateWidgetForList( TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable )
 * {
 *     return SNew(STextBlock).Text( (*InItem) )
 * }
 *
 */
template <typename ItemType>
class SListView : public STableViewBase, TListTypeTraits<ItemType>::SerializerType, public ITypedTableView< ItemType >
{
public:
	typedef typename TListTypeTraits< ItemType >::NullableType NullableItemType;

	typedef typename TSlateDelegates< ItemType >::FOnGenerateRow FOnGenerateRow;
	typedef typename TSlateDelegates< ItemType >::FOnItemScrolledIntoView FOnItemScrolledIntoView;
	typedef typename TSlateDelegates< NullableItemType >::FOnSelectionChanged FOnSelectionChanged;
	typedef typename TSlateDelegates< ItemType >::FOnMouseButtonClick FOnMouseButtonClick;
	typedef typename TSlateDelegates< ItemType >::FOnMouseButtonDoubleClick FOnMouseButtonDoubleClick;

	typedef typename TSlateDelegates< ItemType >::FOnItemToString_Debug FOnItemToString_Debug; 

	DECLARE_DELEGATE_OneParam( FOnWidgetToBeRemoved, const TSharedRef<ITableRow>& );

public:
	SLATE_BEGIN_ARGS(SListView<ItemType>)
		: _OnGenerateRow()
		, _OnRowReleased()
		, _ListItemsSource()
		, _ItemHeight(16)
		, _OnContextMenuOpening()
		, _OnMouseButtonClick()
		, _OnMouseButtonDoubleClick()
		, _OnSelectionChanged()
		, _SelectionMode(ESelectionMode::Multi)
		, _ClearSelectionOnClick(true)
		, _ExternalScrollbar()
		, _AllowOverscroll(EAllowOverscroll::Yes)
		, _ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible)
		, _WheelScrollMultiplier(GetGlobalScrollAmount())
		, _HandleGamepadEvents( true )
		, _HandleDirectionalNavigation( true )
		, _OnItemToString_Debug()
		, _OnEnteredBadState()
		{
		}

		SLATE_EVENT( FOnGenerateRow, OnGenerateRow )

		SLATE_EVENT( FOnWidgetToBeRemoved, OnRowReleased )

		SLATE_EVENT( FOnTableViewScrolled, OnListViewScrolled )

		SLATE_EVENT( FOnItemScrolledIntoView, OnItemScrolledIntoView )

		SLATE_ARGUMENT( const TArray<ItemType>* , ListItemsSource )

		SLATE_ATTRIBUTE( float, ItemHeight )

		SLATE_EVENT( FOnContextMenuOpening, OnContextMenuOpening )

		SLATE_EVENT(FOnMouseButtonClick, OnMouseButtonClick)

		SLATE_EVENT( FOnMouseButtonDoubleClick, OnMouseButtonDoubleClick )

		SLATE_EVENT( FOnSelectionChanged, OnSelectionChanged )

		SLATE_ATTRIBUTE( ESelectionMode::Type, SelectionMode )

		SLATE_ARGUMENT( TSharedPtr<SHeaderRow>, HeaderRow )

		SLATE_ARGUMENT ( bool, ClearSelectionOnClick )

		SLATE_ARGUMENT( TSharedPtr<SScrollBar>, ExternalScrollbar )

		SLATE_ATTRIBUTE( EVisibility, ScrollbarVisibility)
		
		SLATE_ARGUMENT( EAllowOverscroll, AllowOverscroll );

		SLATE_ARGUMENT( EConsumeMouseWheel, ConsumeMouseWheel );

		SLATE_ARGUMENT( float, WheelScrollMultiplier );

		SLATE_ARGUMENT( bool, HandleGamepadEvents );

		SLATE_ARGUMENT( bool, HandleDirectionalNavigation );

		/** Assign this to get more diagnostics from the list view. */
		SLATE_EVENT(FOnItemToString_Debug, OnItemToString_Debug)

		SLATE_EVENT(FOnTableViewBadState, OnEnteredBadState);

	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct(const typename SListView<ItemType>::FArguments& InArgs)
	{
		this->OnGenerateRow = InArgs._OnGenerateRow;
		this->OnRowReleased = InArgs._OnRowReleased;
		this->OnItemScrolledIntoView = InArgs._OnItemScrolledIntoView;

		this->ItemsSource = InArgs._ListItemsSource;
		this->OnContextMenuOpening = InArgs._OnContextMenuOpening;
		this->OnClick = InArgs._OnMouseButtonClick;
		this->OnDoubleClick = InArgs._OnMouseButtonDoubleClick;
		this->OnSelectionChanged = InArgs._OnSelectionChanged;
		this->SelectionMode = InArgs._SelectionMode;

		this->bClearSelectionOnClick = InArgs._ClearSelectionOnClick;

		this->AllowOverscroll = InArgs._AllowOverscroll;
		this->ConsumeMouseWheel = InArgs._ConsumeMouseWheel;

		this->WheelScrollMultiplier = InArgs._WheelScrollMultiplier;
		this->bHandleGamepadEvents = InArgs._HandleGamepadEvents;
		this->bHandleDirectionalNavigation = InArgs._HandleDirectionalNavigation;

		this->OnItemToString_Debug =
			InArgs._OnItemToString_Debug.IsBound()
			? InArgs._OnItemToString_Debug
			: GetDefaultDebugDelegate();
		OnEnteredBadState = InArgs._OnEnteredBadState;

		// Check for any parameters that the coder forgot to specify.
		FString ErrorString;
		{
			if ( !this->OnGenerateRow.IsBound() )
			{
				ErrorString += TEXT("Please specify an OnGenerateRow. \n");
			}

			if ( this->ItemsSource == nullptr )
			{
				ErrorString += TEXT("Please specify a ListItemsSource. \n");
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
			ConstructChildren( 0, InArgs._ItemHeight, EListItemAlignment::LeftAligned, InArgs._HeaderRow, InArgs._ExternalScrollbar, InArgs._OnListViewScrolled );
			if(ScrollBar.IsValid())
			{
				ScrollBar->SetUserVisibility(InArgs._ScrollbarVisibility);
			}

		}
	}

	SListView( ETableViewMode::Type InListMode = ETableViewMode::List )
		: STableViewBase( InListMode )
		, WidgetGenerator(this)
		, SelectorItem( NullableItemType(nullptr) )
		, RangeSelectionStart( NullableItemType(nullptr) )
		, ItemsSource( nullptr )
		, ItemToScrollIntoView( NullableItemType(nullptr) )
		, UserRequestingScrollIntoView( 0 )
		, ItemToNotifyWhenInView( NullableItemType(nullptr) ) 
	{ }

public:

	// SWidget overrides

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override
	{
		const TArray<ItemType>& ItemsSourceRef = (*this->ItemsSource);

		// Don't respond to key-presses containing "Alt" as a modifier
		if ( ItemsSourceRef.Num() > 0 && !InKeyEvent.IsAltDown() )
		{
			bool bWasHandled = false;
			NullableItemType ItemNavigatedTo( nullptr );

			// Check for selection manipulation keys (Up, Down, Home, End, PageUp, PageDown)
			if ( InKeyEvent.GetKey() == EKeys::Home )
			{
				// Select the first item
				ItemNavigatedTo = ItemsSourceRef[0];
				bWasHandled = true;
			}
			else if ( InKeyEvent.GetKey() == EKeys::End )
			{
				// Select the last item
				ItemNavigatedTo = ItemsSourceRef.Last();
				bWasHandled = true;
			}
			else if ( InKeyEvent.GetKey() == EKeys::PageUp )
			{
				int32 SelectionIndex = 0;
				if( TListTypeTraits<ItemType>::IsPtrValid(SelectorItem) )
				{
					SelectionIndex = ItemsSourceRef.Find( TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType( SelectorItem ) );
				}

				int32 NumItemsInAPage = GetNumLiveWidgets();
				int32 Remainder = NumItemsInAPage % GetNumItemsWide();
				NumItemsInAPage -= Remainder;

				if ( SelectionIndex >= NumItemsInAPage )
				{
					// Select an item on the previous page
					ItemNavigatedTo = ItemsSourceRef[SelectionIndex - NumItemsInAPage];
				}
				else
				{
					ItemNavigatedTo = ItemsSourceRef[0];
				}

				bWasHandled = true;
			}
			else if ( InKeyEvent.GetKey() == EKeys::PageDown )
			{
				int32 SelectionIndex = 0;
				if( TListTypeTraits<ItemType>::IsPtrValid(SelectorItem) )
				{
					SelectionIndex = ItemsSourceRef.Find( TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType( SelectorItem ) );
				}

				int32 NumItemsInAPage = GetNumLiveWidgets();
				int32 Remainder = NumItemsInAPage % GetNumItemsWide();
				NumItemsInAPage -= Remainder;

				if ( SelectionIndex < ItemsSourceRef.Num() - NumItemsInAPage )
				{
					// Select an item on the next page
					ItemNavigatedTo = ItemsSourceRef[SelectionIndex + NumItemsInAPage];
				}
				else
				{
					ItemNavigatedTo = ItemsSourceRef.Last();
				}

				bWasHandled = true;
			}

			if( TListTypeTraits<ItemType>::IsPtrValid(ItemNavigatedTo) )
			{
				ItemType ItemToSelect( TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType( ItemNavigatedTo ) );
				NavigationSelect( ItemToSelect, InKeyEvent );
			}
			else
			{
				// Change selected status of item.
				if( TListTypeTraits<ItemType>::IsPtrValid(SelectorItem) && InKeyEvent.GetKey() == EKeys::SpaceBar )
				{
					ItemType SelectorItemDereference( TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType( SelectorItem ) );

					// Deselect.
					if( InKeyEvent.IsControlDown() || SelectionMode.Get() == ESelectionMode::SingleToggle )
					{
						this->Private_SetItemSelection( SelectorItemDereference, !( this->Private_IsItemSelected( SelectorItemDereference ) ), true );
						this->Private_SignalSelectionChanged( ESelectInfo::OnKeyPress );
						bWasHandled = true;
					}
					else
					{
						// Already selected, don't handle.
						if( this->Private_IsItemSelected( SelectorItemDereference ) )
						{
							bWasHandled = false;
						}
						// Select.
						else
						{
							this->Private_SetItemSelection( SelectorItemDereference, true, true );
							this->Private_SignalSelectionChanged( ESelectInfo::OnKeyPress );
							bWasHandled = true;
						}
					}

					RangeSelectionStart = SelectorItem;

					// If the selector is not in the view, scroll it into view.
					TSharedPtr<ITableRow> WidgetForItem = this->WidgetGenerator.GetWidgetForItem( SelectorItemDereference );
					if ( !WidgetForItem.IsValid() )
					{
						this->RequestScrollIntoView(SelectorItemDereference, InKeyEvent.GetUserIndex());
					}
				}
				// Select all items
				else if ( (!InKeyEvent.IsShiftDown() && !InKeyEvent.IsAltDown() && InKeyEvent.IsControlDown() && InKeyEvent.GetKey() == EKeys::A) && SelectionMode.Get() == ESelectionMode::Multi )
				{
					this->Private_ClearSelection();

					for ( int32 ItemIdx = 0; ItemIdx < ItemsSourceRef.Num(); ++ItemIdx )
					{
						this->Private_SetItemSelection( ItemsSourceRef[ItemIdx], true );
					}

					this->Private_SignalSelectionChanged(ESelectInfo::OnKeyPress);

					bWasHandled = true;
				}
			}

			return (bWasHandled ? FReply::Handled() : FReply::Unhandled());
		}

		return STableViewBase::OnKeyDown(MyGeometry, InKeyEvent);
	}

	virtual FNavigationReply OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent) override
	{
		if (this->ItemsSource && this->bHandleDirectionalNavigation && (this->bHandleGamepadEvents || InNavigationEvent.GetNavigationGenesis() != ENavigationGenesis::Controller))
		{
			const TArray<ItemType>& ItemsSourceRef = (*this->ItemsSource);

			const int32 NumItemsWide = GetNumItemsWide();
			const int32 CurSelectionIndex = (!TListTypeTraits<ItemType>::IsPtrValid(SelectorItem)) ? 0 : ItemsSourceRef.Find(TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType(SelectorItem));
			int32 AttemptSelectIndex = -1;

			const EUINavigation NavType = InNavigationEvent.GetNavigationType();
			switch (NavType)
			{
			case EUINavigation::Up:
				AttemptSelectIndex = CurSelectionIndex - NumItemsWide;
				break;

			case EUINavigation::Down:
				AttemptSelectIndex = CurSelectionIndex + NumItemsWide;
				break;

			default:
				break;
			}

			// If it's valid we'll scroll it into view and return an explicit widget in the FNavigationReply
			if (ItemsSourceRef.IsValidIndex(AttemptSelectIndex))
			{
				NavigationSelect(ItemsSourceRef[AttemptSelectIndex], InNavigationEvent);
				return FNavigationReply::Explicit(nullptr);
			}
		}

		return STableViewBase::OnNavigation(MyGeometry, InNavigationEvent);
	}

	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override
	{
		if ( bClearSelectionOnClick
			&& SelectionMode.Get() != ESelectionMode::None
			&& MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
			&& !MouseEvent.IsControlDown()
			&& !MouseEvent.IsShiftDown()
			)
		{
			// Left clicking on a list (but not an item) will clear the selection on mouse button down.
			// Right clicking is handled on mouse up.
			if ( this->Private_GetNumSelectedItems() > 0 )
			{
				this->Private_ClearSelection();
				this->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);
			}

			return FReply::Handled();
		}

		return STableViewBase::OnMouseButtonDown(MyGeometry, MouseEvent);
	}

	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override
	{
		if ( bClearSelectionOnClick
			&& SelectionMode.Get() != ESelectionMode::None
			&& MouseEvent.GetEffectingButton() == EKeys::RightMouseButton
			&& !MouseEvent.IsControlDown()
			&& !MouseEvent.IsShiftDown()
			&& !this->IsRightClickScrolling()
			)
		{
			// Right clicking on a list (but not an item) will clear the selection on mouse button up.
			// Left clicking is handled on mouse down
			if ( this->Private_GetNumSelectedItems() > 0 )
			{
				this->Private_ClearSelection();
				this->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);
			}
		}

		return STableViewBase::OnMouseButtonUp(MyGeometry, MouseEvent);
	}

private:

	friend class FWidgetGenerator;
	/**
	 * A WidgetGenerator is a component responsible for creating widgets from data items.
	 * It also provides mapping from currently generated widgets to the data items which they
	 * represent.
	 */
	class FWidgetGenerator
	{
	public:
		FWidgetGenerator(SListView<ItemType>* InOwnerList)
			: OwnerList(InOwnerList)
		{
		}

		/**
		 * Find a widget for this item if it has already been constructed.
		 *
		 * @param Item  The item for which to find the widget.
		 * @return A pointer to the corresponding widget if it exists; otherwise nullptr.
		 */
		TSharedPtr<ITableRow> GetWidgetForItem( const ItemType& Item )
		{
			TSharedRef<ITableRow>* LookupResult = ItemToWidgetMap.Find( Item );
			if ( LookupResult != nullptr )
			{
				return *LookupResult;
			}
			else
			{
				return TSharedPtr<ITableRow>(nullptr);
			}
		}

		/**
		 * Keep track of every item and corresponding widget during a generation pass.
		 *
		 * @param InItem             The DataItem which is in view.
		 * @param InGeneratedWidget  The widget generated for this item; it may have been newly generated.
		 */
		void OnItemSeen( ItemType InItem, TSharedRef<ITableRow> InGeneratedWidget)
		{
			TSharedRef<ITableRow>* LookupResult = ItemToWidgetMap.Find( InItem );
			const bool bWidgetIsNewlyGenerated = (LookupResult == nullptr);
			if ( bWidgetIsNewlyGenerated )
			{
				// It's a newly generated item!
				ItemToWidgetMap.Add( InItem, InGeneratedWidget );
				WidgetMapToItem.Add( &InGeneratedWidget.Get(), InItem );		
			}

			// We should not clean up this item's widgets because it is in view.
			ItemsToBeCleanedUp.Remove(InItem);
			ItemsWithGeneratedWidgets.Add(InItem);
		}

		/**
		 * Called at the beginning of the generation pass.
		 * Begins tracking of which widgets were in view and which were not (so we can clean them up)
		 * 
		 * @param InNumDataItems   The total number of data items being observed
		 */
		void OnBeginGenerationPass()
		{
			// Assume all the previously generated items need to be cleaned up.				
			ItemsToBeCleanedUp = ItemsWithGeneratedWidgets;
			ItemsWithGeneratedWidgets.Empty();
		}

		/**
		 * Called at the end of the generation pass.
		 * Cleans up any widgets associated with items that were not in view this frame.
		 */
		void OnEndGenerationPass()
		{
			ensureMsgf( OwnerList, TEXT( "OwnerList is null, something is wrong." ) );

			for( int32 ItemIndex = 0; ItemIndex < ItemsToBeCleanedUp.Num(); ++ItemIndex )
			{
				ItemType ItemToBeCleanedUp = ItemsToBeCleanedUp[ItemIndex];
				const TSharedRef<ITableRow>* FindResult = ItemToWidgetMap.Find( ItemToBeCleanedUp );
				if ( FindResult != nullptr )
				{
					const TSharedRef<ITableRow> WidgetToCleanUp = *FindResult;
					ItemToWidgetMap.Remove( ItemToBeCleanedUp );
					WidgetMapToItem.Remove( &WidgetToCleanUp.Get() );

					// broadcast here
					if ( OwnerList )
					{
						OwnerList->OnRowReleased.ExecuteIfBound( WidgetToCleanUp );
					}
				}				
			}

			ValidateWidgetGeneration();

			ItemsToBeCleanedUp.Reset();
		}

		/**
		 * Clear everything so widgets will be regenerated
		*/
		void Clear()
		{
			// Clean up all the previously generated items			
			ItemsToBeCleanedUp = ItemsWithGeneratedWidgets;
			ItemsWithGeneratedWidgets.Empty();

			for( int32 ItemIndex = 0; ItemIndex < ItemsToBeCleanedUp.Num(); ++ItemIndex )
			{
				ItemType ItemToBeCleanedUp = ItemsToBeCleanedUp[ItemIndex];
				const TSharedRef<ITableRow>* FindResult = ItemToWidgetMap.Find( ItemToBeCleanedUp );
				if ( FindResult != nullptr )
				{
					const TSharedRef<ITableRow> WidgetToCleanUp = *FindResult;
					ItemToWidgetMap.Remove( ItemToBeCleanedUp );
					WidgetMapToItem.Remove( &WidgetToCleanUp.Get() );
				}				
			}

			ItemsToBeCleanedUp.Reset();
		}

		void ValidateWidgetGeneration()
		{
			const bool bMapsMismatch = ItemToWidgetMap.Num() != WidgetMapToItem.Num();
			const bool bGeneratedWidgetsSizeMismatch = WidgetMapToItem.Num() != ItemsWithGeneratedWidgets.Num();
			if (bMapsMismatch)
			{
				UE_LOG(LogSlate, Warning, TEXT("ItemToWidgetMap length (%d) does not match WidgetMapToItem length (%d) in %s. Diagnostics follow. "),
					ItemToWidgetMap.Num(),
					WidgetMapToItem.Num(),
					OwnerList ? *OwnerList->ToString() : TEXT("null"));
			}
			
			if (bGeneratedWidgetsSizeMismatch)
			{
				UE_LOG(LogSlate, Warning,
					TEXT("WidgetMapToItem length (%d) does not match ItemsWithGeneratedWidgets length (%d). This is often because the same item is in the list more than once in %s. Diagnostics follow."),
					WidgetMapToItem.Num(),
					ItemsWithGeneratedWidgets.Num(),
					OwnerList ? *OwnerList->ToString() : TEXT("null"));
			}

			if (bMapsMismatch || bGeneratedWidgetsSizeMismatch)
			{
				if (OwnerList->OnItemToString_Debug.IsBound())
				{
					UE_LOG(LogSlate, Warning, TEXT(""));
					UE_LOG(LogSlate, Warning, TEXT("ItemToWidgetMap :"));
					for (auto ItemWidgetPair = ItemToWidgetMap.CreateConstIterator(); ItemWidgetPair; ++ItemWidgetPair)
					{
						const TSharedRef<SWidget> RowAsWidget = ItemWidgetPair.Value()->AsWidget();
						UE_LOG(LogSlate, Warning, TEXT("%s -> 0x%08x @ %s"), *OwnerList->OnItemToString_Debug.Execute(ItemWidgetPair.Key()), &RowAsWidget.Get(), *RowAsWidget->ToString() );
					}

					UE_LOG(LogSlate, Warning, TEXT(""));
					UE_LOG(LogSlate, Warning, TEXT("WidgetMapToItem:"))
					for (auto WidgetItemPair = WidgetMapToItem.CreateConstIterator(); WidgetItemPair; ++WidgetItemPair)
					{
						UE_LOG(LogSlate, Warning, TEXT("0x%08x -> %s"), WidgetItemPair.Key(), *OwnerList->OnItemToString_Debug.Execute(WidgetItemPair.Value()) );
					}

					UE_LOG(LogSlate, Warning, TEXT(""));
					UE_LOG(LogSlate, Warning, TEXT("ItemsWithGeneratedWidgets:"));
					for (int i = 0; i < ItemsWithGeneratedWidgets.Num(); ++i)
					{
						UE_LOG(LogSlate, Warning, TEXT("[%d] %s"),i, *OwnerList->OnItemToString_Debug.Execute(ItemsWithGeneratedWidgets[i]));
					}
				}
				else
				{
					UE_LOG(LogSlate, Warning, TEXT("Provide custom 'OnItemToString_Debug' for diagnostics dump."));
				}

				OwnerList->OnEnteredBadState.ExecuteIfBound();

				checkf( false, TEXT("%s detected a critical error. See diagnostic dump above. Provide a custom 'OnItemToString_Debug' for more detailed diagnostics."), *OwnerList->ToString() );
			}			
		}

		/** We store a pointer to the owner list for error purposes, so when asserts occur we can report which list it happened for. */
		SListView<ItemType>* OwnerList;

		/** Map of DataItems to corresponding SWidgets */
		TMap< ItemType, TSharedRef<ITableRow> > ItemToWidgetMap;

		/** Map of SWidgets to DataItems from which they were generated */
		TMap< const ITableRow*, ItemType > WidgetMapToItem;

		/** A set of Items that currently have a generated widget */
		TArray< ItemType > ItemsWithGeneratedWidgets;

		/** Total number of DataItems the last time we performed a generation pass. */
		int32 TotalItemsLastGeneration;

		/** Items that need their widgets destroyed because they are no longer on screen. */
		TArray<ItemType> ItemsToBeCleanedUp;
	};

public:

	// A low-level interface for use various widgets generated by ItemsWidgets(Lists, Trees, etc).
	// These handle selection, expansion, and other such properties common to ItemsWidgets.
	//

	virtual void Private_SetItemSelection( ItemType TheItem, bool bShouldBeSelected, bool bWasUserDirected = false ) override
	{
		if ( SelectionMode.Get() == ESelectionMode::None )
		{
			return;
		}

		if ( bShouldBeSelected )
		{
			SelectedItems.Add( TheItem );
		}
		else
		{
			SelectedItems.Remove( TheItem );
		}

		// Only move the selector item and range selection start if the user directed this change in selection.
		if( bWasUserDirected )
		{
			SelectorItem = TheItem;
			RangeSelectionStart = TheItem;
		}

		this->InertialScrollManager.ClearScrollVelocity();
	}

	virtual void Private_ClearSelection() override
	{
		SelectedItems.Empty();

		this->InertialScrollManager.ClearScrollVelocity();
	}

	virtual void Private_SelectRangeFromCurrentTo( ItemType InRangeSelectionEnd ) override
	{
		if ( SelectionMode.Get() == ESelectionMode::None )
		{
			return;
		}

		const TArray<ItemType>& ItemsSourceRef = (*ItemsSource);

		int32 RangeStartIndex = 0;
		if( TListTypeTraits<ItemType>::IsPtrValid(RangeSelectionStart) )
		{
			RangeStartIndex = ItemsSourceRef.Find( TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType( RangeSelectionStart ) );
		}

		int32 RangeEndIndex = ItemsSourceRef.Find( InRangeSelectionEnd );

		RangeStartIndex = FMath::Clamp(RangeStartIndex, 0, ItemsSourceRef.Num());
		RangeEndIndex = FMath::Clamp(RangeEndIndex, 0, ItemsSourceRef.Num());

		if (RangeEndIndex < RangeStartIndex)
		{
			Swap( RangeStartIndex, RangeEndIndex );
		}

		for( int32 ItemIndex = RangeStartIndex; ItemIndex <= RangeEndIndex; ++ItemIndex )
		{
			SelectedItems.Add( ItemsSourceRef[ItemIndex] );
		}

		this->InertialScrollManager.ClearScrollVelocity();
	}

	virtual void Private_SignalSelectionChanged(ESelectInfo::Type SelectInfo) override
	{
		if ( SelectionMode.Get() == ESelectionMode::None )
		{
			return;
		}

		if( OnSelectionChanged.IsBound() )
		{
			NullableItemType SelectedItem = (SelectedItems.Num() > 0)
				? (*typename TSet<ItemType>::TIterator(SelectedItems))
				: TListTypeTraits< ItemType >::MakeNullPtr();

			OnSelectionChanged.ExecuteIfBound(SelectedItem, SelectInfo );
		}
	}

	virtual const ItemType* Private_ItemFromWidget( const ITableRow* TheWidget ) const override
	{
		ItemType const * LookupResult = WidgetGenerator.WidgetMapToItem.Find( TheWidget );
		return LookupResult == nullptr ? nullptr : LookupResult;
	}

	virtual bool Private_UsesSelectorFocus() const override
	{
		return true;
	}

	virtual bool Private_HasSelectorFocus( const ItemType& TheItem ) const override
	{
		return SelectorItem == TheItem;
	}

	virtual bool Private_IsItemSelected( const ItemType& TheItem ) const override
	{
		return nullptr != SelectedItems.Find(TheItem);
	}

	virtual bool Private_IsItemExpanded( const ItemType& TheItem ) const override
	{
		// List View does not support item expansion.
		return false;	
	}

	virtual void Private_SetItemExpansion( ItemType TheItem, bool bShouldBeExpanded ) override
	{
		// Do nothing; you cannot expand an item in a list!
	}

	virtual void Private_OnExpanderArrowShiftClicked( ItemType TheItem, bool bShouldBeExpanded ) override
	{
		// Do nothing; you cannot expand an item in a list!
	}

	virtual bool Private_DoesItemHaveChildren( int32 ItemIndexInList ) const override
	{
		// List View items cannot have children
		return false;
	}

	virtual int32 Private_GetNumSelectedItems() const override
	{
		return SelectedItems.Num();
	}

	virtual int32 Private_GetNestingDepth( int32 ItemIndexInList ) const override
	{
		// List View items are not indented
		return 0;
	}

	virtual ESelectionMode::Type Private_GetSelectionMode() const override
	{
		return SelectionMode.Get();
	}

	virtual void Private_OnItemRightClicked( ItemType TheItem, const FPointerEvent& MouseEvent ) override
	{
		this->OnRightMouseButtonUp( MouseEvent );
	}

	virtual bool Private_OnItemClicked(ItemType TheItem) override
	{
		if (OnClick.ExecuteIfBound(TheItem))
		{
			return true;	// Handled
		}

		return false;	// Not handled
	}
	
	virtual bool Private_OnItemDoubleClicked( ItemType TheItem ) override
	{
		if( OnDoubleClick.ExecuteIfBound( TheItem ) )
		{
			return true;	// Handled
		}

		return false;	// Not handled
	}

	virtual ETableViewMode::Type GetTableViewMode() const override
	{
		return TableViewMode;
	}

	virtual TSharedRef<SWidget> AsWidget() override
	{
		return SharedThis(this);
	}

public:	

	/**
	 * Remove any items that are no longer in the list from the selection set.
	 */
	virtual void UpdateSelectionSet() override
	{
		// Trees take care of this update in a different way.
		if ( TableViewMode != ETableViewMode::Tree )
		{
			bool bSelectionChanged = false;
			if ( ItemsSource == nullptr )
			{
				// We are no longer observing items so there is no more selection.
				this->Private_ClearSelection();
				bSelectionChanged = true;
			}
			else
			{
				// We are observing some items; they are potentially different.
				// Unselect any that are no longer being observed.
				TSet< ItemType > NewSelectedItems;
				for ( int32 ItemIndex = 0; ItemIndex < ItemsSource->Num(); ++ItemIndex )
				{
					ItemType CurItem = (*ItemsSource)[ItemIndex];
					const bool bItemIsSelected = ( nullptr != SelectedItems.Find( CurItem ) );
					if ( bItemIsSelected )
					{
						NewSelectedItems.Add( CurItem );
					}
				}

				// Look for items that were removed from the selection.
				TSet< ItemType > SetDifference = SelectedItems.Difference( NewSelectedItems );
				bSelectionChanged = (SetDifference.Num()) > 0;

				// Update the selection to reflect the removal of any items from the ItemsSource.
				SelectedItems = NewSelectedItems;
			}

			if (bSelectionChanged)
			{
				Private_SignalSelectionChanged(ESelectInfo::Direct);
			}
		}
	}

	/**
	 * Update generate Widgets for Items as needed and clean up any Widgets that are no longer needed.
	 * Re-arrange the visible widget order as necessary.
	 */
	virtual FReGenerateResults ReGenerateItems( const FGeometry& MyGeometry ) override
	{
		// Clear all the items from our panel. We will re-add them in the correct order momentarily.
		this->ClearWidgets();

		// Ensure that we always begin and clean up a generation pass.
		
		FGenerationPassGuard GenerationPassGuard(WidgetGenerator);

		const TArray<ItemType>* SourceItems = ItemsSource;
		if ( SourceItems != nullptr && SourceItems->Num() > 0 )
		{
			// Items in view, including fractional items
			float ItemsInView = 0.0f;

			// Height of generated widgets that is landing in the bounds of the view.
			float ViewHeightUsedSoFar = 0.0f;

			// Total height of widgets generated so far.
			float HeightGeneratedSoFar = 0.0f;

			// Index of the item at which we start generating based on how far scrolled down we are
			// Note that we must generate at LEAST one item.
			int32 StartIndex = FMath::Clamp( FMath::FloorToInt(ScrollOffset), 0, SourceItems->Num()-1 );

			// Height of the first item that is generated. This item is at the location where the user requested we scroll
			float FirstItemHeight = 0.0f;

			// Generate widgets assuming scenario a.
			bool bGeneratedEnoughForSmoothScrolling = false;
			bool bAtEndOfList = false;

			const float LayoutScaleMultiplier = MyGeometry.GetAccumulatedLayoutTransform().GetScale();
			for( int32 ItemIndex = StartIndex; !bGeneratedEnoughForSmoothScrolling && ItemIndex < SourceItems->Num(); ++ItemIndex )
			{
				const ItemType& CurItem = (*SourceItems)[ItemIndex];

				const float ItemHeight = GenerateWidgetForItem(CurItem, ItemIndex, StartIndex, LayoutScaleMultiplier);

				const bool bIsFirstItem = ItemIndex == StartIndex;

				if (bIsFirstItem)
				{
					FirstItemHeight = ItemHeight;
				}

				// Track the number of items in the view, including fractions.
				if (bIsFirstItem)
				{
					// The first item may not be fully visible (but cannot exceed 1)
					// FirstItemFractionScrolledIntoView is the fraction of the item that is visible after taking into account anything that may be scrolled off the top of the list view
					// FirstItemHeightScrolledIntoView is the height of the item, ignoring anything that is scrolled off the top of the list view
					// FirstItemVisibleFraction is either: The visible item height as a fraction of the available list view height (if the item size is larger than the available size, otherwise this will be >1), or just FirstItemFractionScrolledIntoView (which can never be >1)
					const float FirstItemFractionScrolledIntoView = 1.0f - FMath::Max(FMath::Fractional(ScrollOffset), 0.0f);
					const float FirstItemHeightScrolledIntoView = ItemHeight * FirstItemFractionScrolledIntoView;
					const float FirstItemVisibleFraction = FMath::Min(MyGeometry.GetLocalSize().Y / FirstItemHeightScrolledIntoView, FirstItemFractionScrolledIntoView);
					ItemsInView += FirstItemVisibleFraction;
				}
				else if (ViewHeightUsedSoFar + ItemHeight > MyGeometry.GetLocalSize().Y)
				{
					// The last item may not be fully visible either
					ItemsInView += (MyGeometry.GetLocalSize().Y - ViewHeightUsedSoFar) / ItemHeight;
				}
				else
				{
					ItemsInView += 1;
				}

				HeightGeneratedSoFar += ItemHeight;

				ViewHeightUsedSoFar += (bIsFirstItem)
					? ItemHeight * ItemsInView	// For the first item, ItemsInView <= 1.0f
					: ItemHeight;

				if (ItemIndex >= SourceItems->Num()-1)
				{
					bAtEndOfList = true;
				}

				if (ViewHeightUsedSoFar > MyGeometry.GetLocalSize().Y )
				{
					bGeneratedEnoughForSmoothScrolling = true;
				}
			}

			// Handle scenario b.
			// We may have stopped because we got to the end of the items.
			// But we may still have space to fill!
			if (bAtEndOfList && ViewHeightUsedSoFar < MyGeometry.GetLocalSize().Y)
			{
				float NewScrollOffsetForBackfill = StartIndex + (HeightGeneratedSoFar - MyGeometry.GetLocalSize().Y) / FirstItemHeight;

				for( int32 ItemIndex = StartIndex-1; HeightGeneratedSoFar < MyGeometry.GetLocalSize().Y && ItemIndex >= 0; --ItemIndex )
				{
					const ItemType& CurItem = (*SourceItems)[ItemIndex];

					const float ItemHeight = GenerateWidgetForItem(CurItem, ItemIndex, StartIndex, LayoutScaleMultiplier);

					if (HeightGeneratedSoFar + ItemHeight > MyGeometry.GetLocalSize().Y)
					{
						// Generated the item that puts us over the top.
						// Count the fraction of this item that will stick out above the list
						NewScrollOffsetForBackfill = ItemIndex + (HeightGeneratedSoFar + ItemHeight - MyGeometry.GetLocalSize().Y) / ItemHeight;
					}

					// The widget used up some of the available vertical space.
					HeightGeneratedSoFar += ItemHeight;
				}

				return FReGenerateResults(NewScrollOffsetForBackfill, HeightGeneratedSoFar, ItemsSource->Num() - NewScrollOffsetForBackfill, bAtEndOfList);
			}

			return FReGenerateResults(ScrollOffset, HeightGeneratedSoFar, ItemsInView, bAtEndOfList);
		}

		return FReGenerateResults(0.0f, 0.0f, 0.0f, false);

	}

	float GenerateWidgetForItem( const ItemType& CurItem, int32 ItemIndex, int32 StartIndex, float LayoutScaleMultiplier )
	{
		// Find a previously generated Widget for this item, if one exists.
		TSharedPtr<ITableRow> WidgetForItem = WidgetGenerator.GetWidgetForItem( CurItem );
		if ( !WidgetForItem.IsValid() )
		{
			// We couldn't find an existing widgets, meaning that this data item was not visible before.
			// Make a new widget for it.
			WidgetForItem = this->GenerateNewWidget( CurItem );
		}

		// It is useful to know the item's index that the widget was generated from.
		// Helps with even/odd coloring
		WidgetForItem->SetIndexInList(ItemIndex);

		// Let the item generator know that we encountered the current Item and associated Widget.
		WidgetGenerator.OnItemSeen( CurItem, WidgetForItem.ToSharedRef() );

		// We rely on the widgets desired size in order to determine how many will fit on screen.
		const TSharedRef<SWidget> NewlyGeneratedWidget = WidgetForItem->AsWidget();
		NewlyGeneratedWidget->SlatePrepass(LayoutScaleMultiplier);

		const bool IsFirstWidgetOnScreen = (ItemIndex == StartIndex);
		const float ItemHeight = NewlyGeneratedWidget->GetDesiredSize().Y;

		// We have a widget for this item; add it to the panel so that it is part of the UI.
		if (ItemIndex >= StartIndex)
		{
			// Generating widgets downward
			this->AppendWidget( WidgetForItem.ToSharedRef() );
		}
		else
		{
			// Backfilling widgets; going upward
			this->InsertWidget( WidgetForItem.ToSharedRef() );
		}

		return ItemHeight;
	}

	/** @return how many items there are in the TArray being observed */
	virtual int32 GetNumItemsBeingObserved() const override
	{
		return ItemsSource == nullptr ? 0 : ItemsSource->Num();
	}

	/**
	 * Given an data item, generate a widget corresponding to it.
	 *
	 * @param InItem  The data item which to visualize.
	 *
	 * @return A new Widget that represents the data item.
	 */
	virtual TSharedRef<ITableRow> GenerateNewWidget(ItemType InItem)
	{
		if ( OnGenerateRow.IsBound() )
		{
			return OnGenerateRow.Execute( InItem, SharedThis(this) );
		}
		else
		{
			// The programmer did not provide an OnGenerateRow() handler; let them know.
			TSharedRef< STableRow<ItemType> > NewListItemWidget = 
				SNew( STableRow<ItemType>, SharedThis(this) )
				.Content()
				[
					SNew(STextBlock) .Text( NSLOCTEXT("SListView", "BrokenUIMessage", "OnGenerateWidget() not assigned.") )
				];

			return NewListItemWidget;
		}

	}

	/**
	 * Given a Widget, find the corresponding data item.
	 * 
	 * @param WidgetToFind  An widget generated by the list view for some data item.
	 *
	 * @return the data item from which the WidgetToFind was generated
	 */
	const ItemType* ItemFromWidget( const ITableRow* WidgetToFind ) const
	{
		return Private_ItemFromWidget( WidgetToFind );
	}

	/**
	 * Test if the current item is selected.
	 *
	 * @param InItem The item to test.
	 *
	 * @return true if the item is selected in this list; false otherwise.
	 */
	bool IsItemSelected( const ItemType& InItem ) const
	{
		if ( SelectionMode.Get() == ESelectionMode::None )
		{
			return false;
		}

		return Private_IsItemSelected( InItem );
	}

	/**
	 * Set the selection state of an item.
	 *
	 * @param InItem      The Item whose selection state to modify
	 * @param bSelected   true to select the item; false to unselect
	 * @param SelectInfo  Provides context on how the selection changed
	 */
	void SetItemSelection( const ItemType& InItem, bool bSelected, ESelectInfo::Type SelectInfo = ESelectInfo::Direct )
	{
		if ( SelectionMode.Get() == ESelectionMode::None )
		{
			return;
		}

		Private_SetItemSelection( InItem, bSelected, SelectInfo != ESelectInfo::Direct);
		Private_SignalSelectionChanged(SelectInfo);
	}

	/**
	 * Empty the selection set.
	 */
	void ClearSelection()
	{
		if ( SelectionMode.Get() == ESelectionMode::None )
		{
			return;
		}

		if ( SelectedItems.Num() == 0 )
		{
			return;
		}

		Private_ClearSelection();
		Private_SignalSelectionChanged(ESelectInfo::Direct);
	}

	/**
	 * Gets the number of selected items.
	 *
	 * @return Number of selected items.
	 */
	int32 GetNumItemsSelected() const
	{
		return SelectedItems.Num();
	}

	/** Deletes all items and rebuilds the list */
	void RebuildList()
	{
		WidgetGenerator.Clear();
		RequestListRefresh();
	}

	/**
	 * Returns a list of selected item indices, or an empty array if nothing is selected
	 *
	 * @return	List of selected item indices (in no particular order)
	 */
	TArray< ItemType > GetSelectedItems() const
	{
		TArray< ItemType > SelectedItemArray;
		SelectedItemArray.Empty( SelectedItems.Num() );
		for( typename TSet< ItemType >::TConstIterator SelectedItemIt( SelectedItems ); SelectedItemIt; ++SelectedItemIt )
		{
			SelectedItemArray.Add( *SelectedItemIt );
		}
		return SelectedItemArray;
	}

	int32 GetSelectedItems(TArray< ItemType >&SelectedItemArray) const
	{
		SelectedItemArray.Empty(SelectedItems.Num());
		for (typename TSet< ItemType >::TConstIterator SelectedItemIt(SelectedItems); SelectedItemIt; ++SelectedItemIt)
		{
			SelectedItemArray.Add(*SelectedItemIt);
		}
		return SelectedItems.Num();
	}

	/**
	 * Checks whether the specified item is currently visible in the list view.
	 *
	 * @param Item - The item to check.
	 *
	 * @return true if the item is visible, false otherwise.
	 */
	bool IsItemVisible( ItemType Item )
	{
		return WidgetGenerator.GetWidgetForItem(Item).IsValid();
	}

	/**
	 * Scroll an item into view. If the item is not found, fails silently.
	 *
	 * @param ItemToView  The item to scroll into view on next tick.
	 */
	void RequestScrollIntoView( ItemType ItemToView, const uint32 UserIndex = 0, const bool NavigateOnScrollIntoView = false)
	{
		ItemToScrollIntoView = ItemToView; 
		UserRequestingScrollIntoView = UserIndex;
		bNavigateOnScrollIntoView = NavigateOnScrollIntoView;
		RequestListRefresh();
	}

	/**
	 * Set the currently selected Item.
	 *
	 * @param SoleSelectedItem   Sole item that should be selected.
	 * @param SelectInfo Provides context on how the selection changed
	 */
	void SetSelection( ItemType SoleSelectedItem, ESelectInfo::Type SelectInfo = ESelectInfo::Direct  )
	{
		SelectedItems.Empty();
		SetItemSelection( SoleSelectedItem, true, SelectInfo );
	}

	/**
	 * Find a widget for this item if it has already been constructed.
	 *
	 * @param InItem  The item for which to find the widget.
	 *
	 * @return A pointer to the corresponding widget if it exists; otherwise nullptr.
	*/
	TSharedPtr<ITableRow> WidgetFromItem( const ItemType& InItem )
	{
		return WidgetGenerator.GetWidgetForItem(InItem);
	}

	/**
	 * Lists and Trees serialize items that they observe because they rely on the property
	 * that holding a reference means it will not be garbage collected.
	 *
	 * @param Ar The archive to serialize with
	 */
	virtual void AddReferencedObjects( FReferenceCollector& Collector )
	{
		TListTypeTraits<ItemType>::AddReferencedObjects( Collector, WidgetGenerator.ItemsWithGeneratedWidgets, SelectedItems );
	}

	/**
	* Will determine the max row size for the specified column id
	*
	* @param ColumnId  Column Id
	* @param Orientation  Orientation that is main axis you want to query
	*
	* @return The max size for a column Id.
	*/
	FVector2D GetMaxRowSizeForColumn(const FName& ColumnId, EOrientation Orientation)
	{
		FVector2D MaxSize = FVector2D::ZeroVector;

		for (auto It = WidgetGenerator.WidgetMapToItem.CreateConstIterator(); It; ++It)
		{
			const ITableRow* TableRow = It.Key();
			FVector2D NewMaxSize = TableRow->GetRowSizeForColumn(ColumnId);

			// We'll return the full size, but we only take into consideration the asked axis for the calculation of the size
			if (NewMaxSize.Component(Orientation) > MaxSize.Component(Orientation))
			{
				MaxSize = NewMaxSize;
			}
		}

		return MaxSize;
	}

protected:

	FOnItemToString_Debug GetDefaultDebugDelegate()
	{
		return
		FOnItemToString_Debug::CreateLambda([](ItemType InItem)
		{
			if (TListTypeTraits<ItemType>::IsPtrValid(InItem))
			{
				return FString::Printf(TEXT("0x%08x"), &(*InItem));
			}
			else
			{
				return FString(TEXT("nullptr"));
			}
		});
	}

	/**
	 * If there is a pending request to scroll an item into view, do so.
	 * 
	 * @param ListViewGeometry  The geometry of the listView; can be useful for centering the item.
	 */
	virtual EScrollIntoViewResult ScrollIntoView( const FGeometry& ListViewGeometry ) override
	{
		if ( TListTypeTraits<ItemType>::IsPtrValid(ItemToScrollIntoView) && ItemsSource != nullptr )
		{
			const int32 IndexOfItem = ItemsSource->Find( TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType( ItemToScrollIntoView ) );
			if (IndexOfItem != INDEX_NONE)
			{
				double NumLiveWidgets = GetNumLiveWidgets();
				if (NumLiveWidgets == 0 && IsPendingRefresh())
				{
					// Use the last number of widgets on screen to estimate if we actually need to scroll.
					NumLiveWidgets = LastGenerateResults.ExactNumRowsOnScreen;

					// If we still don't have any widgets, we're not in a situation where we can scroll an item into view
					// (probably as nothing has been generated yet), so we'll defer this again until the next frame
					if (NumLiveWidgets == 0)
					{
						return EScrollIntoViewResult::Deferred;
					}
				}
				
				// Only scroll the item into view if it's not already in the visible range
				const double IndexPlusOne = IndexOfItem+1;
				if (IndexOfItem < ScrollOffset || IndexPlusOne > (ScrollOffset + NumLiveWidgets))
				{
					// Scroll the top of the listview to the item in question
					double NewScrollOffset = IndexOfItem;
					// Center the list view on the item in question.
					NewScrollOffset -= (NumLiveWidgets / 2);
					//we also don't want the widget being chopped off if it is at the end of the list
					const double MoveBackBy = FMath::Clamp<double>(IndexPlusOne - (NewScrollOffset + NumLiveWidgets), 0, FLT_MAX);
					//Move to the correct center spot
					NewScrollOffset += MoveBackBy;

					SetScrollOffset( NewScrollOffset );
				}

				RequestListRefresh();

				ItemToNotifyWhenInView = ItemToScrollIntoView;
			}

			TListTypeTraits<ItemType>::ResetPtr(ItemToScrollIntoView);
		}

		return EScrollIntoViewResult::Success;
	}

	virtual void NotifyItemScrolledIntoView() override
	{
		// Notify that an item that came into view
		if ( TListTypeTraits<ItemType>::IsPtrValid( ItemToNotifyWhenInView ) )
		{
			ItemType NonNullItemToNotifyWhenInView = TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType( ItemToNotifyWhenInView );
			TSharedPtr<ITableRow> Widget = WidgetGenerator.GetWidgetForItem(NonNullItemToNotifyWhenInView);
			
			if (bNavigateOnScrollIntoView && Widget.IsValid())
			{
				NavigateToWidget(UserRequestingScrollIntoView, Widget->AsWidget());
			}
			bNavigateOnScrollIntoView = false;

			OnItemScrolledIntoView.ExecuteIfBound(NonNullItemToNotifyWhenInView, Widget);

			TListTypeTraits<ItemType>::ResetPtr(ItemToNotifyWhenInView);
		}
	}

	virtual float ScrollBy( const FGeometry& MyGeometry, float ScrollByAmountInSlateUnits, EAllowOverscroll InAllowOverscroll ) override
	{
		if (InAllowOverscroll == EAllowOverscroll::No)
		{
			//check if we are on the top of the list and want to scroll up
			if (ScrollOffset < KINDA_SMALL_NUMBER && ScrollByAmountInSlateUnits < 0)
			{
				return 0.0f;
			}

			//check if we are on the bottom of the list and want to scroll down
			if (bWasAtEndOfList && ScrollByAmountInSlateUnits > 0)
			{
				return 0.0f;
			}
		}

		float AbsScrollByAmount = FMath::Abs( ScrollByAmountInSlateUnits );
		int32 StartingItemIndex = (int32)ScrollOffset;
		double NewScrollOffset = ScrollOffset;

		const bool bWholeListVisible = ScrollOffset == 0 && bWasAtEndOfList;
		if ( InAllowOverscroll == EAllowOverscroll::Yes && Overscroll.ShouldApplyOverscroll( ScrollOffset == 0, bWasAtEndOfList, ScrollByAmountInSlateUnits ) )
		{
			const float UnclampedScrollDelta = FMath::Sign(ScrollByAmountInSlateUnits) * AbsScrollByAmount;				
			const float ActuallyScrolledBy = Overscroll.ScrollBy(MyGeometry, UnclampedScrollDelta);
			if (ActuallyScrolledBy != 0.0f)
			{
				this->RequestListRefresh();
			}
			return ActuallyScrolledBy;
		}
		else if (!bWholeListVisible)
		{
			// We know how far we want to scroll in SlateUnits, but we store scroll offset in "number of widgets".
			// Challenge: each widget can be a different height.
			// Strategy:
			//           Scroll "one widget's height" at a time until we've scrolled as far as the user asked us to.
			//           Generate widgets on demand so we can figure out how tall they are.

			const TArray<ItemType>* SourceItems = ItemsSource;
			if ( SourceItems != nullptr && SourceItems->Num() > 0 )
			{
				int ItemIndex = StartingItemIndex;
				const float LayoutScaleMultiplier = MyGeometry.GetAccumulatedLayoutTransform().GetScale();
				while( AbsScrollByAmount != 0 && ItemIndex < SourceItems->Num() && ItemIndex >= 0 )
				{
					const ItemType CurItem = (*SourceItems)[ ItemIndex ];
					TSharedPtr<ITableRow> RowWidget = WidgetGenerator.GetWidgetForItem( CurItem );
					if ( !RowWidget.IsValid() )
					{
						// We couldn't find an existing widgets, meaning that this data item was not visible before.
						// Make a new widget for it.
						RowWidget = this->GenerateNewWidget( CurItem );

						// It is useful to know the item's index that the widget was generated from.
						// Helps with even/odd coloring
						RowWidget->SetIndexInList(ItemIndex);

						// Let the item generator know that we encountered the current Item and associated Widget.
						WidgetGenerator.OnItemSeen( CurItem, RowWidget.ToSharedRef() );

						RowWidget->AsWidget()->SlatePrepass(LayoutScaleMultiplier);
					}

					if ( ScrollByAmountInSlateUnits > 0 )
					{
						FVector2D WidgetDesiredSize = RowWidget->AsWidget()->GetDesiredSize();
						const float RemainingHeight = WidgetDesiredSize.Y * ( 1.0 - FMath::Fractional( NewScrollOffset ) );

						if ( AbsScrollByAmount > RemainingHeight )
						{
							if ( ItemIndex != SourceItems->Num() )
							{
								AbsScrollByAmount -= RemainingHeight;
								NewScrollOffset = 1.0f + (int32)NewScrollOffset;
								++ItemIndex;
							}
							else
							{
								NewScrollOffset = SourceItems->Num();
								break;
							}
						} 
						else if ( AbsScrollByAmount == RemainingHeight )
						{
							NewScrollOffset = 1.0f + (int32)NewScrollOffset;
							break;
						}
						else
						{
							NewScrollOffset = (int32)NewScrollOffset + ( 1.0f - ( ( RemainingHeight - AbsScrollByAmount ) / WidgetDesiredSize.Y ) );
							break;
						}
					}
					else
					{
						FVector2D WidgetDesiredSize = RowWidget->AsWidget()->GetDesiredSize();

						float Fractional = FMath::Fractional( NewScrollOffset );
						if ( Fractional == 0 )
						{
							Fractional = 1.0f;
							--NewScrollOffset;
						}

						const float PrecedingHeight = WidgetDesiredSize.Y * Fractional;

						if ( AbsScrollByAmount > PrecedingHeight )
						{
							if ( ItemIndex != 0 )
							{
								AbsScrollByAmount -= PrecedingHeight;
								NewScrollOffset -= FMath::Fractional( NewScrollOffset );
								--ItemIndex;
							}
							else
							{
								NewScrollOffset = 0;
								break;
							}
						} 
						else if ( AbsScrollByAmount == PrecedingHeight )
						{
							NewScrollOffset -= FMath::Fractional( NewScrollOffset );
							break;
						}
						else
						{
							NewScrollOffset = (int32)NewScrollOffset + ( ( PrecedingHeight - AbsScrollByAmount ) / WidgetDesiredSize.Y );
							break;
						}
					}
				}
			}


			return ScrollTo( NewScrollOffset );
		}

		return 0;
	}

protected:

	/**
	 * Selects the specified item and scrolls it into view. If shift is held, it will be a range select.
	 * 
	 * @param ItemToSelect		The item that was selected by a keystroke
	 * @param InInputEvent	The key event that caused this selection
	 */
	virtual void NavigationSelect(const ItemType& ItemToSelect, const FInputEvent& InInputEvent)
	{
		const ESelectionMode::Type CurrentSelectionMode = SelectionMode.Get();

		if (CurrentSelectionMode != ESelectionMode::None)
		{
			// Must be set before signaling selection changes because sometimes new items will be selected that need to stomp this value
			SelectorItem = ItemToSelect;

			if (CurrentSelectionMode == ESelectionMode::Multi && (InInputEvent.IsShiftDown() || InInputEvent.IsControlDown()))
			{
				// Range select.
				if (InInputEvent.IsShiftDown())
				{
					// Holding control makes the range select bidirectional, where as it is normally unidirectional.
					if (!(InInputEvent.IsControlDown()))
					{
						this->Private_ClearSelection();
					}

					this->Private_SelectRangeFromCurrentTo(ItemToSelect);
				}

				this->Private_SignalSelectionChanged(ESelectInfo::OnNavigation);
			}
			else
			{
				// Single select.
				this->SetSelection(ItemToSelect, ESelectInfo::OnNavigation);;
			}

			// Always request scroll into view, otherwise partially visible items will be selected.
			TSharedPtr<ITableRow> WidgetForItem = this->WidgetGenerator.GetWidgetForItem(ItemToSelect);
			this->RequestScrollIntoView(ItemToSelect, InInputEvent.GetUserIndex(), true); 
		}
	}

protected:

	/** A widget generator component */
	FWidgetGenerator WidgetGenerator;

	/** Delegate to be invoked when the list needs to generate a new widget from a data item. */
	FOnGenerateRow OnGenerateRow;

	/** Assign this to get more diagnostics from the list view. */
	FOnItemToString_Debug OnItemToString_Debug;

	/** Invoked when the tree enters a bad state. */
	FOnTableViewBadState OnEnteredBadState;

	/**/
	FOnWidgetToBeRemoved OnRowReleased;

	/** Delegate to be invoked when an item has come into view after it was requested to come into view. */
	FOnItemScrolledIntoView OnItemScrolledIntoView;

	/** A set of selected data items */
	TSet< ItemType > SelectedItems;

	/** The item to manipulate selection for */
	NullableItemType SelectorItem;

	/** The item which was last manipulated; used as a start for shift-click selection */
	NullableItemType RangeSelectionStart;

	/** Pointer to the array of data items that we are observing */
	const TArray<ItemType>* ItemsSource;

	/** When not null, the list will try to scroll to this item on tick. */
	NullableItemType ItemToScrollIntoView;

	/** The user index requesting the item to be scrolled into view. */
	uint32 UserRequestingScrollIntoView;

	/** When set, the list will notify this item when it has been scrolled into view */
	NullableItemType ItemToNotifyWhenInView;

	/** Delegate to invoke when selection changes. */
	FOnSelectionChanged OnSelectionChanged;

	/** Called when the user clicks on an element int he list view with the left mouse button */
	FOnMouseButtonClick OnClick;

	/** Called when the user double-clicks on an element in the list view with the left mouse button */
	FOnMouseButtonDoubleClick OnDoubleClick;

	/** If true, the selection will be cleared if the user clicks in empty space (not on an item) */
	bool bClearSelectionOnClick;

	/** Should gamepad nav be supported */
	bool bHandleGamepadEvents;

	/** Should directional nav be supported */
	bool bHandleDirectionalNavigation;

private:

	bool bNavigateOnScrollIntoView;

	struct FGenerationPassGuard
	{
		FWidgetGenerator& Generator;
		FGenerationPassGuard( FWidgetGenerator& InGenerator )
			: Generator(InGenerator)
		{
			// Let the WidgetGenerator that we are starting a pass so that it can keep track of data items and widgets.
			Generator.OnBeginGenerationPass();
		}

		~FGenerationPassGuard()
		{
			// We have completed the generation pass. The WidgetGenerator will clean up unused Widgets when it goes out of scope.
			Generator.OnEndGenerationPass();
		}
	};
};

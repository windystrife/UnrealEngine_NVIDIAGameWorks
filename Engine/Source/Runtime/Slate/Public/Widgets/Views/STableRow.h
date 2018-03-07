// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "InputCoreTypes.h"
#include "Styling/SlateColor.h"
#include "Layout/Geometry.h"
#include "Input/Events.h"
#include "Input/DragAndDrop.h"
#include "Input/Reply.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Layout/Margin.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Framework/Views/ITypedTableView.h"
#include "Widgets/Views/STableViewBase.h"
#include "Rendering/DrawElements.h"
#include "Types/SlateStructs.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Framework/Views/TableViewTypeTraits.h"

template <typename ItemType> class SListView;

/**
 * Interface for table views to talk to their rows.
 */
class SLATE_API ITableRow
{
	public:
		/**
		 * @param InIndexInList  The index of the item for which this widget was generated
		 */
		virtual void SetIndexInList( int32 InIndexInList ) = 0;

		/** @return true if the corresponding item is expanded; false otherwise*/
		virtual bool IsItemExpanded() const = 0;

		/** Toggle the expansion of the item associated with this row */
		virtual void ToggleExpansion() = 0;

		/** @return how nested the item associated with this row when it is in a TreeView */
		virtual int32 GetIndentLevel() const = 0;

		/** @return Does this item have children? */
		virtual int32 DoesItemHaveChildren() const = 0;

		/** @return this table row as a widget */
		virtual TSharedRef<SWidget> AsWidget() = 0;

		/** @return the content of this table row */
		virtual TSharedPtr<SWidget> GetContent() = 0;

		/** Called when the expander arrow for this row is shift+clicked */
		virtual void Private_OnExpanderArrowShiftClicked() = 0;

		/** @return the size for the specified column name */
		virtual FVector2D GetRowSizeForColumn(const FName& InColumnName) const = 0;

	protected:
		/** Called to query the selection mode for the row */
		virtual ESelectionMode::Type GetSelectionMode() const = 0;
};


/**
 * Where we are going to drop relative to the target item.
 */
enum class EItemDropZone
{
	AboveItem,
	OntoItem,
	BelowItem
};

template <typename ItemType> class SListView;

DECLARE_DELEGATE_OneParam(FOnTableRowDragEnter, FDragDropEvent const&);
DECLARE_DELEGATE_OneParam(FOnTableRowDragLeave, FDragDropEvent const&);
DECLARE_DELEGATE_RetVal_OneParam(FReply, FOnTableRowDrop, FDragDropEvent const&);


/**
 * The ListView is populated by Selectable widgets.
 * A Selectable widget is away of the ListView containing it (OwnerWidget) and holds arbitrary Content (Content).
 * A Selectable works with its corresponding ListView to provide selection functionality.
 */
template<typename ItemType>
class STableRow : public ITableRow, public SBorder
{
	static_assert(TIsValidListItem<ItemType>::Value, "Item type T must be UObjectBase*, TSharedRef<>, or TSharedPtr<>.");

public:
	/** Delegate signature for querying whether this FDragDropEvent will be handled by the drop target of type ItemType. */
	DECLARE_DELEGATE_RetVal_ThreeParams(TOptional<EItemDropZone>, FOnCanAcceptDrop, const FDragDropEvent&, EItemDropZone, ItemType);
	/** Delegate signature for handling the drop of FDragDropEvent onto target of type ItemType */
	DECLARE_DELEGATE_RetVal_ThreeParams(FReply, FOnAcceptDrop, const FDragDropEvent&, EItemDropZone, ItemType);

public:

	SLATE_BEGIN_ARGS( STableRow< ItemType > )
		: _Style( &FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row") )
		, _ExpanderStyleSet( &FCoreStyle::Get() )
		, _Padding( FMargin(0) )
		, _ShowSelection( true )
		, _Content()
		{}
	
		SLATE_STYLE_ARGUMENT( FTableRowStyle, Style )
		SLATE_ARGUMENT(const ISlateStyle*, ExpanderStyleSet)

		// High Level DragAndDrop

		/**
		 * Handle this event to determine whether a drag and drop operation can be executed on top of the target row widget.
		 * Most commonly, this is used for previewing re-ordering and re-organization operations in lists or trees.
		 * e.g. A user is dragging one item into a different spot in the list or tree.
		 *      This delegate will be called to figure out if we should give visual feedback on whether an item will 
		 *      successfully drop into the list.
		 */
		SLATE_EVENT( FOnCanAcceptDrop, OnCanAcceptDrop )

		/**
		 * Perform a drop operation onto the target row widget
		 * Most commonly used for executing a re-ordering and re-organization operations in lists or trees.
		 * e.g. A user was dragging one item into a different spot in the list; they just dropped it.
		 *      This is our chance to handle the drop by reordering items and calling for a list refresh.
		 */
		SLATE_EVENT( FOnAcceptDrop,    OnAcceptDrop )

		// Low level DragAndDrop
		SLATE_EVENT( FOnDragDetected,      OnDragDetected )
		SLATE_EVENT( FOnTableRowDragEnter, OnDragEnter )
		SLATE_EVENT( FOnTableRowDragLeave, OnDragLeave )
		SLATE_EVENT( FOnTableRowDrop,      OnDrop )

		SLATE_ATTRIBUTE( FMargin, Padding )
	
		SLATE_ARGUMENT( bool, ShowSelection )
		
		SLATE_DEFAULT_SLOT( typename STableRow<ItemType>::FArguments, Content )

	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct(const typename STableRow<ItemType>::FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		/** Note: Please initialize any state in ConstructInternal, not here. This is because STableRow derivatives call ConstructInternal directly to avoid constructing children. **/

		ConstructInternal(InArgs, InOwnerTableView);

		ConstructChildren(
			InOwnerTableView->TableViewMode,
			InArgs._Padding,
			InArgs._Content.Widget
		);
	}

	virtual void ConstructChildren( ETableViewMode::Type InOwnerTableMode, const TAttribute<FMargin>& InPadding, const TSharedRef<SWidget>& InContent )
	{
		this->Content = InContent;
		InnerContentSlot = nullptr;

		if ( InOwnerTableMode == ETableViewMode::List || InOwnerTableMode == ETableViewMode::Tile )
		{
			// -- Row is in a ListView or the user --
			FSimpleSlot* InnerContentSlotNativePtr = nullptr;

			// We just need to hold on to this row's content.
			this->ChildSlot
			.Expose( InnerContentSlotNativePtr )
			.Padding( InPadding )
			[
				InContent
			];

			InnerContentSlot = InnerContentSlotNativePtr;
		}
		else
		{
			// -- Row is for TreeView --
			SHorizontalBox::FSlot* InnerContentSlotNativePtr = nullptr;

			// Rows in a TreeView need an expander button and some indentation
			this->ChildSlot
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Fill)
				[
					SNew(SExpanderArrow, SharedThis(this) )
					.StyleSet(ExpanderStyleSet)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1)
				.Expose( InnerContentSlotNativePtr )
				.Padding( InPadding )
				[
					InContent
				]
			];

			InnerContentSlot = InnerContentSlotNativePtr;
		}
	}

	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override
	{
		TSharedRef< ITypedTableView<ItemType> > OwnerWidget = OwnerTablePtr.Pin().ToSharedRef();
		const bool bIsActive = OwnerWidget->AsWidget()->HasKeyboardFocus();
		const ItemType* MyItem = OwnerWidget->Private_ItemFromWidget( this );
		if ( bIsActive && OwnerWidget->Private_UsesSelectorFocus() && OwnerWidget->Private_HasSelectorFocus( *MyItem ) )
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(),
				&Style->SelectorFocusedBrush,
				ESlateDrawEffect::None,
				Style->SelectorFocusedBrush.GetTint( InWidgetStyle ) * InWidgetStyle.GetColorAndOpacityTint()
				);
		}

		LayerId = SBorder::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

		if (ItemDropZone.IsSet())
		{
			// Draw feedback for user dropping an item above, below, or onto a row.
			const FSlateBrush* DropIndicatorBrush = [&]()
			{
				switch (ItemDropZone.GetValue())
				{
					case EItemDropZone::AboveItem: return &Style->DropIndicator_Above; break;
					default:
					case EItemDropZone::OntoItem: return &Style->DropIndicator_Onto; break;
					case EItemDropZone::BelowItem: return &Style->DropIndicator_Below; break;
				};
			}();

			FSlateDrawElement::MakeBox
			(
				OutDrawElements,
				LayerId++,
				AllottedGeometry.ToPaintGeometry(),
				DropIndicatorBrush,
				ESlateDrawEffect::None,
				DropIndicatorBrush->GetTint(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
			);
		}

		return LayerId;
	}

	/**
	 * Called when a mouse button is double clicked.  Override this in derived classes.
	 *
	 * @param  InMyGeometry  Widget geometry.
	 * @param  InMouseEvent  Mouse button event.
	 * @return  Returns whether the event was handled, along with other possible actions.
	 */
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override
	{
		if( InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
		{
			TSharedRef< ITypedTableView<ItemType> > OwnerWidget = OwnerTablePtr.Pin().ToSharedRef();

			// Only one item can be double-clicked
			const ItemType* MyItem = OwnerWidget->Private_ItemFromWidget( this );

			// If we're configured to route double-click messages to the owner of the table, then
			// do that here.  Otherwise, we'll toggle expansion.
			const bool bWasHandled = OwnerWidget->Private_OnItemDoubleClicked( *MyItem );
			if( !bWasHandled )
			{
				ToggleExpansion();
			}

			return FReply::Handled();
		}

		return FReply::Unhandled();
	}

	/**
	 * See SWidget::OnMouseButtonDown
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event.
	 * @param MouseEvent Information about the input event.
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */	
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override
	{
		TSharedPtr< ITypedTableView<ItemType> > OwnerWidget = OwnerTablePtr.Pin();
		ChangedSelectionOnMouseDown = false;

		check(OwnerWidget.IsValid());

		if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
		{
			switch( GetSelectionMode() )
			{
			case ESelectionMode::Single:
				{
					const ItemType* MyItem = OwnerWidget->Private_ItemFromWidget( this );
					const bool bIsSelected = OwnerWidget->Private_IsItemSelected( *MyItem );

					// Select the item under the cursor
					if( !bIsSelected )
					{
						OwnerWidget->Private_ClearSelection();
						OwnerWidget->Private_SetItemSelection( *MyItem, true, true );
						ChangedSelectionOnMouseDown = true;
					}
					
					return FReply::Handled()
						.DetectDrag( SharedThis(this), EKeys::LeftMouseButton )
						.SetUserFocus(OwnerWidget->AsWidget(), EFocusCause::Mouse)
						.CaptureMouse( SharedThis(this) );
				}
				break;

			case ESelectionMode::SingleToggle:
				{
					const ItemType* MyItem = OwnerWidget->Private_ItemFromWidget( this );
					const bool bIsSelected = OwnerWidget->Private_IsItemSelected( *MyItem );

					if( !bIsSelected )
					{
						OwnerWidget->Private_ClearSelection();
						OwnerWidget->Private_SetItemSelection( *MyItem, true, true );
						ChangedSelectionOnMouseDown = true;
					}

					return FReply::Handled()
						.DetectDrag( SharedThis(this), EKeys::LeftMouseButton )
						.SetUserFocus(OwnerWidget->AsWidget(), EFocusCause::Mouse)
						.CaptureMouse( SharedThis(this) );
				}
				break;

			case ESelectionMode::Multi:
				{
					const ItemType* MyItem = OwnerWidget->Private_ItemFromWidget( this );
					const bool bIsSelected = OwnerWidget->Private_IsItemSelected( *MyItem );

					check(MyItem != nullptr);

					if ( MouseEvent.IsControlDown() )
					{
						OwnerWidget->Private_SetItemSelection( *MyItem, !bIsSelected, true );
						ChangedSelectionOnMouseDown = true;
					}
					else if ( MouseEvent.IsShiftDown() )
					{
						OwnerWidget->Private_SelectRangeFromCurrentTo( *MyItem );
						ChangedSelectionOnMouseDown = true;
					}
					else if ( !bIsSelected )
					{
						OwnerWidget->Private_ClearSelection();
						OwnerWidget->Private_SetItemSelection( *MyItem, true, true );
						ChangedSelectionOnMouseDown = true;
					}

					return FReply::Handled()
						.DetectDrag( SharedThis(this), EKeys::LeftMouseButton )
						.SetUserFocus(OwnerWidget->AsWidget(), EFocusCause::Mouse)
						.CaptureMouse( SharedThis(this) );
				}
				break;
			}
		}

		return FReply::Unhandled();
	}
	
	/**
	 * See SWidget::OnMouseButtonUp
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event.
	 * @param MouseEvent Information about the input event.
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override
	{
		TSharedPtr< ITypedTableView<ItemType> > OwnerWidget = OwnerTablePtr.Pin();
		check(OwnerWidget.IsValid());

		// Requires #include "Widgets/Views/STableViewBase.h"
		TSharedRef< STableViewBase > OwnerTableViewBase = StaticCastSharedPtr< SListView<ItemType> >( OwnerWidget ).ToSharedRef();

		if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
		{
			FReply Reply = FReply::Unhandled().ReleaseMouseCapture();

			if ( ChangedSelectionOnMouseDown )
			{
				Reply = FReply::Handled().ReleaseMouseCapture();
			}

			const bool bIsUnderMouse = MyGeometry.IsUnderLocation(MouseEvent.GetScreenSpacePosition());
			if ( HasMouseCapture() )
			{
				if ( bIsUnderMouse )
				{
					switch( GetSelectionMode() )
					{
					case ESelectionMode::SingleToggle:
						{
							if ( !ChangedSelectionOnMouseDown )
							{
								const ItemType* MyItem = OwnerWidget->Private_ItemFromWidget( this );
								const bool bIsSelected = OwnerWidget->Private_IsItemSelected( *MyItem );

								OwnerWidget->Private_ClearSelection();
								OwnerWidget->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);
							}

							Reply = FReply::Handled().ReleaseMouseCapture();
						}
						break;

					case ESelectionMode::Multi:
						{
							if ( !ChangedSelectionOnMouseDown && !MouseEvent.IsControlDown() && !MouseEvent.IsShiftDown() )
							{
								const ItemType* MyItem = OwnerWidget->Private_ItemFromWidget( this );
								check(MyItem != nullptr);

								const bool bIsSelected = OwnerWidget->Private_IsItemSelected( *MyItem );
								if ( bIsSelected && OwnerWidget->Private_GetNumSelectedItems() > 1 )
								{
									// We are mousing up on a previous selected item;
									// deselect everything but this item.

									OwnerWidget->Private_ClearSelection();
									OwnerWidget->Private_SetItemSelection( *MyItem, true, true );
									OwnerWidget->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);

									Reply = FReply::Handled().ReleaseMouseCapture();
								}
							}
						}
						break;
					}
				}

				const ItemType* MyItem = OwnerWidget->Private_ItemFromWidget(this);
				if (MyItem && OwnerWidget->Private_OnItemClicked(*MyItem))
				{
					Reply = FReply::Handled().ReleaseMouseCapture();
				}

				if ( ChangedSelectionOnMouseDown )
				{
					OwnerWidget->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);
				}

				return Reply;
			}
		}
		else if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && !OwnerTableViewBase->IsRightClickScrolling() )
		{
			// Handle selection of items when releasing the right mouse button, but only if the user isn't actively
			// scrolling the view by holding down the right mouse button.

			switch( GetSelectionMode() )
			{
			case ESelectionMode::Single:
			case ESelectionMode::SingleToggle:
			case ESelectionMode::Multi:
				{
					// Only one item can be selected at a time
					const ItemType* MyItem = OwnerWidget->Private_ItemFromWidget( this );
					const bool bIsSelected = OwnerWidget->Private_IsItemSelected( *MyItem );

					// Select the item under the cursor
					if( !bIsSelected )
					{
						OwnerWidget->Private_ClearSelection();
						OwnerWidget->Private_SetItemSelection( *MyItem, true, true );
						OwnerWidget->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);
					}

					OwnerWidget->Private_OnItemRightClicked( *MyItem, MouseEvent );

					return FReply::Handled();
				}
				break;
			}
		}

		return FReply::Unhandled();
	}

	virtual FReply OnTouchStarted( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent ) override
	{
		bProcessingSelectionTouch = true;

		return
			FReply::Handled()
			// Drag detect because if this tap turns into a drag, we stop processing
			// the selection touch.
			.DetectDrag( SharedThis(this), EKeys::LeftMouseButton );
	}

	virtual FReply OnTouchEnded( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent ) override
	{
		if ( bProcessingSelectionTouch )
		{
			bProcessingSelectionTouch = false;
			const TSharedPtr< ITypedTableView<ItemType> > OwnerWidget = OwnerTablePtr.Pin();
			const ItemType* MyItem = OwnerWidget->Private_ItemFromWidget( this );

			switch( GetSelectionMode() )
			{
				default:
				case ESelectionMode::None:
					return FReply::Unhandled();
				break;

				case ESelectionMode::Single:
				{
					OwnerWidget->Private_ClearSelection();
					OwnerWidget->Private_SetItemSelection( *MyItem, true, true );
					OwnerWidget->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);
					return FReply::Handled();
				}
				break;

				case ESelectionMode::SingleToggle:
				{
					const bool bShouldBecomeSelected = !OwnerWidget->Private_IsItemSelected(*MyItem);
					OwnerWidget->Private_ClearSelection();
					OwnerWidget->Private_SetItemSelection( *MyItem, bShouldBecomeSelected, true );
					OwnerWidget->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);
				}
				break;

				case ESelectionMode::Multi:
				{
					const bool bShouldBecomeSelected = !OwnerWidget->Private_IsItemSelected(*MyItem);
					OwnerWidget->Private_SetItemSelection( *MyItem, bShouldBecomeSelected, true );
					OwnerWidget->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);
				}
				break;
			}
			
			return FReply::Handled();
		}
		else
		{
			return FReply::Unhandled();
		}
	}

	virtual FReply OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override
	{
		if (bProcessingSelectionTouch)
		{
			// With touch input, dragging scrolls the list while selection requires a tap.
			// If we are processing a touch and it turned into a drag; pass it on to the 
			bProcessingSelectionTouch = false;
			return FReply::Handled().CaptureMouse( OwnerTablePtr.Pin()->AsWidget() );
		}
		else if ( HasMouseCapture() && ChangedSelectionOnMouseDown )
		{
			TSharedPtr< ITypedTableView<ItemType> > OwnerWidget = OwnerTablePtr.Pin();
			OwnerWidget->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);
		}

		if (OnDragDetected_Handler.IsBound())
		{
			return OnDragDetected_Handler.Execute( MyGeometry, MouseEvent );
		}
		else
		{
			return FReply::Unhandled();
		}
	}

	virtual void OnDragEnter(FGeometry const& MyGeometry, FDragDropEvent const& DragDropEvent) override
	{
		if (OnDragEnter_Handler.IsBound())
		{
			OnDragEnter_Handler.Execute(DragDropEvent);
		}
	}

	virtual void OnDragLeave(FDragDropEvent const& DragDropEvent) override
	{
		ItemDropZone = TOptional<EItemDropZone>();

		if (OnDragLeave_Handler.IsBound())
		{
			OnDragLeave_Handler.Execute(DragDropEvent);
		}
	}

	/** @return the zone (above, onto, below) based on where the user is hovering over within the row */
	EItemDropZone ZoneFromPointerPosition(FVector2D LocalPointerPos, float RowHeight)
	{
		const float VecticalZoneBoundarySu = FMath::Clamp(RowHeight * 0.25f, 3.0f, 10.0f);
		if (LocalPointerPos.Y < VecticalZoneBoundarySu)
		{
			return EItemDropZone::AboveItem;
		}
		else if (LocalPointerPos.Y > RowHeight - VecticalZoneBoundarySu)
		{
			return EItemDropZone::BelowItem;
		}
		else
		{
			return EItemDropZone::OntoItem;
		}
	}

	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
	{
		if ( OnCanAcceptDrop.IsBound() )
		{
			const FVector2D LocalPointerPos = MyGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition());
			const EItemDropZone ItemHoverZone = ZoneFromPointerPosition(LocalPointerPos, MyGeometry.GetLocalSize().Y);

			ItemDropZone = [&]()
			{
				TSharedPtr< ITypedTableView<ItemType> > OwnerWidget = OwnerTablePtr.Pin();
				const ItemType* MyItem = OwnerWidget->Private_ItemFromWidget(this);
				return OnCanAcceptDrop.Execute(DragDropEvent, ItemHoverZone, *MyItem);
			}();

			return FReply::Handled();
		}
		else
		{
			return FReply::Unhandled();
		}

	}

	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
	{
		const FReply Reply = [&]()
		{
			if (OnAcceptDrop.IsBound())
			{
				const TSharedRef< ITypedTableView<ItemType> > OwnerWidget = OwnerTablePtr.Pin().ToSharedRef();

				// A drop finishes the drag/drop operation, so we are no longer providing any feedback.
				ItemDropZone = TOptional<EItemDropZone>();

				// Find item associated with this widget.
				const ItemType* MyItem = OwnerWidget->Private_ItemFromWidget(this);
				
				// Which physical drop zone is the drop about to be performed onto?
				const FVector2D LocalPointerPos = MyGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition());
				const EItemDropZone HoveredZone = ZoneFromPointerPosition(LocalPointerPos, MyGeometry.GetLocalSize().Y);

				// The row gets final say over which zone to drop onto regardless of physical location.
				const TOptional<EItemDropZone> ReportedZone = OnCanAcceptDrop.IsBound()
					? OnCanAcceptDrop.Execute(DragDropEvent, HoveredZone, *MyItem)
					: HoveredZone;

				if (ReportedZone.IsSet())
				{
					FReply DropReply = OnAcceptDrop.Execute(DragDropEvent, ReportedZone.GetValue(), *MyItem);
					if (DropReply.IsEventHandled())
					{
						// Expand the drop target just in case, so that what we dropped is visible.
						OwnerWidget->Private_SetItemExpansion(*MyItem, true);
					}

					return DropReply;
				}
			}

			return FReply::Unhandled();
		}();

		// @todo slate : Made obsolete by OnAcceptDrop. Get rid of this.
		if ( !Reply.IsEventHandled() && OnDrop_Handler.IsBound() )
		{
			return OnDrop_Handler.Execute(DragDropEvent);
		}

		return Reply;
	}

	virtual void SetIndexInList( int32 InIndexInList ) override
	{
		IndexInList = InIndexInList;
	}

	virtual bool IsItemExpanded() const override
	{
		TSharedPtr< ITypedTableView<ItemType> > OwnerWidget = OwnerTablePtr.Pin();
		const ItemType* MyItem = OwnerWidget->Private_ItemFromWidget( this );
		const bool bIsItemExpanded = OwnerWidget->Private_IsItemExpanded( *MyItem );
		return bIsItemExpanded;
	}

	virtual void ToggleExpansion() override
	{
		TSharedPtr< ITypedTableView<ItemType> > OwnerWidget = OwnerTablePtr.Pin();

		const bool bItemHasChildren = OwnerWidget->Private_DoesItemHaveChildren( IndexInList );
		// Nothing to expand if row being clicked on doesn't have children
		if( bItemHasChildren )
		{
			ItemType MyItem = *(OwnerWidget->Private_ItemFromWidget( this ));
			const bool IsItemExpanded = bItemHasChildren && OwnerWidget->Private_IsItemExpanded( MyItem );
			OwnerWidget->Private_SetItemExpansion( MyItem, !IsItemExpanded );
		}
	}

	virtual int32 GetIndentLevel() const override
	{
		return OwnerTablePtr.Pin()->Private_GetNestingDepth( IndexInList );
	}

	virtual int32 DoesItemHaveChildren() const override
	{
		return OwnerTablePtr.Pin()->Private_DoesItemHaveChildren( IndexInList );
	}

	virtual TSharedRef<SWidget> AsWidget() override
	{
		return SharedThis(this);
	}

	/** Set the entire content of this row, replacing any extra UI (such as the expander arrows for tree views) that was added by ConstructChildren */
	virtual void SetRowContent(TSharedRef< SWidget > InContent)
	{
		this->Content = InContent;
		InnerContentSlot = nullptr;
		SBorder::SetContent(InContent);
	}

	/** Set the inner content of this row, preserving any extra UI (such as the expander arrows for tree views) that was added by ConstructChildren */
	virtual void SetContent(TSharedRef< SWidget > InContent) override
	{
		this->Content = InContent;

		if (InnerContentSlot)
		{
			InnerContentSlot->AttachWidget(InContent);
		}
		else
		{
			SBorder::SetContent(InContent);
		}
	}

	/** Get the inner content of this row */
	virtual TSharedPtr<SWidget> GetContent() override
	{
		if ( this->Content.IsValid() )
		{
			return this->Content.Pin();
		}
		else
		{
			return TSharedPtr<SWidget>();
		}
	}

	virtual void Private_OnExpanderArrowShiftClicked() override
	{
		TSharedPtr< ITypedTableView<ItemType> > OwnerWidget = OwnerTablePtr.Pin();

		const bool bItemHasChildren = OwnerWidget->Private_DoesItemHaveChildren( IndexInList );
		// Nothing to expand if row being clicked on doesn't have children
		if( bItemHasChildren )
		{
			ItemType MyItem = *(OwnerWidget->Private_ItemFromWidget( this ));
			const bool IsItemExpanded = bItemHasChildren && OwnerWidget->Private_IsItemExpanded( MyItem );
			OwnerWidget->Private_OnExpanderArrowShiftClicked( MyItem, !IsItemExpanded );
		}
	}

	/** @return The border to be drawn around this list item */
	const FSlateBrush* GetBorder() const 
	{
		TSharedPtr< ITypedTableView<ItemType> > OwnerWidget = OwnerTablePtr.Pin();

		const bool bIsActive = OwnerWidget->AsWidget()->HasKeyboardFocus();

		static FName GenericWhiteBoxBrush("GenericWhiteBox");

		// @todo: Slate Style - make this part of the widget style
		const FSlateBrush* WhiteBox = FCoreStyle::Get().GetBrush(GenericWhiteBoxBrush);

		const ItemType* MyItem = OwnerWidget->Private_ItemFromWidget( this );

		check(MyItem);

		const bool bIsSelected = OwnerWidget->Private_IsItemSelected( *MyItem );

		if (bIsSelected && bShowSelection)
		{
			if( bIsActive )
			{
				return IsHovered()
					? &Style->ActiveHoveredBrush
					: &Style->ActiveBrush;
			}
			else
			{
				return IsHovered()
					? &Style->InactiveHoveredBrush
					: &Style->InactiveBrush;
			}
		}
		else
		{
			// Add a slightly lighter background for even rows
			const bool bAllowSelection = GetSelectionMode() != ESelectionMode::None;
			if( IndexInList % 2 == 0 )
			{
				return ( IsHovered() && bAllowSelection )
					? &Style->EvenRowBackgroundHoveredBrush
					: &Style->EvenRowBackgroundBrush;

			}
			else
			{
				return ( IsHovered() && bAllowSelection )
					? &Style->OddRowBackgroundHoveredBrush
					: &Style->OddRowBackgroundBrush;

			}
		}
	}

	/** 
	 * Callback to determine if the row is selected singularly and has keyboard focus or not
	 *
	 * @return		true if selected by owning widget.
	 */
	bool IsSelectedExclusively() const
	{
		TSharedPtr< ITypedTableView< ItemType > > OwnerWidget = OwnerTablePtr.Pin();

		if(!OwnerWidget->AsWidget()->HasKeyboardFocus() || OwnerWidget->Private_GetNumSelectedItems() > 1)
		{
			return false;
		}

		const ItemType* MyItem = OwnerWidget->Private_ItemFromWidget( this );
		return OwnerWidget->Private_IsItemSelected( *MyItem );
	}

	/**
	 * Callback to determine if the row is selected or not
	 *
	 * @return		true if selected by owning widget.
	 */
	bool IsSelected() const
	{
		TSharedPtr< ITypedTableView< ItemType > > OwnerWidget = OwnerTablePtr.Pin();

		const ItemType* MyItem = OwnerWidget->Private_ItemFromWidget(this);
		return OwnerWidget->Private_IsItemSelected(*MyItem);
	}

	/** By default, this function does nothing, it should be implemented by derived class */
	virtual FVector2D GetRowSizeForColumn(const FName& InColumnName) const override
	{
		return FVector2D::ZeroVector;
	}

	/** Protected constructor; SWidgets should only be instantiated via declarative syntax. */
	STableRow()
		: IndexInList(0)
		, bShowSelection(true)
	{ }

protected:

	/**
	 * An internal method to construct and setup this row widget (purposely avoids child construction). 
	 * Split out from Construct() so that sub-classes can invoke super construction without invoking 
	 * ConstructChildren() (sub-classes may want to constuct their own children in their own special way).
	 * 
	 * @param  InArgs			Declaration data for this widget.
	 * @param  InOwnerTableView	The table that this row belongs to.
	 */
	void ConstructInternal(FArguments const& InArgs, TSharedRef<STableViewBase> const& InOwnerTableView)
	{
		bProcessingSelectionTouch = false;

		check(InArgs._Style);
		Style = InArgs._Style;

		check(InArgs._ExpanderStyleSet);
		ExpanderStyleSet = InArgs._ExpanderStyleSet;

		this->BorderImage = TAttribute<const FSlateBrush*>( this, &STableRow::GetBorder );

		this->ForegroundColor = TAttribute<FSlateColor>( this, &STableRow::GetForegroundBasedOnSelection );

		this->OnCanAcceptDrop = InArgs._OnCanAcceptDrop;
		this->OnAcceptDrop = InArgs._OnAcceptDrop;

		this->OnDragDetected_Handler = InArgs._OnDragDetected;
		this->OnDragEnter_Handler = InArgs._OnDragEnter;
		this->OnDragLeave_Handler = InArgs._OnDragLeave;
		this->OnDrop_Handler = InArgs._OnDrop;
		
		this->SetOwnerTableView( InOwnerTableView );

		this->bShowSelection = InArgs._ShowSelection;
	}

	void SetOwnerTableView( TSharedPtr<STableViewBase> OwnerTableView )
	{
		// We want to cast to a ITypedTableView.
		// We cast to a SListView<ItemType> because C++ doesn't know that
		// being a STableView implies being a ITypedTableView.
		// See SListView.
		this->OwnerTablePtr = StaticCastSharedPtr< SListView<ItemType> >(OwnerTableView);
	}

	FSlateColor GetForegroundBasedOnSelection() const
	{
		const TSharedPtr< ITypedTableView<ItemType> > OwnerWidget = OwnerTablePtr.Pin();
		const FSlateColor& NonSelectedForeground = Style->TextColor; 
		const FSlateColor& SelectedForeground = Style->SelectedTextColor;

		if ( !bShowSelection || !OwnerWidget.IsValid() )
		{
			return NonSelectedForeground;
		}

		const ItemType* MyItem = OwnerWidget->Private_ItemFromWidget( this );
		const bool bIsSelected = OwnerWidget->Private_IsItemSelected( *MyItem );

		return bIsSelected
			? SelectedForeground
			: NonSelectedForeground;
	}

	virtual ESelectionMode::Type GetSelectionMode() const override
	{
		const TSharedPtr< ITypedTableView<ItemType> > OwnerWidget = OwnerTablePtr.Pin();
		return OwnerWidget->Private_GetSelectionMode();
	}

protected:

	/** The list that owns this Selectable */
	TWeakPtr< ITypedTableView<ItemType> > OwnerTablePtr;

	/** Index of the corresponding data item in the list */
	int32 IndexInList;

	/** Whether or not to visually show that this row is selected */
	bool bShowSelection;

	/** Style used to draw this table row */
	const FTableRowStyle* Style;

	/** The slate style to use with the expander */
	const ISlateStyle* ExpanderStyleSet;

	/** @see STableRow's OnCanAcceptDrop event */
	FOnCanAcceptDrop OnCanAcceptDrop;

	/** @see STableRow's OnAcceptDrop event */
	FOnAcceptDrop OnAcceptDrop;

	/** Are we currently dragging/dropping over this item? */
	TOptional<EItemDropZone> ItemDropZone;

	/** Delegate triggered when a user starts to drag a list item */
	FOnDragDetected OnDragDetected_Handler;

	/** Delegate triggered when a user's drag enters the bounds of this list item */
	FOnTableRowDragEnter OnDragEnter_Handler;

	/** Delegate triggered when a user's drag leaves the bounds of this list item */
	FOnTableRowDragLeave OnDragLeave_Handler;

	/** Delegate triggered when a user's drag is dropped in the bounds of this list item */
	FOnTableRowDrop OnDrop_Handler;

	/** The slot that contains the inner content for this row. If this is set, SetContent populates this slot with the new content rather than replace the content wholesale */
	FSlotBase* InnerContentSlot;

	/** The widget in the content slot for this row */
	TWeakPtr<SWidget> Content;

	bool ChangedSelectionOnMouseDown;

	/** Did the current a touch interaction start in this item?*/
	bool bProcessingSelectionTouch;
};


template<typename ItemType>
class SMultiColumnTableRow : public STableRow<ItemType>
{
public:

	/**
	 * Users of SMultiColumnTableRow would usually some piece of data associated with it.
	 * The type of this data is ItemType; it's the stuff that your TableView (i.e. List or Tree) is visualizing.
	 * The ColumnName tells you which column of the TableView we need to make a widget for.
	 * Make a widget and return it.
	 *
	 * @param ColumnName    A unique ID for a column in this TableView; see SHeaderRow::FColumn for more info.
	 * @return a widget to represent the contents of a cell in this row of a TableView. 
	 */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& InColumnName ) = 0;

	/** Use this to construct the superclass; e.g. FSuperRowType::Construct( FTableRowArgs(), OwnerTableView ) */
	typedef SMultiColumnTableRow< ItemType > FSuperRowType;

	/** Use this to construct the superclass; e.g. FSuperRowType::Construct( FTableRowArgs(), OwnerTableView ) */
	typedef typename STableRow<ItemType>::FArguments FTableRowArgs;

protected:
	void Construct(const FTableRowArgs& InArgs, const TSharedRef<STableViewBase>& OwnerTableView)
	{
		STableRow<ItemType>::Construct(
			FTableRowArgs()
			.Style(InArgs._Style)
			.ExpanderStyleSet(InArgs._ExpanderStyleSet)
			.Padding(InArgs._Padding)
			.ShowSelection(InArgs._ShowSelection)
			.OnCanAcceptDrop(InArgs._OnCanAcceptDrop)
			.OnAcceptDrop(InArgs._OnAcceptDrop)
			.OnDragDetected(InArgs._OnDragDetected)
			.OnDragEnter(InArgs._OnDragEnter)
			.OnDragLeave(InArgs._OnDragLeave)
			.OnDrop(InArgs._OnDrop)
			.Content()
			[
				SAssignNew( Box, SHorizontalBox )
			]
		
			, OwnerTableView );

		// Sign up for notifications about changes to the HeaderRow
		TSharedPtr< SHeaderRow > HeaderRow = OwnerTableView->GetHeaderRow();
		check( HeaderRow.IsValid() );
		HeaderRow->OnColumnsChanged()->AddSP( this, &SMultiColumnTableRow<ItemType>::GenerateColumns );

		// Populate the row with user-generated content
		this->GenerateColumns( HeaderRow.ToSharedRef() );
	}

	virtual void ConstructChildren( ETableViewMode::Type InOwnerTableMode, const TAttribute<FMargin>& InPadding, const TSharedRef<SWidget>& InContent ) override
	{
		STableRow<ItemType>::Content = InContent;

		// MultiColumnRows let the user decide which column should contain the expander/indenter item.
		this->ChildSlot
		.Padding( InPadding )
		[
			InContent
		];
	}

	void GenerateColumns( const TSharedRef<SHeaderRow>& InColumnHeaders )
	{
		Box->ClearChildren();
		const TIndirectArray<SHeaderRow::FColumn>& Columns = InColumnHeaders->GetColumns();
		const int32 NumColumns = Columns.Num();
		TMap< FName, TSharedRef< SWidget > > NewColumnIdToSlotContents;

		for( int32 ColumnIndex = 0; ColumnIndex < NumColumns; ++ColumnIndex )
		{
			const SHeaderRow::FColumn& Column = Columns[ColumnIndex];
			if (Column.ShouldGenerateWidget.Get(true))
			{
				TSharedRef< SWidget >* ExistingWidget = ColumnIdToSlotContents.Find(Column.ColumnId);
				TSharedRef< SWidget > CellContents = SNullWidget::NullWidget;
				if (ExistingWidget != nullptr)
				{
					CellContents = *ExistingWidget;
				}
				else
				{
					CellContents = GenerateWidgetForColumn(Column.ColumnId);
				}

				if ( CellContents != SNullWidget::NullWidget )
				{
					CellContents->SetClipping(EWidgetClipping::OnDemand);
				}

				switch (Column.SizeRule)
				{
				case EColumnSizeMode::Fill:
				{
					TAttribute<float> WidthBinding;
					WidthBinding.BindRaw(&Column, &SHeaderRow::FColumn::GetWidth);

					SHorizontalBox::FSlot& NewSlot = Box->AddSlot()
					.HAlign(Column.CellHAlignment)
					.VAlign(Column.CellVAlignment)
					.FillWidth(WidthBinding)
					[
						CellContents
					];
				}
				break;

				case EColumnSizeMode::Fixed:
				{
					Box->AddSlot()
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(Column.Width.Get())
						.HAlign(Column.CellHAlignment)
						.VAlign(Column.CellVAlignment)
						.Clipping(EWidgetClipping::OnDemand)
						[
							CellContents
						]
					];
				}
				break;

				case EColumnSizeMode::Manual:
				{
					auto GetColumnWidthAsOptionalSize = [&Column]() -> FOptionalSize
					{
						const float DesiredWidth = Column.GetWidth();
						return FOptionalSize(DesiredWidth);
					};

					TAttribute<FOptionalSize> WidthBinding;
					WidthBinding.Bind(TAttribute<FOptionalSize>::FGetter::CreateLambda(GetColumnWidthAsOptionalSize));

					Box->AddSlot()
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(WidthBinding)
						.HAlign(Column.CellHAlignment)
						.VAlign(Column.CellVAlignment)
						.Clipping(EWidgetClipping::OnDemand)
						[
							CellContents
						]
					];
				}
				break;

				default:
					break;
				}

				NewColumnIdToSlotContents.Add(Column.ColumnId, CellContents);
			}
		}

		ColumnIdToSlotContents = NewColumnIdToSlotContents;
	}

	void ClearCellCache()
	{
		ColumnIdToSlotContents.Empty();
	}

	const TSharedRef<SWidget>* GetWidgetFromColumnId(const FName& ColumnId) const
	{
		return ColumnIdToSlotContents.Find(ColumnId);
	}

private:
	
	TSharedPtr<SHorizontalBox> Box;
	TMap< FName, TSharedRef< SWidget > > ColumnIdToSlotContents;
};

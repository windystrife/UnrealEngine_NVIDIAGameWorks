// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "Types/SlateStructs.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SlotBase.h"
#include "Widgets/SWidget.h"
#include "Layout/Children.h"
#include "Widgets/SPanel.h"
#include "ArrangedChildren.h"
#include "DragAndDrop.h"
#include "DrawElements.h"

class FArrangedChildren;

/**
 * A BoxPanel contains one child and describes how that child should be arranged on the screen.
 */
class SLATECORE_API SBoxPanel
	: public SPanel
{
public:

	/**
	 * A BoxPanel contains one BoxPanel child and describes how that
	 * child should be arranged on the screen.
	 */
	class FSlot : public TSlotBase<FSlot>
	{
	public:		
		/** Horizontal and Vertical Boxes inherit from FSlot */
		virtual ~FSlot(){}
		
		/** Horizontal positioning of child within the allocated slot */
		TEnumAsByte<EHorizontalAlignment> HAlignment;
		
		/** Vertical positioning of child within the allocated slot */
		TEnumAsByte<EVerticalAlignment> VAlignment;

		/**
		* How much space this slot should occupy along panel's direction.
		*   When SizeRule is SizeRule_Auto, the widget's DesiredSize will be used as the space required.
		*   When SizeRule is SizeRule_Stretch, the available space will be distributed proportionately between
		*   peer Widgets depending on the Value property. Available space is space remaining after all the
		*   peers' SizeRule_Auto requirements have been satisfied.
		*/
		FSizeParam SizeParam;

		/** The padding to add around the child. */
		TAttribute<FMargin> SlotPadding;
		
		/** The max size that this slot can be (0 if no max) */
		TAttribute<float> MaxSize;
			
	protected:
		/** Default values for a slot. */
		FSlot()
			: TSlotBase<FSlot>()
			, HAlignment( HAlign_Fill )
			, VAlignment( VAlign_Fill )
			, SizeParam( FStretch(1) )
			, SlotPadding( FMargin(0) )
			, MaxSize( 0.0f )
		{ }
	};

public:

	/** Removes a slot from this box panel which contains the specified SWidget
	 *
	 * @param SlotWidget The widget to match when searching through the slots
	 * @returns The index in the children array where the slot was removed and -1 if no slot was found matching the widget
	 */
	int32 RemoveSlot( const TSharedRef<SWidget>& SlotWidget );

	/**
	 * Removes all children from the box.
	 */
	void ClearChildren();

public:

	// Begin SWidget overrides.
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual FChildren* GetChildren() override;
	// End SWidget overrides.

protected:
	// Begin SWidget overrides.
	virtual FVector2D ComputeDesiredSize(float) const override;
	// End SWidget overrides.

	/**
	 * A Box Panel's orientation cannot be changed once it is constructed..
	 *
	 * @param InOrientation   The orientation of the Box Panel
	 */
	SBoxPanel( EOrientation InOrientation );

	/** The Box Panel's children. */
	TPanelChildren<FSlot> Children;

	/** The Box Panel's orientation; determined at construct time. */
	const EOrientation Orientation;
};


/** A Horizontal Box Panel. See SBoxPanel for more info. */
class SLATECORE_API SHorizontalBox : public SBoxPanel
{
public:
	class FSlot : public SBoxPanel::FSlot
	{
		public:
		FSlot()
		: SBoxPanel::FSlot()
		{
		}

		FSlot& AutoWidth()
		{
			SizeParam = FAuto();
			return *this;
		}

		FSlot& MaxWidth( const TAttribute< float >& InMaxWidth )
		{
			MaxSize = InMaxWidth;
			return *this;
		}

		FSlot& FillWidth( const TAttribute< float >& StretchCoefficient )
		{
			SizeParam = FStretch( StretchCoefficient );
			return *this;
		}
		FSlot& Padding( float Uniform )
		{
			SlotPadding = FMargin(Uniform);
			return *this;
		}

		FSlot& Padding( float Horizontal, float Vertical )
		{
			SlotPadding = FMargin(Horizontal, Vertical);
			return *this;
		}

		FSlot& Padding( float Left, float Top, float Right, float Bottom )
		{
			SlotPadding = FMargin(Left, Top, Right, Bottom);
			return *this;
		}
		
		FSlot& Padding( TAttribute<FMargin> InPadding )
		{
			SlotPadding = InPadding;
			return *this;
		}

		FSlot& HAlign( EHorizontalAlignment InHAlignment )
		{
			HAlignment = InHAlignment;
			return *this;
		}

		FSlot& VAlign( EVerticalAlignment InVAlignment )
		{
			VAlignment = InVAlignment;
			return *this;
		}

		FSlot& operator[]( TSharedRef<SWidget> InWidget )
		{
			SBoxPanel::FSlot::operator[](InWidget);
			return *this;
		}

		FSlot& Expose( FSlot*& OutVarToInit )
		{
			OutVarToInit = this;
			return *this;
		}
	};

	static FSlot& Slot()
	{
		return *(new FSlot());
	}

	SLATE_BEGIN_ARGS( SHorizontalBox )
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}

		SLATE_SUPPORTS_SLOT(SHorizontalBox::FSlot)

	SLATE_END_ARGS()

	FSlot& AddSlot()
	{
		SHorizontalBox::FSlot& NewSlot = *new SHorizontalBox::FSlot();
		this->Children.Add( &NewSlot );

		Invalidate(EInvalidateWidget::Layout);

		return NewSlot;
	}

	FSlot& InsertSlot(int32 Index = INDEX_NONE)
	{
		if (Index == INDEX_NONE)
		{
			return AddSlot();
		}
		SHorizontalBox::FSlot& NewSlot = *new SHorizontalBox::FSlot();
		this->Children.Insert(&NewSlot, Index);

		Invalidate(EInvalidateWidget::Layout);

		return NewSlot;
	}

	int32 NumSlots() const
	{
		return this->Children.Num();
	}

	FORCENOINLINE SHorizontalBox()
	: SBoxPanel( Orient_Horizontal )
	{
		bCanTick = false;
		bCanSupportFocus = false;
	}

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );
};

/** A Vertical Box Panel. See SBoxPanel for more info. */
class SLATECORE_API SVerticalBox : public SBoxPanel
{
public:
	class FSlot : public SBoxPanel::FSlot
	{
		public:

		FSlot()
		: SBoxPanel::FSlot()
		{
		}

		FSlot& AutoHeight()
		{
			SizeParam = FAuto();
			return *this;
		}

		FSlot& MaxHeight( const TAttribute< float >& InMaxHeight )
		{
			MaxSize = InMaxHeight;
			return *this;
		}

		FSlot& FillHeight( const TAttribute< float >& StretchCoefficient )
		{
			SizeParam = FStretch( StretchCoefficient );
			return *this;
		}

		FSlot& Padding( float Uniform )
		{
			SlotPadding = FMargin(Uniform);
			return *this;
		}

		FSlot& Padding( float Horizontal, float Vertical )
		{
			SlotPadding = FMargin(Horizontal, Vertical);
			return *this;
		}

		FSlot& Padding( float Left, float Top, float Right, float Bottom )
		{
			SlotPadding = FMargin(Left, Top, Right, Bottom);
			return *this;
		}

		FSlot& Padding( const TAttribute<FMargin>::FGetter& InDelegate )
		{
			SlotPadding.Bind( InDelegate );
			return *this;
		}

		FSlot& HAlign( EHorizontalAlignment InHAlignment )
		{
			HAlignment = InHAlignment;
			return *this;
		}

		FSlot& VAlign( EVerticalAlignment InVAlignment )
		{
			VAlignment = InVAlignment;
			return *this;
		}

		FSlot& Padding( TAttribute<FMargin> InPadding )
		{
			SlotPadding = InPadding;
			return *this;
		}

		FSlot& operator[]( TSharedRef<SWidget> InWidget )
		{
			SBoxPanel::FSlot::operator[](InWidget);
			return *this;
		}

		FSlot& Expose( FSlot*& OutVarToInit )
		{
			OutVarToInit = this;
			return *this;
		}
	};

	static FSlot& Slot()
	{
		return *(new FSlot());
	}


	SLATE_BEGIN_ARGS( SVerticalBox )
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}

		SLATE_SUPPORTS_SLOT(SVerticalBox::FSlot)

	SLATE_END_ARGS()

	FSlot& AddSlot()
	{
		SVerticalBox::FSlot& NewSlot = *new SVerticalBox::FSlot();
		this->Children.Add( &NewSlot );

		Invalidate(EInvalidateWidget::Layout);

		return NewSlot;
	}

	FSlot& InsertSlot(int32 Index = INDEX_NONE)
	{
		if (Index == INDEX_NONE)
		{
			return AddSlot();
		}
		SVerticalBox::FSlot& NewSlot = *new SVerticalBox::FSlot();
		this->Children.Insert(&NewSlot, Index);

		Invalidate(EInvalidateWidget::Layout);

		return NewSlot;
	}

	int32 NumSlots() const
	{
		return this->Children.Num();
	}

	FORCENOINLINE SVerticalBox()
	: SBoxPanel( Orient_Vertical )
	{
		bCanTick = false;
		bCanSupportFocus = false;
	}

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );
};

class FDragAndDropVerticalBoxOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FDragAndDropVerticalBoxOp, FDragDropOperation)

	/** Data this item represent */
	int32 SlotIndexBeingDragged;
	SVerticalBox::FSlot* SlotBeingDragged;

	virtual ~FDragAndDropVerticalBoxOp()
	{}
};

/** A Vertical Box Panel. See SBoxPanel for more info. */
class SLATECORE_API SDragAndDropVerticalBox : public SVerticalBox
{
public:
	/**
	* Where we are going to drop relative to the target item.
	*/
	enum class EItemDropZone
	{
		AboveItem,
		BelowItem
	};

	DECLARE_DELEGATE_RetVal_FourParams(FReply, FOnDragAndDropVerticalBoxDragDetected, const FGeometry&, const FPointerEvent&, int32, SVerticalBox::FSlot*)
	DECLARE_DELEGATE_OneParam(FOnDragAndDropVerticalBoxDragEnter, FDragDropEvent const&);
	DECLARE_DELEGATE_OneParam(FOnDragAndDropVerticalBoxDragLeave, FDragDropEvent const&);
	DECLARE_DELEGATE_RetVal_OneParam(FReply, FOnDragAndDropVerticalBoxDrop, FDragDropEvent const&);

	/** Delegate signature for querying whether this FDragDropEvent will be handled by the drop target of type ItemType. */
	DECLARE_DELEGATE_RetVal_ThreeParams(TOptional<EItemDropZone>, FOnCanAcceptDrop, const FDragDropEvent&, SDragAndDropVerticalBox::EItemDropZone, SVerticalBox::FSlot*);
	/** Delegate signature for handling the drop of FDragDropEvent onto target of type ItemType */
	DECLARE_DELEGATE_RetVal_FourParams(FReply, FOnAcceptDrop, const FDragDropEvent&, SDragAndDropVerticalBox::EItemDropZone, int32, SVerticalBox::FSlot*);

	SLATE_BEGIN_ARGS(SDragAndDropVerticalBox)
		{}

		// High Level DragAndDrop

		/**
		* Handle this event to determine whether a drag and drop operation can be executed on top of the target row widget.
		* Most commonly, this is used for previewing re-ordering and re-organization operations in lists or trees.
		* e.g. A user is dragging one item into a different spot in the list or tree.
		*      This delegate will be called to figure out if we should give visual feedback on whether an item will
		*      successfully drop into the list.
		*/
		SLATE_EVENT(FOnCanAcceptDrop, OnCanAcceptDrop)

		/**
		* Perform a drop operation onto the target row widget
		* Most commonly used for executing a re-ordering and re-organization operations in lists or trees.
		* e.g. A user was dragging one item into a different spot in the list; they just dropped it.
		*      This is our chance to handle the drop by reordering items and calling for a list refresh.
		*/
		SLATE_EVENT(FOnAcceptDrop, OnAcceptDrop)

		// Low level DragAndDrop
		SLATE_EVENT(FOnDragAndDropVerticalBoxDragDetected, OnDragDetected)
		SLATE_EVENT(FOnDragAndDropVerticalBoxDragEnter, OnDragEnter)
		SLATE_EVENT(FOnDragAndDropVerticalBoxDragLeave, OnDragLeave)
		SLATE_EVENT(FOnDragAndDropVerticalBoxDrop, OnDrop)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Set the Drop indicators */
	SDragAndDropVerticalBox& SetDropIndicator_Above(const FSlateBrush& InValue) { DropIndicator_Above = InValue; return *this; }
	SDragAndDropVerticalBox& SetDropIndicator_Below(const FSlateBrush& InValue) { DropIndicator_Below = InValue; return *this; }

	/** Drag detection and handling */
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
		
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnDragEnter(FGeometry const& MyGeometry, FDragDropEvent const& DragDropEvent) override;
	virtual void OnDragLeave(FDragDropEvent const& DragDropEvent) override;
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	/** @see SDragAndDropVerticalBox's OnCanAcceptDrop event */
	FOnCanAcceptDrop OnCanAcceptDrop;

	/** @see SDragAndDropVerticalBox's OnAcceptDrop event */
	FOnAcceptDrop OnAcceptDrop;

	/** Are we currently dragging/dropping over this item? */
	TOptional<EItemDropZone> ItemDropZone;

	/** Delegate triggered when a user starts to drag a slot item */
	FOnDragAndDropVerticalBoxDragDetected OnDragDetected_Handler;

	/** Delegate triggered when a user's drag enters the bounds of this slot item */
	FOnDragAndDropVerticalBoxDragEnter OnDragEnter_Handler;

	/** Delegate triggered when a user's drag leaves the bounds of this slot item */
	FOnDragAndDropVerticalBoxDragLeave OnDragLeave_Handler;

	/** Delegate triggered when a user's drag is dropped in the bounds of this slot item */
	FOnDragAndDropVerticalBoxDrop OnDrop_Handler;

	/** Brush used to provide feedback that a user can drop below the hovered row. */
	FSlateBrush DropIndicator_Above;
	FSlateBrush DropIndicator_Below;

	/** This is required for the paint to access which item we're hovering */
	FVector2D CurrentDragOperationScreenSpaceLocation;
	int32 CurrentDragOverSlotIndex;

	/** @return the zone (above, below) based on where the user is hovering over */
	EItemDropZone ZoneFromPointerPosition(FVector2D LocalPointerPos, const FGeometry& CurrentGeometry, const FGeometry& StartGeometry) const;
};
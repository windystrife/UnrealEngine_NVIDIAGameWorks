// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Layout/LayoutGeometry.h"
#include "Widgets/SWidget.h"
#include "SlotBase.h"
#include "Layout/Children.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Widgets/SPanel.h"
#include "Styling/SlateTypes.h"

class FArrangedChildren;
class FPaintArgs;
class FSlateWindowElementList;

namespace ESplitterResizeMode
{
	enum Type
	{
		FixedPosition,
		FixedSize,
		Fill,
	};
}

class FLayoutGeometry;
/**
 * SSplitter divides its allotted area into N segments, where N is the number of children it has.
 * It allows the users to resize the children along the splitters axis: that is, horizontally or vertically.
 */
class SLATE_API SSplitter : public SPanel
{

public:
	/** How should a child's size be determined */
	enum ESizeRule
	{
		/** Get the DesiredSize() of the content */
		SizeToContent,
		/** Use a fraction of the parent's size */
		FractionOfParent
	};

	DECLARE_DELEGATE_OneParam(
		FOnSlotResized,
		/** The new size coefficient of the slot */
		float );

	DECLARE_DELEGATE_RetVal_OneParam(FVector2D, FOnGetMaxSlotSize, int32);

public:
	class FSlot : public TSlotBase<FSlot>
	{
	public:		
		FSlot()
			: TSlotBase<FSlot>()
			, SizingRule( FractionOfParent )
			, SizeValue( 1 )
		{
		}

		FSlot& Value( const TAttribute<float>& InValue )
		{
			SizeValue = InValue;
			return *this;
		}
		
		FSlot& OnSlotResized( const FOnSlotResized& InHandler )
		{
			OnSlotResized_Handler = InHandler;
			return *this;
		}

		FSlot& SizeRule( const TAttribute<ESizeRule>& InSizeRule ) 
		{
			SizingRule = InSizeRule;
			return *this;
		}

		TAttribute<ESizeRule> SizingRule;
		TAttribute<float> SizeValue;
		FOnSlotResized OnSlotResized_Handler;
	};

	/** @return a new SSplitter::FSlot() */
	static FSlot& Slot();
	
	/**
	 * Add a slot to the splitter at the specified index
	 * Sample usage:
	 *     SomeSplitter->AddSlot()
	 *     [
	 *       SNew(SSomeWidget)
	 *     ];
	 *
	 * @return the new slot.
	 */
	FSlot& AddSlot( int32 AtIndex = INDEX_NONE );


	SLATE_BEGIN_ARGS(SSplitter)
		: _Style( &FCoreStyle::Get().GetWidgetStyle<FSplitterStyle>("Splitter") )
		, _Orientation( Orient_Horizontal )
		, _ResizeMode( ESplitterResizeMode::FixedPosition )
		, _PhysicalSplitterHandleSize( 5.0f )
		, _HitDetectionSplitterHandleSize( 5.0f )
		, _OnSplitterFinishedResizing()
		{
		}

		SLATE_SUPPORTS_SLOT(FSlot)

		/** Style used to draw this splitter */
		SLATE_STYLE_ARGUMENT( FSplitterStyle, Style )

		SLATE_ARGUMENT( EOrientation, Orientation )

		SLATE_ARGUMENT( ESplitterResizeMode::Type, ResizeMode )

		SLATE_ARGUMENT( float, PhysicalSplitterHandleSize )

		SLATE_ARGUMENT( float, HitDetectionSplitterHandleSize )

		SLATE_EVENT( FSimpleDelegate, OnSplitterFinishedResizing )
		
		SLATE_EVENT(FOnGetMaxSlotSize, OnGetMaxSlotSize)

	SLATE_END_ARGS()

	SSplitter();

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

public:

	/**
	 * Get the slot at the specified index
	 *
	 * @param SlotIndex    Replace the child at this index.
	 *
	 * @return Slot at the index specified by SlotIndex
	 */
	SSplitter::FSlot& SlotAt( int32 SlotIndex );


	/**
	 * Remove the child at IndexToRemove
	 *
	 * @param IndexToRemove     Remove the slot and child at this index.
	 */
	void RemoveAt( int32 IndexToRemove );

public:

	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;


	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;


	/**
	 * A Panel's desired size in the space required to arrange of its children on the screen while respecting all of
	 * the children's desired sizes and any layout-related options specified by the user. See StackPanel for an example.
	 */
	virtual FVector2D ComputeDesiredSize(float) const override;

	/**
	 * All widgets must provide a way to access their children in a layout-agnostic way.
	 * Panels store their children in Slots, which creates a dilemma. Most panels
	 * can store their children in a TPanelChildren<Slot>, where the Slot class
	 * provides layout information about the child it stores. In that case
	 * GetChildren should simply return the TPanelChildren<Slot>. See StackPanel for an example.
	 */
	virtual FChildren* GetChildren() override;

	/**
	 * The system calls this method to notify the widget that a mouse button was pressed within it. This event is bubbled.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	
	/**
	 * The system calls this method to notify the widget that a mouse button was release within it. This event is bubbled.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) override;

	/**
	 * The system asks each widget under the mouse to provide a cursor. This event is bubbled.
	 * 
	 * @return FCursorReply::Unhandled() if the event is not handled; return FCursorReply::Cursor() otherwise.
	 */
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;

	/**
	 * Change the orientation of the splitter
	 *
	 * @param NewOrientation  Should the splitter be horizontal or vertical
	 */
	void SetOrientation( EOrientation NewOrientation );

	/**
	 * @return the current orientation of the splitter.
	 */
	EOrientation GetOrientation() const;	


private:
	TArray<FLayoutGeometry> ArrangeChildrenForLayout( const FGeometry& AllottedGeometry ) const;

protected:

	/**
	 * Given the index of the dragged handle and the children, find a child above/left_of of the dragged handle that can be resized.
	 *
	 * @return INDEX_NONE if no such child can be found.
	 */
	static int32 FindResizeableSlotBeforeHandle( int32 DraggedHandle, const TPanelChildren<FSlot>& Children );

	/**
	 * Given the index of the dragged handle and the children, find a child below/right_of the dragged handle that can be resized
	 *
	 * @return Children.Num() if no such child can be found.
	 */
	static int32 FindResizeableSlotAfterHandle( int32 DraggedHandle, const TPanelChildren<FSlot>& Children );

	static void FindAllResizeableSlotsAfterHandle( int32 DraggedHandle, const TPanelChildren<FSlot>& Children, TArray< int32 >& OutSlotIndicies );

	/**
	 * Resizes the children based on user input. The template parameter Orientation corresponds to the splitter being horizontal or vertical.
	 * 
	 * @param DraggedHandle    The index of the handle that the user is dragging.
	 * @param LocalMousePos    The position of the mouse in this widgets local space.
	 * @param Children         A reference to this splitter's children array; we will modify the children's layout values.
	 * @param ChildGeometries  The arranged children; we need their sizes and positions so that we can perform a resizing.
	 */
	static void HandleResizingByMousePosition(EOrientation Orientation, const float PhysicalSplitterHandleSize, const ESplitterResizeMode::Type ResizeMode, int32 DraggedHandle, const FVector2D& LocalMousePos, TPanelChildren<FSlot>& Children, const TArray<FLayoutGeometry>& ChildGeometries );
	static void HandleResizingDelta(EOrientation Orientation, const float PhysicalSplitterHandleSize, const ESplitterResizeMode::Type ResizeMode, int32 DraggedHandle, float Delta, TPanelChildren<FSlot>& Children, const TArray<FLayoutGeometry>& ChildGeometries);
	static void HandleResizingBySize(EOrientation Orientation, const float PhysicalSplitterHandleSize, const ESplitterResizeMode::Type ResizeMode, int32 DraggedHandle, const FVector2D& DesiredSize, TPanelChildren<FSlot>& Children, const TArray<FLayoutGeometry>& ChildGeometries);

	/**
	 * @param ProposedSize  A size that a child would like to be
	 *
	 * @return A size that is clamped against the minimum size allowed for children.
	 */
	static float ClampChild( float ProposedSize );

	/**
	 * Given a mouse position within the splitter, figure out which resize handle we are hovering (if any).
	 *

	 * @param LocalMousePos  The mouse position within this splitter.
	 * @param ChildGeometris The arranged children and their geometries; we need to test the mouse against them.
	 *
	 * @return The index of the handle being hovered, or INDEX_NONE if we are not hovering a handle.
	 */
	template<EOrientation SplitterOrientation>
	static int32 GetHandleBeingResizedFromMousePosition(  float PhysicalSplitterHandleSize, float HitDetectionSplitterHandleSize, FVector2D LocalMousePos, const TArray<FLayoutGeometry>& ChildGeometries );

	TPanelChildren< FSlot > Children;

	int32 HoveredHandleIndex;
	bool bIsResizing;
	EOrientation Orientation;
	ESplitterResizeMode::Type ResizeMode;

	FSimpleDelegate OnSplitterFinishedResizing;
	FOnGetMaxSlotSize OnGetMaxSlotSize;

	/** The thickness of the grip area that the user uses to resize a splitter */
	float PhysicalSplitterHandleSize;
	float HitDetectionSplitterHandleSize;

	const FSplitterStyle* Style;
};

/**
 * SSplitter2x2													
 * A splitter which has exactly 4 children and allows simultaneous		
 * of all children along an axis as well as resizing all children
 * by dragging the center of the splitter.
 */
class SLATE_API SSplitter2x2 : public SPanel
{
public:
	class FSlot : public TSlotBase<FSlot>
	{
	public:	
		/** Default Constructor.  Initially each slot takes up a quarter of the entire space */
		FSlot()
			: TSlotBase<FSlot>( SNullWidget::NullWidget )
			, PercentageAttribute( FVector2D(0.5, 0.5) )
		{
		}

		/** Copy Constructor */
		FSlot( const TSharedRef<SWidget>& InWidget )
			: TSlotBase<FSlot>( InWidget )
			, PercentageAttribute( FVector2D(0.5, 0.5) )
		{
		}

		/**
		 * Sets the percentage attribute
		 *
		 * @param Value The new percentage value
		 */
		FSlot& SetPercentage( const FVector2D& Value )
		{
			PercentageAttribute.Set( Value );
			return *this;
		}
	public:
		/** The percentage of the alloted space of the splitter that this slot requires */
		TAttribute<FVector2D> PercentageAttribute;
	};

	SLATE_BEGIN_ARGS( SSplitter2x2 ){}
		SLATE_NAMED_SLOT( FArguments, TopLeft )
		SLATE_NAMED_SLOT( FArguments, BottomLeft )
		SLATE_NAMED_SLOT( FArguments, TopRight )
		SLATE_NAMED_SLOT( FArguments, BottomRight )
	SLATE_END_ARGS()

	SSplitter2x2();

	void Construct( const FArguments& InArgs );

	/**
	 * Returns the widget displayed in the splitter top left area
	 *
	 * @return	Top left widget
	 */
	TSharedRef< SWidget > GetTopLeftContent();

	/**
	 * Returns the widget displayed in the splitter bottom left area
	 *
	 * @return	Bottom left widget
	 */
	TSharedRef< SWidget > GetBottomLeftContent();

	/**
	 * Returns the widget displayed in the splitter top right area
	 *
	 * @return	Top right widget
	 */
	TSharedRef< SWidget > GetTopRightContent();

	/**
	 * Returns the widget displayed in the splitter bottom right area
	 *
	 * @return	Bottom right widget
	 */
	TSharedRef< SWidget > GetBottomRightContent();

	/**
	 * Sets the widget to be displayed in the splitter top left area
	 *
	 * @param	TopLeftContent	The top left widget
	 */
	void SetTopLeftContent( TSharedRef< SWidget > TopLeftContent );

	/**
	 * Sets the widget to be displayed in the splitter bottom left area
	 *
	 * @param	BottomLeftContent	The bottom left widget
	 */
	void SetBottomLeftContent( TSharedRef< SWidget > BottomLeftContent );

	/**
	 * Sets the widget to be displayed in the splitter top right area
	 *
	 * @param	TopRightContent	The top right widget
	 */
	void SetTopRightContent( TSharedRef< SWidget > TopRightContent );

	/**
	 * Sets the widget to be displayed in the splitter bottom right area
	 *
	 * @param	BottomRightContent	The bottom right widget
	 */
	void SetBottomRightContent( TSharedRef< SWidget > BottomRightContent );

	/** Returns an array of size percentages for the children in this order: TopLeft, BottomLeft, TopRight, BottomRight */
	void GetSplitterPercentages( TArray< FVector2D >& OutPercentages ) const;
	
	/** Sets the size percentages for the children in this order: TopLeft, BottomLeft, TopRight, BottomRight */
	void SetSplitterPercentages( const TArray< FVector2D >& InPercentages );


private:

	TArray<FLayoutGeometry> ArrangeChildrenForLayout( const FGeometry& AllottedGeometry ) const;

	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;

	/**
	 * A Panel's desired size in the space required to arrange of its children on the screen while respecting all of
	 * the children's desired sizes and any layout-related options specified by the user. See StackPanel for an example.
	 */
	virtual FVector2D ComputeDesiredSize(float) const override;

	/**
	 * All widgets must provide a way to access their children in a layout-agnostic way.
	 * Panels store their children in Slots, which creates a dilemma. Most panels
	 * can store their children in a TPanelChildren<Slot>, where the Slot class
	 * provides layout information about the child it stores. In that case
	 * GetChildren should simply return the TPanelChildren<Slot>. See StackPanel for an example.
	 */
	virtual FChildren* GetChildren() override;

	/**
	 * The system calls this method to notify the widget that a mouse button was pressed within it. This event is bubbled.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
 	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	
	/**
	 * The system calls this method to notify the widget that a mouse button was release within it. This event is bubbled.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
 	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	/**
	 * The system calls this method to notify the widget that a mouse moved within it. This event is bubbled.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	/**
	 * @return The cursor that should be visible
	 */
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;

	/**
	 * Calculates the axis being resized
	 * 
	 * @param MyGeometry	The geometry of this widget
	 * @param LocalMousePos	The local space current mouse position
	 */
	int32 CalculateResizingAxis( const FGeometry& MyGeometry, const FVector2D& LocalMousePos ) const;

	/**
	 * Resizes all children based on a user moving the splitter handles
	 * 
	 * @param ArrangedChildren	The current geometry of all arranged children before the user moved the splitter
	 * @param LocalMousePos		The current mouse position		
	 */
	void ResizeChildren( const FGeometry& MyGeometry, const TArray<FLayoutGeometry>& ArrangedChildren, const FVector2D& LocalMousePos );


private:

	/** The children of the splitter. There can only be four */
	TPanelChildren<FSlot> Children;

	/** The axis currently being resized or INDEX_NONE if no resizing */
	int32 ResizingAxis;

	/** true if a splitter axis is currently being resized. */
	bool bIsResizing;

	float SplitterHandleSize;
};

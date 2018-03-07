// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SlotBase.h"
#include "Layout/Children.h"
#include "Widgets/SPanel.h"

class FArrangedChildren;

/**
 * Implements a widget switcher.
 *
 * A widget switcher is like a tab control, but without tabs. At most one widget is visible at time.
 */
class SLATE_API SWidgetSwitcher
	: public SPanel
{
public:

	class FSlot
		: public TSlotBase<FSlot>
		, public TSupportsContentAlignmentMixin<FSlot>
		, public TSupportsContentPaddingMixin<FSlot>
	{
	public:

		FSlot()
			: TSlotBase<FSlot>()
			,TSupportsContentAlignmentMixin<FSlot>( HAlign_Fill, VAlign_Fill )
		{ }
	};

	SLATE_BEGIN_ARGS(SWidgetSwitcher)
		: _WidgetIndex(0)
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}

		SLATE_SUPPORTS_SLOT(FSlot)

		/** Holds the index of the initial widget to be displayed (INDEX_NONE = default). */
		SLATE_ATTRIBUTE(int32, WidgetIndex)

	SLATE_END_ARGS()

	SWidgetSwitcher();

public:

	/**
	 * Adds a slot to the widget switcher at the specified location.
	 *
	 * @param SlotIndex The index at which to insert the slot, or INDEX_NONE to append.
	 */
	FSlot& AddSlot( int32 SlotIndex = INDEX_NONE );

	/**
	 * Constructs the widget.
	 */
	void Construct( const FArguments& InArgs );

	/**
	 * Gets the active widget.
	 *
	 * @return Active widget.
	 */
	TSharedPtr<SWidget> GetActiveWidget( ) const;

	/**
	 * Gets the slot index of the currently active widget.
	 *
	 * @return The slot index, or INDEX_NONE if no widget is active.
	 */
	int32 GetActiveWidgetIndex( ) const
	{
		return WidgetIndex.Get();
	}

	/**
	 * Gets the number of widgets that this switcher manages.
	 *
	 * @return Number of widgets.
	 */
	int32 GetNumWidgets( ) const
	{
		return AllChildren.Num();
	}

	/**
	 * Gets the widget in the specified slot.
	 *
	 * @param SlotIndex The slot index of the widget to get.
	 * @return The widget, or nullptr if the slot does not exist.
	 */
	TSharedPtr<SWidget> GetWidget( int32 SlotIndex ) const;

	/**
	 * Gets the slot index of the specified widget.
	 *
	 * @param Widget The widget to get the index for.
	 * @return The slot index, or INDEX_NONE if the widget does not exist.
	 */
	int32 GetWidgetIndex( TSharedRef<SWidget> Widget ) const;

	/**
	 * Removes a slot with the corresponding widget in it.  Returns the index where the widget was found, otherwise -1.
	 *
	 * @param Widget The widget to find and remove.
	 */
	int32 RemoveSlot( TSharedRef<SWidget> WidgetToRemove );

	/**
	 * Sets the active widget.
	 *
	 * @param Widget The widget to activate.
	 */
	void SetActiveWidget( TSharedRef<SWidget> Widget )
	{
		SetActiveWidgetIndex(GetWidgetIndex(Widget));
	}

	/**
	 * Activates the widget at the specified index.
	 *
	 * @param Index The slot index.
	 */
	void SetActiveWidgetIndex( int32 Index );

public:

	/**
	 * Creates a new widget slot.
	 *
	 * @return A new slot.
	 */
	static FSlot& Slot( )
	{
		return *(new FSlot());
	}

protected:

	// SCompoundWidget interface

	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual FVector2D ComputeDesiredSize( float ) const override;
	virtual FChildren* GetChildren( ) override;

	const FSlot* GetActiveSlot() const;

private:

	/** Required to implement GetChildren() in a way that can dynamically return the currently active child. */
	class SLATE_API FOneDynamicChild
		: public FChildren
	{
	public:

		FOneDynamicChild( TPanelChildren<FSlot>* InAllChildren = nullptr, const TAttribute<int32>* InWidgetIndex = nullptr )
			: AllChildren( InAllChildren )
			, WidgetIndex( InWidgetIndex )
		{ }
		
		virtual int32 Num() const override { return AllChildren->Num() > 0 ? 1 : 0; }
		
		virtual TSharedRef<SWidget> GetChildAt( int32 Index ) override { check(Index == 0); return AllChildren->GetChildAt(WidgetIndex->Get()); }
		
		virtual TSharedRef<const SWidget> GetChildAt( int32 Index ) const override { check(Index == 0); return AllChildren->GetChildAt(WidgetIndex->Get()); }
		
	private:

		virtual const FSlotBase& GetSlotAt(int32 ChildIndex) const override { return (*AllChildren)[ChildIndex]; }

		TPanelChildren<FSlot>* AllChildren;
		const TAttribute<int32>* WidgetIndex;

	} OneDynamicChild;

	/** Holds the desired widget index */
	TAttribute<int32> WidgetIndex;

	// Holds the collection of widgets.
	TPanelChildren<FSlot> AllChildren;
};

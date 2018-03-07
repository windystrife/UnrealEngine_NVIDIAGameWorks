// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SlotBase.h"
#include "Widgets/SWidget.h"
#include "Layout/Children.h"
#include "Widgets/SPanel.h"

class FArrangedChildren;
class FPaintArgs;
class FSlateWindowElementList;

class SLATE_API SGridPanel
	: public SPanel
{
public:
	// Used by the mandatory named parameter in FSlot
	class Layer
	{
	public:
		explicit Layer(int32 InLayer)
			: TheLayer(InLayer)
		{

		}

		int TheLayer;
	};

	class FSlot : public TSlotBase<FSlot>, public TSupportsContentAlignmentMixin<FSlot>, public TSupportsContentPaddingMixin<FSlot>
	{
		public:
			/** Default values for a slot. */
			FSlot( int32 Column, int32 Row, int32 InLayer )
				: TSlotBase<FSlot>()
				, TSupportsContentAlignmentMixin<FSlot>(HAlign_Fill, VAlign_Fill)
				, ColumnParam( Column )
				, ColumnSpanParam( 1 )
				, RowParam( Row )
				, RowSpanParam( 1 )
				, LayerParam( InLayer )
				, NudgeParam( FVector2D::ZeroVector )
			{
			}

			/** The panel that contains this slot */
			TWeakPtr<SGridPanel> Panel;

			/** Which column in the grid this cell belongs to */
			int32 ColumnParam;

			/** How many columns this slot spans over */
			int32 ColumnSpanParam;

			/** Which row in the grid this cell belongs to */
			int32 RowParam;

			/** How many rows this this slot spans over */
			int32 RowSpanParam;

			/** Positive values offset this cell to be hit-tested and drawn on top of others. Default is 0; i.e. no offset. */
			int32 LayerParam;

			/** Offset this slot's content by some amount; positive values offset to lower right*/
			FVector2D NudgeParam;

			/**  */
			FSlot& Column(int32 Column)
			{
				ColumnParam = FMath::Max(0, Column);
				NotifySlotChanged();
				return *this;
			}

			/** How many columns this slot spans over */
			FSlot& ColumnSpan( int32 ColumnSpan )
			{
				ColumnSpanParam = FMath::Max(1,ColumnSpan);
				NotifySlotChanged();
				return *this;
			}

			/**  */
			FSlot& Row(int32 Row)
			{
				RowParam = FMath::Max(0, Row);
				NotifySlotChanged();
				return *this;
			}

			/** How many rows this this slot spans over */
			FSlot& RowSpan( int32 RowSpan )
			{
				RowSpanParam = FMath::Max(1, RowSpan);
				NotifySlotChanged();
				return *this;
			}

			/** Positive values offset this cell to be hit-tested and drawn on top of others. Default is 0; i.e. no offset. */
			FSlot& Layer(int32 Layer)
			{
				LayerParam = Layer;
				NotifySlotChanged();
				return *this;
			}

			/** Offset this slot's content by some amount; positive values offset to lower right */
			FSlot& Nudge( const FVector2D& Nudge )
			{
				NudgeParam = Nudge;
				return *this;
			}

			/** Notify that the slot was changed */
			FORCEINLINE void NotifySlotChanged()
			{
				if ( Panel.IsValid() )
				{
					Panel.Pin()->NotifySlotChanged(this);
				}
			}
	};

	/**
	 * Used by declarative syntax to create a Slot in the specified Column, Row and Layer.
	 */
	static FSlot& Slot( int32 Column, int32 Row, Layer InLayer = Layer(0) )
	{
		return *(new FSlot( Column, Row, InLayer.TheLayer ));
	}

	/**
	 * Dynamically add a new slot to the UI at specified Column and Row. Optionally, specify a layer at which this slot should be added.
	 *
	 * @return A reference to the newly-added slot
	 */
	FSlot& AddSlot( int32 Column, int32 Row, Layer InLayer = Layer(0) );

	/**
	* Removes a slot from this panel which contains the specified SWidget
	*
	* @param SlotWidget The widget to match when searching through the slots
	* @returns The true if the slot was removed and false if no slot was found matching the widget
	*/
	bool RemoveSlot(const TSharedRef<SWidget>& SlotWidget);

	SLATE_BEGIN_ARGS( SGridPanel )
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}
		SLATE_SUPPORTS_SLOT( FSlot )

		/** Specify a column to stretch instead of sizing to content. */
		FArguments& FillColumn( int32 ColumnId, const TAttribute<float>& Coefficient )
		{
			while (ColFillCoefficients.Num() <= ColumnId)
			{
				ColFillCoefficients.Emplace( 0 );
			}
			ColFillCoefficients[ColumnId] = Coefficient;
			return Me();
		}

		/** Specify a row to stretch instead of sizing to content. */
		FArguments& FillRow( int32 RowId, const TAttribute<float>& Coefficient )
		{
			while (RowFillCoefficients.Num() <= RowId)
			{
				RowFillCoefficients.Emplace( 0 );
			}
			RowFillCoefficients[RowId] = Coefficient;
			return Me();
		}

		/** Coefficients for columns that need to stretch instead of size to content */
		TArray<TAttribute<float>> ColFillCoefficients;
		
		/** Coefficients for rows that need to stretch instead of size to content */
		TArray<TAttribute<float>> RowFillCoefficients;
		
	SLATE_END_ARGS()

	SGridPanel();

	/** Removes all slots from the panel */
	void ClearChildren();
	
	void Construct( const FArguments& InArgs );

	/**
	 * GetDesiredSize of a subregion in the graph.
	 *
	 * @param StartCell   The cell (inclusive) in the upper left of the region.
	 * @param Size        Number of cells in the X and Y directions to get the size for.
	 *
	 * @return FVector2D  The desired size of the region of cells specified.
	 */
	FVector2D GetDesiredSize( const FIntPoint& StartCell, int32 Width, int32 Height ) const;

	/** Specify a column to stretch instead of sizing to content. */
	void SetColumnFill( int32 ColumnId, const TAttribute<float>& Coefficient );

	/** Specify a row to stretch instead of sizing to content. */
	void SetRowFill( int32 RowId, const TAttribute<float>& Coefficient );

	/** Clear the row and column fill rules. */
	void ClearFill();

public:

	// SWidget interface

	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual void CacheDesiredSize(float) override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FChildren* GetChildren() override;

private:

	/**
	 * Given an array of values, re-populate the array such that every contains the partial sums up to that element.
	 * i.e. Array[N] = Array.Sum(0 .. N-1)
	 *
	 * The resulting array is 1-element longer.
	 */
	static void ComputePartialSums( TArray<float>& TurnMeIntoPartialSums );
	
	/** Given a SizeContribution, distribute it to the elements in DistributeOverMe at indexes from [StartIndex .. UpperBound) */
	static void DistributeSizeContributions( float SizeContribution, TArray<float>& DistributeOverMe, int32 StartIndex, int32 UpperBound );

	/**
	 * Inserts the given slot into the list of Slots based on its LayerParam, such that Slots are sorter by layer.
	 *
	 * @param The newly-allocated slot to insert.
	 * @return A reference to the added slot
	 */
	FSlot& InsertSlot( FSlot* InSlot );	

	/** Compute the sizes of columns and rows needed to fit all the slots in this grid. */
	void ComputeDesiredCellSizes( TArray<float>& OutColumns, TArray<float>& OutRows ) const;

	/** Draw the debug grid of rows and colummns; useful for inspecting the GridPanel's logic. See OnPaint() for parameter meaning */
	int32 LayoutDebugPaint(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId ) const;
	
	/** 
	 * Callback used to resize our internal arrays if a slot (or slot span) is changed. 
	 *
	 * @param The slot that has just changed.
	 */
	void NotifySlotChanged( FSlot* InSlot );

private:

	/** The slots that are placed into various grid locations */
	TPanelChildren<FSlot> Slots;

	/**
	 * Offsets of each column from the beginning of the grid.
	 * Includes a faux value at the end of the array for finding the size of the last cell.
	 */
	TArray<float> Columns;
	
	/**
	 * Offsets of each row from the beginning of the grid.
	 * Includes a faux value at the end of the array for finding the size of the last cell.
	 */
	TArray<float> Rows;

	/** Total desires size along each axis. */
	FVector2D TotalDesiredSizes;

	/** Fill coefficients for the columns */
	TArray<TAttribute<float>> ColFillCoefficients;

	/** Fill coefficients for the rows */
	TArray<TAttribute<float>> RowFillCoefficients;
};

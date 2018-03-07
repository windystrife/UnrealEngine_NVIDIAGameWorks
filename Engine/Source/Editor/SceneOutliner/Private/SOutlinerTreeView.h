// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SceneOutlinerFwd.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "Animation/SlateSprings.h"

namespace SceneOutliner
{

	class SOutlinerTreeView : public STreeView< FTreeItemPtr >
	{
	public:
		
		/** Construct this widget */
		void Construct(const FArguments& InArgs, TSharedRef<SSceneOutliner> Owner);

		void FlashHighlightOnItem( FTreeItemPtr FlashHighlightOnItem );

	protected:

		virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;

		virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;

		//virtual FReply OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

		virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;

		/** Weak reference to the outliner widget that owns this list */
		TWeakPtr<SSceneOutliner> SceneOutlinerWeak;
	};

	/** Widget that represents a row in the outliner's tree control.  Generates widgets for each column on demand. */
	class SSceneOutlinerTreeRow
		: public SMultiColumnTableRow< FTreeItemPtr >
	{

	public:

		SLATE_BEGIN_ARGS( SSceneOutlinerTreeRow ) {}

			/** The list item for this row */
			SLATE_ARGUMENT( FTreeItemPtr, Item )

		SLATE_END_ARGS()


		/** Construct function for this widget */
		void Construct( const FArguments& InArgs, const TSharedRef<SOutlinerTreeView>& OutlinerTreeView, TSharedRef<SSceneOutliner> SceneOutliner );

		/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the tree row. */
		virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override;

		void FlashHighlight();

	protected:

		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
		virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

		virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;

		virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;

		virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override
		{
			return FReply::Handled();
		}

		virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;

		bool IsRowEnabled() const;

	private:

		/** Weak reference to the outliner widget that owns our list */
		TWeakPtr< SSceneOutliner > SceneOutlinerWeak;

		/** The item associated with this row of data */
		TWeakPtr<ITreeItem> Item;

	protected:

		virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

		virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;

	private:
		/** How many pixels to extend the highlight rectangle's left side horizontally */
		static const float HighlightRectLeftOffset;

		/** How many pixels to extend the highlight rectangle's right side horizontally */
		static const float HighlightRectRightOffset;

		/** How quickly the highlight 'targeting' rectangle will slide around.  Larger is faster. */
		static const float HighlightTargetSpringConstant;

		/** Duration of animation highlight target effects */
		static const float HighlightTargetEffectDuration;

		/** Opacity of the highlight target effect overlay */
		static const float HighlightTargetOpacity;

		/** How large the highlight target effect will be when highlighting, as a scalar percentage of font height */
		static const float LabelChangedAnimOffsetPercent;

		/** Highlight "targeting" visual effect left position */
		FFloatSpring1D HighlightTargetLeftSpring;

		/** Highlight "targeting" visual effect right position */
		FFloatSpring1D HighlightTargetRightSpring;

		/** Last time that the user had a major interaction with the highlight */
		double LastHighlightInteractionTime;
	};
}		// namespace SceneOutliner

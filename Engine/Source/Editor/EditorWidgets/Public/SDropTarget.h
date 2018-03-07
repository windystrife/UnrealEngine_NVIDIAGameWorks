// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "EditorStyleSet.h"

class FPaintArgs;
class FSlateWindowElementList;

/** 
 * A widget that displays a hover cue and handles dropping assets of allowed types onto this widget
 */
class EDITORWIDGETS_API SDropTarget : public SCompoundWidget
{
public:
	/** Called when a valid asset is dropped */
	DECLARE_DELEGATE_RetVal_OneParam(FReply, FOnDrop, TSharedPtr<FDragDropOperation>);

	DECLARE_DELEGATE_RetVal_OneParam(bool, FVerifyDrag, TSharedPtr<FDragDropOperation>);

	SLATE_BEGIN_ARGS(SDropTarget)
		: _ValidColor(FLinearColor(0, 1, 0, 1))
		, _InvalidColor(FLinearColor(1, 0, 0, 1))
		, _BackgroundColor(FLinearColor(1, 1, 1, 0.50f))
		, _BackgroundColorHover(FLinearColor(1, 1, 1, 0.25f))
		, _VerticalImage(FEditorStyle::GetBrush("WideDash.Vertical"))
		, _HorizontalImage(FEditorStyle::GetBrush("WideDash.Horizontal"))
		, _BackgroundImage(FEditorStyle::GetBrush("WhiteBrush"))
	{ }
		/* Content to display for the in the drop target */
		SLATE_DEFAULT_SLOT( FArguments, Content )
		/** The color of the vertical/horizontal images when the drop data is valid */
		SLATE_ARGUMENT(FLinearColor, ValidColor)
		/** The color of the vertical/horizontal images when the drop data is not valid */
		SLATE_ARGUMENT(FLinearColor, InvalidColor)
		/** The color multiplier that is applied to the background image. */
		SLATE_ARGUMENT(FLinearColor, BackgroundColor)
		/** The color multiplier that is applied to the background image on hover. */
		SLATE_ARGUMENT(FLinearColor, BackgroundColorHover)
		/** Vertical border image that is used. */
		SLATE_ARGUMENT(const FSlateBrush*, VerticalImage)
		/** Horizontal border image that is used. */
		SLATE_ARGUMENT(const FSlateBrush*, HorizontalImage)
		/** The background image that is applied after the surface. */
		SLATE_ARGUMENT(const FSlateBrush*, BackgroundImage)
		/** Called when a valid asset is dropped */
		SLATE_EVENT(FOnDrop, OnDrop)
		/** Called to check if an asset is acceptable for dropping */
		SLATE_EVENT(FVerifyDrag, OnAllowDrop)
		/** Called to check if an asset is acceptable for dropping */
		SLATE_EVENT(FVerifyDrag, OnIsRecognized)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs );

protected:

	bool AllowDrop(TSharedPtr<FDragDropOperation> DragDropOperation) const;

	virtual bool OnAllowDrop(TSharedPtr<FDragDropOperation> DragDropOperation) const;
	virtual bool OnIsRecognized(TSharedPtr<FDragDropOperation> DragDropOperation) const;

protected:
	// SWidget interface
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	// End of SWidget interface

	/** @return Visibility of the overlay text when dragging is occurring. */
	EVisibility GetDragOverlayVisibility() const;

	/** Get the brightness on the background. */
	FSlateColor GetBackgroundBrightness() const;

private:
	/** Delegate to call when an asset is dropped */
	FOnDrop DroppedEvent;
	/** Delegate to call to check validity of the asset */
	FVerifyDrag AllowDropEvent;
	/** Delegate to call to check validity of the asset */
	FVerifyDrag IsRecognizedEvent;

	/** The color of the vertical/horizontal images when the drop data is valid */
	FLinearColor ValidColor;
	/** The color of the vertical/horizontal images when the drop data is not valid */
	FLinearColor InvalidColor;
	/** The color multiplier that is applied to the background image. */
	FLinearColor BackgroundColor;
	/** The color multiplier that is applied to the background image on hover. */
	FLinearColor BackgroundColorHover;
	/** The background image that is applied after the surface. */
	const FSlateBrush* BackgroundImage;
	/** Vertical border image that is used. */
	const FSlateBrush* VerticalImage;
	/** Horizontal border image that is used. */
	const FSlateBrush* HorizontalImage;

	/** Whether or not we are being dragged over by a recognized event*/
	mutable bool bIsDragEventRecognized;
	/** Whether or not we currently allow dropping */
	mutable bool bAllowDrop;
	/** Is the drag operation currently over our airspace? */
	mutable bool bIsDragOver;
};

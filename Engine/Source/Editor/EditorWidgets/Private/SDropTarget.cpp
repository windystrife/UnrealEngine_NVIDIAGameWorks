// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SDropTarget.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SOverlay.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"


#define LOCTEXT_NAMESPACE "EditorWidgets"

void SDropTarget::Construct(const FArguments& InArgs)
{
	DroppedEvent = InArgs._OnDrop;
	AllowDropEvent = InArgs._OnAllowDrop;
	IsRecognizedEvent = InArgs._OnIsRecognized;

	bIsDragEventRecognized = false;
	bAllowDrop = false;
	bIsDragOver = false;

	ValidColor = InArgs._ValidColor;
	InvalidColor = InArgs._InvalidColor;

	BackgroundColor = InArgs._BackgroundColor;
	BackgroundColorHover = InArgs._BackgroundColorHover;

	VerticalImage = InArgs._VerticalImage;
	HorizontalImage = InArgs._HorizontalImage;

	ChildSlot
	[
		SNew(SOverlay)
			
		+ SOverlay::Slot()
		[
			InArgs._Content.Widget
		]

		+ SOverlay::Slot()
		[
			SNew(SBox)
			.Visibility(this, &SDropTarget::GetDragOverlayVisibility)
			[
				SNew(SBorder)
				.BorderImage(InArgs._BackgroundImage)
				.BorderBackgroundColor(this, &SDropTarget::GetBackgroundBrightness)
			]
		]
	];
}

FSlateColor SDropTarget::GetBackgroundBrightness() const
{
	return ( bIsDragOver ) ? BackgroundColorHover : BackgroundColor;
}

EVisibility SDropTarget::GetDragOverlayVisibility() const
{
	if ( FSlateApplication::Get().IsDragDropping() )
	{
		if ( AllowDrop(FSlateApplication::Get().GetDragDroppingContent()) || (bIsDragOver && bIsDragEventRecognized) )
		{
			return EVisibility::HitTestInvisible;
		}
	}

	return EVisibility::Hidden;
}

FReply SDropTarget::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	// Handle the reply if we are allowed to drop, otherwise do not handle it.
	return AllowDrop(DragDropEvent.GetOperation()) ? FReply::Handled() : FReply::Unhandled();
}

bool SDropTarget::AllowDrop(TSharedPtr<FDragDropOperation> DragDropOperation) const
{
	bAllowDrop = OnAllowDrop(DragDropOperation);
	bIsDragEventRecognized = OnIsRecognized(DragDropOperation) || bAllowDrop;

	return bAllowDrop;
}

bool SDropTarget::OnAllowDrop(TSharedPtr<FDragDropOperation> DragDropOperation) const
{
	if ( AllowDropEvent.IsBound() )
	{
		return AllowDropEvent.Execute(DragDropOperation);
	}

	return false;
}

bool SDropTarget::OnIsRecognized(TSharedPtr<FDragDropOperation> DragDropOperation) const
{
	if ( IsRecognizedEvent.IsBound() )
	{
		return IsRecognizedEvent.Execute(DragDropOperation);
	}

	return false;
}

FReply SDropTarget::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	const bool bCurrentbAllowDrop = bAllowDrop;

	// We've dropped an asset so we are no longer being dragged over
	bIsDragEventRecognized = false;
	bIsDragOver = false;
	bAllowDrop = false;

	// if we allow drop, call a delegate to handle the drop
	if ( bCurrentbAllowDrop )
	{
		if ( DroppedEvent.IsBound() )
		{
			return DroppedEvent.Execute(DragDropEvent.GetOperation());
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SDropTarget::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	// initially we dont recognize this event
	bIsDragEventRecognized = false;
	bIsDragOver = true;
}

void SDropTarget::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	// No longer being dragged over
	bIsDragEventRecognized = false;
	// Disallow dropping if not dragged over.
	bAllowDrop = false;

	bIsDragOver = false;
}

int32 SDropTarget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if ( GetDragOverlayVisibility().IsVisible() )
	{
		if ( bIsDragEventRecognized )
		{
			FLinearColor DashColor = bAllowDrop ? ValidColor : InvalidColor;

			const FSlateBrush* HorizontalBrush = FEditorStyle::GetBrush("WideDash.Horizontal");
			const FSlateBrush* VerticalBrush = FEditorStyle::GetBrush("WideDash.Vertical");

			int32 DashLayer = LayerId + 1;

			// Top
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				DashLayer,
				AllottedGeometry.ToPaintGeometry(FVector2D(0, 0), FVector2D(AllottedGeometry.GetLocalSize().X, HorizontalBrush->ImageSize.Y)),
				HorizontalBrush,
				ESlateDrawEffect::None,
				DashColor);

			// Bottom
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				DashLayer,
				AllottedGeometry.ToPaintGeometry(FVector2D(0, AllottedGeometry.GetLocalSize().Y - HorizontalBrush->ImageSize.Y), FVector2D(AllottedGeometry.Size.X, HorizontalBrush->ImageSize.Y)),
				HorizontalBrush,
				ESlateDrawEffect::None,
				DashColor);

			// Left
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				DashLayer,
				AllottedGeometry.ToPaintGeometry(FVector2D(0, 0), FVector2D(VerticalBrush->ImageSize.X, AllottedGeometry.GetLocalSize().Y)),
				VerticalBrush,
				ESlateDrawEffect::None,
				DashColor);

			// Right
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				DashLayer,
				AllottedGeometry.ToPaintGeometry(FVector2D(AllottedGeometry.GetLocalSize().X - VerticalBrush->ImageSize.X, 0), FVector2D(VerticalBrush->ImageSize.X, AllottedGeometry.GetLocalSize().Y)),
				VerticalBrush,
				ESlateDrawEffect::None,
				DashColor);

			return DashLayer;
		}
	}

	return LayerId;
}

#undef LOCTEXT_NAMESPACE

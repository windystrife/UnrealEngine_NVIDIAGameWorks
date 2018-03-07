// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SOutlinerTreeView.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorStyleSet.h"
#include "ISceneOutlinerColumn.h"
#include "SceneOutlinerPublicTypes.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "DragAndDrop/ActorDragDropGraphEdOp.h"
#include "SceneOutlinerDragDrop.h"
#include "SSceneOutliner.h"

#define LOCTEXT_NAMESPACE "SSceneOutliner"

namespace SceneOutliner
{
	static void UpdateOperationDecorator(const FDragDropEvent& Event, const FDragValidationInfo& ValidationInfo)
	{
		const FSlateBrush* Icon = ValidationInfo.IsValid() ? FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK")) : FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));

		FDragDropOperation* Operation = Event.GetOperation().Get();
		if (Operation)
		{
			if (Operation->IsOfType<FSceneOutlinerDragDropOp>())
			{
				auto* OutlinerOp = static_cast<FSceneOutlinerDragDropOp*>(Operation);
				OutlinerOp->SetTooltip(ValidationInfo.ValidationText, Icon);
			}
			else if (Operation->IsOfType<FActorDragDropGraphEdOp>())
			{
				static_cast<FActorDragDropGraphEdOp*>(Operation)->SetToolTip(ValidationInfo.TooltipType, ValidationInfo.ValidationText);
			}
			else if (Operation->IsOfType<FDecoratedDragDropOp>())
			{
				auto* DecoratedOp = static_cast<FDecoratedDragDropOp*>(Operation);
				DecoratedOp->SetToolTip(ValidationInfo.ValidationText, Icon);
			}
		}
	}

	static void ResetOperationDecorator(const FDragDropEvent& Event)
	{
		FDragDropOperation* Operation = Event.GetOperation().Get();
		if (Operation)
		{
			if (Operation->IsOfType<FSceneOutlinerDragDropOp>())
			{
				static_cast<FSceneOutlinerDragDropOp*>(Operation)->ResetTooltip();
			}
			else if (Operation->IsOfType<FDecoratedDragDropOp>())
			{
				static_cast<FDecoratedDragDropOp*>(Operation)->ResetToDefaultToolTip();
			}
		}
	}

	static FReply OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, TWeakPtr<SOutlinerTreeView> Table )
	{
		auto TablePtr = Table.Pin();
		if (TablePtr.IsValid() && MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ))
		{
			auto Operation = SceneOutliner::CreateDragDropOperation(TablePtr->GetSelectedItems());
			if (Operation.IsValid())
			{
				return FReply::Handled().BeginDragDrop(Operation.ToSharedRef());
			}
		}

		return FReply::Unhandled();
	}

	FReply HandleDrop(TWeakPtr<ISceneOutliner> SceneOutlinerWeak, const FDragDropEvent& DragDropEvent, IDropTarget& DropTarget, FDragValidationInfo& ValidationInfo, bool bApplyDrop = false)
	{
		auto SceneOutlinerPtr = SceneOutlinerWeak.Pin();
		if (!SceneOutlinerPtr.IsValid())
		{
			return FReply::Unhandled();
		}

		const FSharedOutlinerData& SharedData = SceneOutlinerPtr->GetSharedData();
		if (SharedData.Mode != ESceneOutlinerMode::ActorBrowsing)
		{
			return FReply::Unhandled();	
		}

		// Don't handle this if we're not showing a hierarchy, not in browsing mode, or the drop operation is not applicable
		if (!SharedData.bShowParentTree || !SharedData.RepresentingWorld)
		{
			return FReply::Unhandled();
		}

		FDragDropPayload DraggedObjects;
		// Validate now to make sure we don't doing anything we shouldn't
		if (!DraggedObjects.ParseDrag(*DragDropEvent.GetOperation()))
		{
			return FReply::Unhandled();
		}

		ValidationInfo = DropTarget.ValidateDrop(DraggedObjects, *SharedData.RepresentingWorld);

		if (!ValidationInfo.IsValid())
		{
			// Return handled here to stop anything else trying to handle it - the operation is invalid as far as we're concerned
			return FReply::Handled();
		}

		if (bApplyDrop)
		{
			DropTarget.OnDrop(DraggedObjects, *SharedData.RepresentingWorld, ValidationInfo, SceneOutlinerPtr.ToSharedRef());
		}

		return FReply::Handled();
	}

	void SOutlinerTreeView::Construct(const SOutlinerTreeView::FArguments& InArgs, TSharedRef<SSceneOutliner> Owner)
	{
		SceneOutlinerWeak = Owner;
		STreeView::Construct(InArgs);
	}

	void SOutlinerTreeView::FlashHighlightOnItem( FTreeItemPtr FlashHighlightOnItem )
	{
		TSharedPtr< SSceneOutlinerTreeRow > RowWidget = StaticCastSharedPtr< SSceneOutlinerTreeRow >( WidgetGenerator.GetWidgetForItem( FlashHighlightOnItem ) );
		if( RowWidget.IsValid() )
		{
			RowWidget->FlashHighlight();
		}
	}

	FReply SOutlinerTreeView::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
	{
		FDragValidationInfo ValidationInfo = FDragValidationInfo::Invalid();

		FFolderDropTarget DropTarget(NAME_None);

		auto Reply = HandleDrop(SceneOutlinerWeak, DragDropEvent, DropTarget, ValidationInfo);

		if (Reply.IsEventHandled())
		{
			UpdateOperationDecorator(DragDropEvent, ValidationInfo);
			return FReply::Handled();
		}

		return FReply::Unhandled();
	}
	
	void SOutlinerTreeView::OnDragLeave(const FDragDropEvent& DragDropEvent)
	{
		if( SceneOutlinerWeak.Pin()->GetSharedData().bShowParentTree )
		{
			ResetOperationDecorator(DragDropEvent);
		}
	}

	FReply SOutlinerTreeView::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
	{
		FDragValidationInfo ValidationInfo = FDragValidationInfo::Invalid();
		FFolderDropTarget DropTarget(NAME_None);

		return HandleDrop(SceneOutlinerWeak, DragDropEvent, DropTarget, ValidationInfo, true);
	}

	FReply SSceneOutlinerTreeRow::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
	{
		auto ItemPtr = Item.Pin();
		if (ItemPtr.IsValid())
		{
			FDragValidationInfo ValidationInfo = FDragValidationInfo::Invalid();
			return HandleDrop(SceneOutlinerWeak, DragDropEvent, *ItemPtr, ValidationInfo, true);
		}

		return FReply::Unhandled();
	}

	void SSceneOutlinerTreeRow::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
	{
		auto ItemPtr = Item.Pin();
		if (ItemPtr.IsValid())
		{
			FDragValidationInfo ValidationInfo = FDragValidationInfo::Invalid();

			HandleDrop(SceneOutlinerWeak, DragDropEvent, *ItemPtr, ValidationInfo, false);
			UpdateOperationDecorator(DragDropEvent, ValidationInfo);
		}
	}

	void SSceneOutlinerTreeRow::OnDragLeave( const FDragDropEvent& DragDropEvent )
	{
		ResetOperationDecorator(DragDropEvent);
	}

	FReply SSceneOutlinerTreeRow::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
	{
		auto ItemPtr = Item.Pin();
		if (ItemPtr.IsValid() && ItemPtr->CanInteract())
		{
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
			{
				FReply Reply = SMultiColumnTableRow<FTreeItemPtr>::OnMouseButtonDown( MyGeometry, MouseEvent );

				// We only support drag and drop when in actor browsing mode
				if( SceneOutlinerWeak.Pin()->GetSharedData().Mode == ESceneOutlinerMode::ActorBrowsing )
				{
					return Reply.DetectDrag( SharedThis(this) , EKeys::LeftMouseButton );
				}

				return Reply.PreventThrottling();
			}
		}

		return FReply::Handled();
	}
	
	FReply SSceneOutlinerTreeRow::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		auto ItemPtr = Item.Pin();
		if (ItemPtr.IsValid() && ItemPtr->CanInteract())
		{
			return SMultiColumnTableRow<FTreeItemPtr>::OnMouseButtonUp(MyGeometry, MouseEvent);
		}

		return FReply::Handled();
	}

	TSharedRef<SWidget> SSceneOutlinerTreeRow::GenerateWidgetForColumn( const FName& ColumnName )
	{
		auto ItemPtr = Item.Pin();
		if (!ItemPtr.IsValid())
		{
			return SNullWidget::NullWidget;
		}


		auto Outliner = SceneOutlinerWeak.Pin();
		check(Outliner.IsValid());

		// Create the widget for this item
		TSharedRef<SWidget> NewItemWidget = SNullWidget::NullWidget;

		auto Column = Outliner->GetColumns().FindRef(ColumnName);
		if (Column.IsValid())
		{
			NewItemWidget = Column->ConstructRowWidget(ItemPtr.ToSharedRef(), *this);
		}

		if( ColumnName == FBuiltInColumnTypes::Label() )
		{
			// The first column gets the tree expansion arrow for this row
			return 
				SNew( SHorizontalBox )

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6, 0, 0, 0)
				[
					SNew( SExpanderArrow, SharedThis(this) ).IndentAmount(12)
				]

			+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					NewItemWidget
				];
		}
		else
		{
			// Other columns just get widget content -- no expansion arrow needed
			return NewItemWidget;
		}
	}

	void SSceneOutlinerTreeRow::Construct( const FArguments& InArgs, const TSharedRef<SOutlinerTreeView>& OutlinerTreeView, TSharedRef<SSceneOutliner> SceneOutliner )
	{
		Item = InArgs._Item->AsShared();
		SceneOutlinerWeak = SceneOutliner;
		LastHighlightInteractionTime = 0.0;

		auto Args = FSuperRowType::FArguments()
			.Style(&FEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("SceneOutliner.TableViewRow"));

		// We only support drag and drop when in actor browsing mode
		if (SceneOutliner->GetSharedData().Mode == ESceneOutlinerMode::ActorBrowsing)
		{
			Args.OnDragDetected_Static(SceneOutliner::OnDragDetected, TWeakPtr<SOutlinerTreeView>(OutlinerTreeView));
		}

		SMultiColumnTableRow<FTreeItemPtr>::Construct(Args, OutlinerTreeView);
	}

	const float SSceneOutlinerTreeRow::HighlightRectLeftOffset = 0.0f;
	const float SSceneOutlinerTreeRow::HighlightRectRightOffset = 0.0f;
	const float SSceneOutlinerTreeRow::HighlightTargetSpringConstant = 25.0f;
	const float SSceneOutlinerTreeRow::HighlightTargetEffectDuration = 0.5f;
	const float SSceneOutlinerTreeRow::HighlightTargetOpacity = 0.8f;
	const float SSceneOutlinerTreeRow::LabelChangedAnimOffsetPercent = 0.2f;
    
    void SSceneOutlinerTreeRow::FlashHighlight()
    {
        LastHighlightInteractionTime = FSlateApplication::Get().GetCurrentTime();
    }

	void SSceneOutlinerTreeRow::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
	{
		// Call parent implementation.
		SCompoundWidget::Tick( AllottedGeometry, InCurrentTime, InDeltaTime );

		// We'll draw with the 'focused' look if we're either focused or we have a context menu summoned
		const bool bShouldAppearFocused = HasKeyboardFocus();

		// Update highlight 'target' effect
		{
			const float HighlightLeftX = HighlightRectLeftOffset;
			const float HighlightRightX = HighlightRectRightOffset + AllottedGeometry.GetLocalSize().X;

			HighlightTargetLeftSpring.SetTarget( HighlightLeftX );
			HighlightTargetRightSpring.SetTarget( HighlightRightX );

			float TimeSinceHighlightInteraction = (float)( InCurrentTime - LastHighlightInteractionTime );
			if( TimeSinceHighlightInteraction <= HighlightTargetEffectDuration || bShouldAppearFocused )
			{
				HighlightTargetLeftSpring.Tick( InDeltaTime );
				HighlightTargetRightSpring.Tick( InDeltaTime );
			}
		}
	}

	int32 SSceneOutlinerTreeRow::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
	{
		int32 StartLayer = SMultiColumnTableRow::OnPaint( Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled );

		const int32 TextLayer = 1;	

		// See if a disabled effect should be used
		bool bEnabled = ShouldBeEnabled( bParentEnabled );
		ESlateDrawEffect DrawEffects = (bEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

		const double CurrentTime = FSlateApplication::Get().GetCurrentTime();

		// We'll draw with the 'focused' look if we're either focused or we have a context menu summoned
		const bool bShouldAppearFocused = HasKeyboardFocus();

		// Draw highlight targeting effect
		const float TimeSinceHighlightInteraction = (float)( CurrentTime - LastHighlightInteractionTime );
		if( TimeSinceHighlightInteraction <= HighlightTargetEffectDuration )
		{

			// Compute animation progress
			float EffectAlpha = FMath::Clamp( TimeSinceHighlightInteraction / HighlightTargetEffectDuration, 0.0f, 1.0f );
			EffectAlpha = 1.0f - EffectAlpha * EffectAlpha;  // Inverse square falloff (looks nicer!)

			// Apply extra opacity falloff when dehighlighting
			float EffectOpacity = EffectAlpha;

			// Figure out a universally visible highlight color.
			FLinearColor HighlightTargetColorAndOpacity = ( (FLinearColor::White - ColorAndOpacity.Get())*0.5f + FLinearColor(+0.4f, +0.1f, -0.2f)) * InWidgetStyle.GetColorAndOpacityTint();
			HighlightTargetColorAndOpacity.A = HighlightTargetOpacity * EffectOpacity * 255.0f;

			// Compute the bounds offset of the highlight target from where the highlight target spring
			// extents currently lie.  This is used to "grow" or "shrink" the highlight as needed.
			const float LabelChangedAnimOffset = LabelChangedAnimOffsetPercent * AllottedGeometry.GetLocalSize().Y;

			// Choose an offset amount depending on whether we're highlighting, or clearing highlight
			const float EffectOffset = EffectAlpha * LabelChangedAnimOffset;

			const float HighlightLeftX = HighlightTargetLeftSpring.GetPosition() - EffectOffset;
			const float HighlightRightX = HighlightTargetRightSpring.GetPosition() + EffectOffset;
			const float HighlightTopY = 0.0f - LabelChangedAnimOffset;
			const float HighlightBottomY = AllottedGeometry.GetLocalSize().Y + EffectOffset;

			const FVector2D DrawPosition = FVector2D( HighlightLeftX, HighlightTopY );
			const FVector2D DrawSize = FVector2D( HighlightRightX - HighlightLeftX, HighlightBottomY - HighlightTopY );

			const FSlateBrush* StyleInfo = FEditorStyle::GetBrush("SceneOutliner.ChangedItemHighlight");

			// NOTE: We rely on scissor clipping for the highlight rectangle
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + TextLayer,
				AllottedGeometry.ToPaintGeometry( DrawPosition, DrawSize ),	// Position, Size, Scale
				StyleInfo,													// Style
				DrawEffects,												// Effects to use
				HighlightTargetColorAndOpacity );							// Color
		}

		return LayerId + TextLayer;
	}
}

#undef LOCTEXT_NAMESPACE

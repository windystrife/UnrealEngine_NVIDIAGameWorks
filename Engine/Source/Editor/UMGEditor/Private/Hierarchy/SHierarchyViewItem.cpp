// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Hierarchy/SHierarchyViewItem.h"
#include "Components/NamedSlotInterface.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditor.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "EditorFontGlyphs.h"
#include "Application/SlateApplication.h"

#if WITH_EDITOR
	#include "EditorStyleSet.h"
#endif // WITH_EDITOR
#include "Components/PanelWidget.h"

#include "Kismet2/BlueprintEditorUtils.h"

#include "DragAndDrop/DecoratedDragDropOp.h"
#include "WidgetTemplate.h"
#include "DragDrop/WidgetTemplateDragDropOp.h"




#include "Widgets/Text/SInlineEditableTextBlock.h"

#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprintEditorUtils.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "UMG"

/**
*
*/
class FHierarchyWidgetDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FHierarchyWidgetDragDropOp, FDecoratedDragDropOp)

	virtual ~FHierarchyWidgetDragDropOp();

	virtual void OnDrop(bool bDropWasHandled, const FPointerEvent& MouseEvent) override;

	struct FItem
	{
		/** The slot properties for the old slot the widget was in, is used to attempt to reapply the same layout information */
		TMap<FName, FString> ExportedSlotProperties;

		/** The widget being dragged and dropped */
		FWidgetReference Widget;

		/** The original parent of the widget. */
		UWidget* WidgetParent;
	};

	TArray<FItem> DraggedWidgets;

	/** The widget being dragged and dropped */
	FScopedTransaction* Transaction;

	/** Constructs a new drag/drop operation */
	static TSharedRef<FHierarchyWidgetDragDropOp> New(UWidgetBlueprint* Blueprint, const TArray<FWidgetReference>& InWidgets);
};

TSharedRef<FHierarchyWidgetDragDropOp> FHierarchyWidgetDragDropOp::New(UWidgetBlueprint* Blueprint, const TArray<FWidgetReference>& InWidgets)
{
	check(InWidgets.Num() > 0);

	TSharedRef<FHierarchyWidgetDragDropOp> Operation = MakeShareable(new FHierarchyWidgetDragDropOp());

	// Set the display text and the transaction name based on whether we're dragging a single or multiple widgets
	if (InWidgets.Num() == 1)
	{
		Operation->CurrentHoverText = Operation->DefaultHoverText = InWidgets[0].GetTemplate()->GetLabelText();
		Operation->Transaction = new FScopedTransaction(LOCTEXT("Designer_MoveWidget", "Move Widget"));
	}
	else
	{
		Operation->CurrentHoverText = Operation->DefaultHoverText = LOCTEXT("Designer_DragMultipleWidgets", "Multiple Widgets");
		Operation->Transaction = new FScopedTransaction(LOCTEXT("Designer_MoveWidgets", "Move Widgets"));
	}

	// Add an FItem for each widget in the drag operation
	for (const auto& Widget : InWidgets)
	{
		FItem DraggedWidget;

		DraggedWidget.Widget = Widget;

		FWidgetBlueprintEditorUtils::ExportPropertiesToText(Widget.GetTemplate()->Slot, DraggedWidget.ExportedSlotProperties);

		UWidget* WidgetTemplate = Widget.GetTemplate();
		WidgetTemplate->Modify();

		DraggedWidget.WidgetParent = WidgetTemplate->GetParent();
		if (DraggedWidget.WidgetParent)
		{
			DraggedWidget.WidgetParent->Modify();
		}

		Operation->DraggedWidgets.Add(DraggedWidget);
	}

	Operation->Construct();
	
	Blueprint->WidgetTree->SetFlags(RF_Transactional);
	Blueprint->WidgetTree->Modify();

	return Operation;
}

FHierarchyWidgetDragDropOp::~FHierarchyWidgetDragDropOp()
{
	delete Transaction;
}

void FHierarchyWidgetDragDropOp::OnDrop(bool bDropWasHandled, const FPointerEvent& MouseEvent)
{
	if ( !bDropWasHandled )
	{
		Transaction->Cancel();
	}
}

//////////////////////////////////////////////////////////////////////////

TOptional<EItemDropZone> ProcessHierarchyDragDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, bool bIsDrop, TSharedPtr<FWidgetBlueprintEditor> BlueprintEditor, FWidgetReference TargetItem, TOptional<int32> Index = TOptional<int32>())
{
	UWidget* TargetTemplate = TargetItem.GetTemplate();

	if ( TargetTemplate && ( DropZone == EItemDropZone::AboveItem || DropZone == EItemDropZone::BelowItem ) )
	{
		if ( UPanelWidget* TargetParentTemplate = TargetTemplate->GetParent() )
		{
			int32 InsertIndex = TargetParentTemplate->GetChildIndex(TargetTemplate);
			InsertIndex += ( DropZone == EItemDropZone::AboveItem ) ? 0 : 1;
			InsertIndex = FMath::Clamp(InsertIndex, 0, TargetParentTemplate->GetChildrenCount());

			FWidgetReference TargetParentTemplateRef = BlueprintEditor->GetReferenceFromTemplate(TargetParentTemplate);
			TOptional<EItemDropZone> ParentZone = ProcessHierarchyDragDrop(DragDropEvent, EItemDropZone::OntoItem, bIsDrop, BlueprintEditor, TargetParentTemplateRef, InsertIndex);
			if ( ParentZone.IsSet() )
			{
				return DropZone;
			}
			else
			{
				DropZone = EItemDropZone::OntoItem;
			}
		}
	}
	else
	{
		DropZone = EItemDropZone::OntoItem;
	}

	UWidgetBlueprint* Blueprint = BlueprintEditor->GetWidgetBlueprintObj();
	check( Blueprint != nullptr && Blueprint->WidgetTree != nullptr );

	// Is this a drag/drop op to create a new widget in the tree?
	TSharedPtr<FWidgetTemplateDragDropOp> TemplateDragDropOp = DragDropEvent.GetOperationAs<FWidgetTemplateDragDropOp>();
	if ( TemplateDragDropOp.IsValid() )
	{
		TemplateDragDropOp->ResetToDefaultToolTip();

		// Are we adding to the root?
		if ( !TargetItem.IsValid() && Blueprint->WidgetTree->RootWidget == nullptr )
		{
			// TODO UMG Allow showing a preview of this.
			if ( bIsDrop )
			{
				FScopedTransaction Transaction(LOCTEXT("AddWidgetFromTemplate", "Add Widget"));

				Blueprint->WidgetTree->SetFlags(RF_Transactional);
				Blueprint->WidgetTree->Modify();

				Blueprint->WidgetTree->RootWidget = TemplateDragDropOp->Template->Create(Blueprint->WidgetTree);
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			}

			TemplateDragDropOp->CurrentIconBrush = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"));
			return EItemDropZone::OntoItem;
		}
		// Are we adding to a panel?
		else if ( UPanelWidget* Parent = Cast<UPanelWidget>(TargetItem.GetTemplate()) )
		{
			if (!Parent->CanAddMoreChildren())
			{
				TemplateDragDropOp->CurrentHoverText = LOCTEXT("NoAdditionalChildren", "Widget can't accept additional children.");
			}
			else
			{
				// TODO UMG Allow showing a preview of this.
				if (bIsDrop)
				{
					FScopedTransaction Transaction(LOCTEXT("AddWidgetFromTemplate", "Add Widget"));

					Blueprint->WidgetTree->SetFlags(RF_Transactional);
					Blueprint->WidgetTree->Modify();
					Parent->Modify();

					UWidget* Widget = TemplateDragDropOp->Template->Create(Blueprint->WidgetTree);

					UPanelSlot* NewSlot = nullptr;
					if (Index.IsSet())
					{
						NewSlot = Parent->InsertChildAt(Index.GetValue(), Widget);
					}
					else
					{
						NewSlot = Parent->AddChild(Widget);
					}
					check(NewSlot);

					FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
				}

				TemplateDragDropOp->CurrentIconBrush = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"));
				return EItemDropZone::OntoItem;
			}
		}
		else
		{
			TemplateDragDropOp->CurrentHoverText = LOCTEXT("CantHaveChildren", "Widget can't have children.");
		}

		TemplateDragDropOp->CurrentIconBrush = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
		return TOptional<EItemDropZone>();
	}

	TSharedPtr<FHierarchyWidgetDragDropOp> HierarchyDragDropOp = DragDropEvent.GetOperationAs<FHierarchyWidgetDragDropOp>();
	if ( HierarchyDragDropOp.IsValid() )
	{
		HierarchyDragDropOp->ResetToDefaultToolTip();

		// If the target item is valid we're dealing with a normal widget in the hierarchy, otherwise we should assume it's
		// the null case and we should be adding it as the root widget.
		if ( TargetItem.IsValid() )
		{
			const bool bIsDraggedObject = HierarchyDragDropOp->DraggedWidgets.ContainsByPredicate([TargetItem](const FHierarchyWidgetDragDropOp::FItem& DraggedItem)
			{
				return DraggedItem.Widget == TargetItem;
			});

			if ( bIsDraggedObject )
			{
				HierarchyDragDropOp->CurrentIconBrush = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
				return TOptional<EItemDropZone>();
			}

			UPanelWidget* NewParent = Cast<UPanelWidget>(TargetItem.GetTemplate());
			if (!NewParent)
			{
				HierarchyDragDropOp->CurrentIconBrush = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
				HierarchyDragDropOp->CurrentHoverText = LOCTEXT("CantHaveChildren", "Widget can't have children.");
				return TOptional<EItemDropZone>();
			}

			if (!NewParent->CanAddMoreChildren())
			{
				HierarchyDragDropOp->CurrentIconBrush = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
				HierarchyDragDropOp->CurrentHoverText = LOCTEXT("NoAdditionalChildren", "Widget can't accept additional children.");
				return TOptional<EItemDropZone>();
			}

			if (!NewParent->CanHaveMultipleChildren() && HierarchyDragDropOp->DraggedWidgets.Num() > 1)
			{
				HierarchyDragDropOp->CurrentIconBrush = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
				HierarchyDragDropOp->CurrentHoverText = LOCTEXT("CantHaveMultipleChildren", "Widget can't have multiple children.");
				return TOptional<EItemDropZone>();
			}

			bool bFoundNewParentInChildSet = false;
			for (const auto& DraggedWidget : HierarchyDragDropOp->DraggedWidgets)
			{
				UWidget* TemplateWidget = DraggedWidget.Widget.GetTemplate();

				// Verify that the new location we're placing the widget is not inside of its existing children.
				Blueprint->WidgetTree->ForWidgetAndChildren(TemplateWidget, [&](UWidget* Widget) {
					if (NewParent == Widget)
					{
						bFoundNewParentInChildSet = true;
					}
				});
			}

			if (bFoundNewParentInChildSet)
			{
				HierarchyDragDropOp->CurrentIconBrush = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
				HierarchyDragDropOp->CurrentHoverText = LOCTEXT("CantMakeWidgetChildOfChildren", "Can't make widget a child of its children.");
				return TOptional<EItemDropZone>();
			}

			if ( bIsDrop )
			{
				NewParent->SetFlags(RF_Transactional);
				NewParent->Modify();

				TSet<FWidgetReference> SelectedTemplates;

				for (const auto& DraggedWidget : HierarchyDragDropOp->DraggedWidgets)
				{
					UWidget* TemplateWidget = DraggedWidget.Widget.GetTemplate();

					if (Index.IsSet())
					{
						// If we're inserting at an index, and the widget we're moving is already
						// in the hierarchy before the point we're moving it to, we need to reduce the index
						// count by one, because the whole set is about to be shifted when it's removed.
						const bool bInsertInSameParent = TemplateWidget->GetParent() == NewParent;
						const bool bNeedToDropIndex = NewParent->GetChildIndex(TemplateWidget) < Index.GetValue();

						if (bInsertInSameParent && bNeedToDropIndex)
						{
							Index = Index.GetValue() - 1;
						}
					}

					// We don't know if this widget is being removed from a named slot and RemoveFromParent is not enough to take care of this
					UWidget* NamedSlotHostWidget = FWidgetBlueprintEditorUtils::FindNamedSlotHostWidgetForContent(TemplateWidget, Blueprint->WidgetTree);
					if (NamedSlotHostWidget != nullptr)
					{
						INamedSlotInterface* NamedSlotHost = Cast<INamedSlotInterface>(NamedSlotHostWidget);
						if (NamedSlotHost != nullptr)
						{
							NamedSlotHostWidget->SetFlags(RF_Transactional);
							NamedSlotHostWidget->Modify();
							FWidgetBlueprintEditorUtils::RemoveNamedSlotHostContent(TemplateWidget, NamedSlotHost);
						}
					}

					UPanelWidget* OriginalParent = TemplateWidget->GetParent();
					UBlueprint* OriginalBP = nullptr;

					// The widget's parent is changing
					if (OriginalParent != NewParent)
					{
						NewParent->SetFlags(RF_Transactional);
						NewParent->Modify();

						Blueprint->WidgetTree->SetFlags(RF_Transactional);
						Blueprint->WidgetTree->Modify();

						UWidgetTree* OriginalWidgetTree = Cast<UWidgetTree>(TemplateWidget->GetOuter());

						if (OriginalWidgetTree != nullptr && UWidgetTree::TryMoveWidgetToNewTree(TemplateWidget, Blueprint->WidgetTree))
						{
							OriginalWidgetTree->SetFlags(RF_Transactional);
							OriginalWidgetTree->Modify();

							OriginalBP = OriginalWidgetTree->GetTypedOuter<UBlueprint>();
						}
					}

					TemplateWidget->RemoveFromParent();

					if (OriginalBP != nullptr && OriginalBP != Blueprint)
					{
						FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(OriginalBP);
					}

					UPanelSlot* NewSlot = nullptr;
					if (Index.IsSet())
					{
						NewSlot = NewParent->InsertChildAt(Index.GetValue(), TemplateWidget);
						Index = Index.GetValue() + 1;
					}
					else
					{
						NewSlot = NewParent->AddChild(TemplateWidget);
					}
					check(NewSlot);

					// Import the old slot properties
					FWidgetBlueprintEditorUtils::ImportPropertiesFromText(NewSlot, DraggedWidget.ExportedSlotProperties);

					SelectedTemplates.Add(BlueprintEditor->GetReferenceFromTemplate(TemplateWidget));
				}

				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
				BlueprintEditor->SelectWidgets(SelectedTemplates, false);
			}

			HierarchyDragDropOp->CurrentIconBrush = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"));
			return EItemDropZone::OntoItem;
		}
		else
		{
			HierarchyDragDropOp->CurrentIconBrush = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
			HierarchyDragDropOp->CurrentHoverText = LOCTEXT("CantHaveChildren", "Widget can't have children.");
		}

		return TOptional<EItemDropZone>();
	}

	return TOptional<EItemDropZone>();
}


//////////////////////////////////////////////////////////////////////////

FHierarchyModel::FHierarchyModel(TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor)
	: bInitialized(false)
	, bIsSelected(false)
	, BlueprintEditor(InBlueprintEditor)
{

}

TOptional<EItemDropZone> FHierarchyModel::HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone)
{
	return TOptional<EItemDropZone>();
}

FReply FHierarchyModel::HandleDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!IsRoot())
	{
		TArray<FWidgetReference> DraggedItems;

		// Dragging multiple items?
		if (bIsSelected)
		{
			const TSet<FWidgetReference>& SelectedWidgets = BlueprintEditor.Pin()->GetSelectedWidgets();
			if (SelectedWidgets.Num() > 1)
			{
				for (const auto& SelectedWidget : SelectedWidgets)
				{
					DraggedItems.Add(SelectedWidget);
				}
			}
		}

		if (DraggedItems.Num() == 0)
		{
			FWidgetReference ThisItem = AsDraggedWidgetReference();
			if (ThisItem.IsValid())
			{
				DraggedItems.Add(ThisItem);
			}
		}

		if (DraggedItems.Num() > 0)
		{
			return FReply::Handled().BeginDragDrop(FHierarchyWidgetDragDropOp::New(BlueprintEditor.Pin()->GetWidgetBlueprintObj(), DraggedItems));
		}
	}

	return FReply::Unhandled();
}

void FHierarchyModel::HandleDragEnter(const FDragDropEvent& DragDropEvent)
{

}

void FHierarchyModel::HandleDragLeave(const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FDecoratedDragDropOp> DecoratedDragDropOp = DragDropEvent.GetOperationAs<FDecoratedDragDropOp>();
	if ( DecoratedDragDropOp.IsValid() )
	{
		DecoratedDragDropOp->ResetToDefaultToolTip();
	}
}

FReply FHierarchyModel::HandleAcceptDrop(FDragDropEvent const& DragDropEvent, EItemDropZone DropZone)
{
	return FReply::Unhandled();
}

bool FHierarchyModel::OnVerifyNameTextChanged(const FText& InText, FText& OutErrorMessage)
{
	return false;
}

void FHierarchyModel::OnNameTextCommited(const FText& InText, ETextCommit::Type CommitInfo)
{

}

void FHierarchyModel::InitializeChildren()
{
	if ( !bInitialized )
	{
		bInitialized = true;
		GetChildren(Models);
	}
}

void FHierarchyModel::GatherChildren(TArray< TSharedPtr<FHierarchyModel> >& Children)
{
	InitializeChildren();

	Children.Append(Models);
}

bool FHierarchyModel::ContainsSelection()
{
	InitializeChildren();

	for ( TSharedPtr<FHierarchyModel>& Model : Models )
	{
		if ( Model->IsSelected() || Model->ContainsSelection() )
		{
			return true;
		}
	}

	return false;
}

void FHierarchyModel::RefreshSelection()
{
	InitializeChildren();

	UpdateSelection();

	for ( TSharedPtr<FHierarchyModel>& Model : Models )
	{
		Model->RefreshSelection();
	}
}

bool FHierarchyModel::IsSelected() const
{
	return bIsSelected;
}

//////////////////////////////////////////////////////////////////////////

FHierarchyRoot::FHierarchyRoot(TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor)
	: FHierarchyModel(InBlueprintEditor)
{
	RootText = FText::Format(LOCTEXT("RootWidgetFormat", "[{0}]"), FText::FromString(BlueprintEditor.Pin()->GetBlueprintObj()->GetName()));
}

FName FHierarchyRoot::GetUniqueName() const
{
	static const FName DesignerRootName(TEXT("WidgetDesignerRoot"));
	return DesignerRootName;
}

FText FHierarchyRoot::GetText() const
{
	return RootText;
}

const FSlateBrush* FHierarchyRoot::GetImage() const
{
	return nullptr;
}

FSlateFontInfo FHierarchyRoot::GetFont() const
{
	return FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 10);
}

void FHierarchyRoot::GetChildren(TArray< TSharedPtr<FHierarchyModel> >& Children)
{
	TSharedPtr<FWidgetBlueprintEditor> BPEd = BlueprintEditor.Pin();
	UWidgetBlueprint* Blueprint = BPEd->GetWidgetBlueprintObj();

	if ( Blueprint->WidgetTree->RootWidget )
	{
		TSharedPtr<FHierarchyWidget> RootChild = MakeShareable(new FHierarchyWidget(BPEd->GetReferenceFromTemplate(Blueprint->WidgetTree->RootWidget), BPEd));
		Children.Add(RootChild);
	}
}

void FHierarchyRoot::OnSelection()
{
	TSharedPtr<FWidgetBlueprintEditor> BPEd = BlueprintEditor.Pin();
	if ( UWidget* Default = BPEd->GetWidgetBlueprintObj()->GeneratedClass->GetDefaultObject<UWidget>() )
	{
		TSet<UObject*> SelectedObjects;

		//Switched from adding CDO to adding the preview, so that the root (owner) widget can be properly animate
		UUserWidget* PreviewWidget = BPEd->GetPreview();
		SelectedObjects.Add(PreviewWidget);

		BPEd->SelectObjects(SelectedObjects);
	}
}

void FHierarchyRoot::UpdateSelection()
{
	TSharedPtr<FWidgetBlueprintEditor> BPEd = BlueprintEditor.Pin();
	if ( UWidget* Default = BPEd->GetWidgetBlueprintObj()->GeneratedClass->GetDefaultObject<UWidget>() )
	{
		const TSet< TWeakObjectPtr<UObject> >& SelectedObjects = BlueprintEditor.Pin()->GetSelectedObjects();

		TWeakObjectPtr<UObject> PreviewWidget = BPEd->GetPreview();

		bIsSelected = SelectedObjects.Contains(PreviewWidget);
	}
	else
	{
		bIsSelected = false;
	}
}

TOptional<EItemDropZone> FHierarchyRoot::HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone)
{
	bool bIsDrop = false;
	return ProcessHierarchyDragDrop(DragDropEvent, DropZone, bIsDrop, BlueprintEditor.Pin(), FWidgetReference());
}

FReply FHierarchyRoot::HandleAcceptDrop(FDragDropEvent const& DragDropEvent, EItemDropZone DropZone)
{
	bool bIsDrop = true;
	TOptional<EItemDropZone> Zone = ProcessHierarchyDragDrop(DragDropEvent, DropZone, bIsDrop, BlueprintEditor.Pin(), FWidgetReference());
	return Zone.IsSet() ? FReply::Handled() : FReply::Unhandled();
}

//////////////////////////////////////////////////////////////////////////

FNamedSlotModel::FNamedSlotModel(FWidgetReference InItem, FName InSlotName, TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor)
	: FHierarchyModel(InBlueprintEditor)
	, Item(InItem)
	, SlotName(InSlotName)
{
}

FName FNamedSlotModel::GetUniqueName() const
{
	UWidget* WidgetTemplate = Item.GetTemplate();
	if ( WidgetTemplate )
	{
		FString UniqueSlot = WidgetTemplate->GetName() + TEXT(".") + SlotName.ToString();
		return FName(*UniqueSlot);
	}

	return NAME_None;
}

FText FNamedSlotModel::GetText() const
{
	if ( INamedSlotInterface* NamedSlotHost = Cast<INamedSlotInterface>(Item.GetTemplate()) )
	{
		TSet<FWidgetReference> SelectedWidgets;
		if ( UWidget* SlotContent = NamedSlotHost->GetContentForSlot(SlotName) )
		{
			return FText::Format(LOCTEXT("NamedSlotTextFormat", "{0} ({1})"), FText::FromName(SlotName), FText::FromName(SlotContent->GetFName()));
		}
	}

	return FText::FromName(SlotName);
}

const FSlateBrush* FNamedSlotModel::GetImage() const
{
	return NULL;
}

FSlateFontInfo FNamedSlotModel::GetFont() const
{
	return FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 10);
}

void FNamedSlotModel::GetChildren(TArray< TSharedPtr<FHierarchyModel> >& Children)
{
	TSharedPtr<FWidgetBlueprintEditor> BPEd = BlueprintEditor.Pin();
	if ( INamedSlotInterface* NamedSlotHost = Cast<INamedSlotInterface>(Item.GetTemplate()) )
	{
		TSet<FWidgetReference> SelectedWidgets;
		if ( UWidget* TemplateSlotContent = NamedSlotHost->GetContentForSlot(SlotName) )
		{
			TSharedPtr<FHierarchyWidget> RootChild = MakeShareable(new FHierarchyWidget(BPEd->GetReferenceFromTemplate(TemplateSlotContent), BPEd));
			Children.Add(RootChild);
		}
	}
}

void FNamedSlotModel::OnSelection()
{
	TSharedPtr<FWidgetBlueprintEditor> Editor = BlueprintEditor.Pin();

	FNamedSlotSelection Selection;
	Selection.NamedSlotHostWidget = Item;
	Selection.SlotName = SlotName;
	Editor->SetSelectedNamedSlot(Selection);
}

void FNamedSlotModel::UpdateSelection()
{
	//bIsSelected = false;

	//const TSet<FWidgetReference>& SelectedWidgets = BlueprintEditor.Pin()->GetSelectedWidgets();

	//TSharedPtr<FWidgetBlueprintEditor> BPEd = BlueprintEditor.Pin();
	//if ( INamedSlotInterface* NamedSlotHost = Cast<INamedSlotInterface>(Item.GetTemplate()) )
	//{
	//	if ( UWidget* TemplateSlotContent = NamedSlotHost->GetContentForSlot(SlotName) )
	//	{
	//		bIsSelected = SelectedWidgets.Contains(BPEd->GetReferenceFromTemplate(TemplateSlotContent));
	//	}
	//}
}

FWidgetReference FNamedSlotModel::AsDraggedWidgetReference() const
{
	if (INamedSlotInterface* NamedSlotHost = Cast<INamedSlotInterface>(Item.GetTemplate()))
	{
		// Only assign content to the named slot if it is null.
		if (UWidget* Content = NamedSlotHost->GetContentForSlot(SlotName))
		{
			return BlueprintEditor.Pin()->GetReferenceFromTemplate(Content);
		}
	}

	return FWidgetReference();
}

TOptional<EItemDropZone> FNamedSlotModel::HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone)
{
	UWidgetBlueprint* Blueprint = BlueprintEditor.Pin()->GetWidgetBlueprintObj();

	TSharedPtr<FWidgetTemplateDragDropOp> TemplateDragDropOp = DragDropEvent.GetOperationAs<FWidgetTemplateDragDropOp>();
	if (TemplateDragDropOp.IsValid())
	{
		TemplateDragDropOp->ResetToDefaultToolTip();

		if ( INamedSlotInterface* NamedSlotHost = Cast<INamedSlotInterface>(Item.GetTemplate()) )
		{
			// Only assign content to the named slot if it is null.
			if ( NamedSlotHost->GetContentForSlot(SlotName) != nullptr )
			{
				TemplateDragDropOp->CurrentIconBrush = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
				TemplateDragDropOp->CurrentHoverText = LOCTEXT("NamedSlotAlreadyFull", "Named Slot already has a child.");
				return TOptional<EItemDropZone>();
			}

			TemplateDragDropOp->CurrentIconBrush = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"));
			return EItemDropZone::OntoItem;
		}
	}

	TSharedPtr<FHierarchyWidgetDragDropOp> HierarchyDragDropOp = DragDropEvent.GetOperationAs<FHierarchyWidgetDragDropOp>();
	if (HierarchyDragDropOp.IsValid() && HierarchyDragDropOp->DraggedWidgets.Num() == 1)
	{
		HierarchyDragDropOp->ResetToDefaultToolTip();

		if (INamedSlotInterface* NamedSlotHost = Cast<INamedSlotInterface>(Item.GetTemplate()))
		{
			// Only assign content to the named slot if it is null.
			if (NamedSlotHost->GetContentForSlot(SlotName) != nullptr)
			{
				HierarchyDragDropOp->CurrentIconBrush = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
				HierarchyDragDropOp->CurrentHoverText = LOCTEXT("NamedSlotAlreadyFull", "Named Slot already has a child.");
				return TOptional<EItemDropZone>();
			}

			bool bFoundNewParentInChildSet = false;
			UWidget* TemplateWidget = HierarchyDragDropOp->DraggedWidgets[0].Widget.GetTemplate();

			// Verify that the new location we're placing the widget is not inside of its existing children.
			Blueprint->WidgetTree->ForWidgetAndChildren(TemplateWidget, [&](UWidget* Widget) {
				if (Item.GetTemplate() == Widget)
				{
					bFoundNewParentInChildSet = true;
				}
			});

			if (bFoundNewParentInChildSet)
			{
				HierarchyDragDropOp->CurrentIconBrush = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
				HierarchyDragDropOp->CurrentHoverText = LOCTEXT("CantMakeWidgetChildOfChildren", "Can't make widget a child of its children.");
				return TOptional<EItemDropZone>();
			}

			HierarchyDragDropOp->CurrentIconBrush = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"));
			return EItemDropZone::OntoItem;
		}
	}

	return TOptional<EItemDropZone>();
}

FReply FNamedSlotModel::HandleAcceptDrop(FDragDropEvent const& DragDropEvent, EItemDropZone DropZone)
{
	UWidget* SlotHostWidget = Item.GetTemplate();
	INamedSlotInterface* NamedSlotHost = Cast<INamedSlotInterface>(SlotHostWidget);
	if (NamedSlotHost == nullptr)
	{
		return FReply::Unhandled();
	}
	if (NamedSlotHost->GetContentForSlot(SlotName) != nullptr)
	{
		return FReply::Unhandled();
	}

	TSharedPtr<FWidgetTemplateDragDropOp> TemplateDragDropOp = DragDropEvent.GetOperationAs<FWidgetTemplateDragDropOp>();
	if (TemplateDragDropOp.IsValid())
	{
		FScopedTransaction Transaction(LOCTEXT("AddWidgetFromTemplate", "Add Widget"));

		UWidgetBlueprint* Blueprint = BlueprintEditor.Pin()->GetWidgetBlueprintObj();
		Blueprint->WidgetTree->SetFlags(RF_Transactional);
		Blueprint->WidgetTree->Modify();

		UWidget* DroppingWidget = TemplateDragDropOp->Template->Create(Blueprint->WidgetTree);

		DoDrop(SlotHostWidget, DroppingWidget);

		return FReply::Handled();
	}

	TSharedPtr<FHierarchyWidgetDragDropOp> HierarchyDragDropOp = DragDropEvent.GetOperationAs<FHierarchyWidgetDragDropOp>();
	if (HierarchyDragDropOp.IsValid() && HierarchyDragDropOp->DraggedWidgets.Num() == 1)
	{
		UWidgetBlueprint* Blueprint = BlueprintEditor.Pin()->GetWidgetBlueprintObj();
		Blueprint->WidgetTree->SetFlags(RF_Transactional);
		Blueprint->WidgetTree->Modify();

		UWidget* DroppingWidget = HierarchyDragDropOp->DraggedWidgets[0].Widget.GetTemplate();

		// We don't know if this widget is being removed from a named slot and RemoveFromParent is not enough to take care of this
		UWidget* SourceNamedSlotHostWidget = FWidgetBlueprintEditorUtils::FindNamedSlotHostWidgetForContent(DroppingWidget, Blueprint->WidgetTree);
		if (SourceNamedSlotHostWidget != nullptr)
		{
			INamedSlotInterface* SourceNamedSlotHost = Cast<INamedSlotInterface>(SourceNamedSlotHostWidget);
			if (SourceNamedSlotHost != nullptr)
			{
				SourceNamedSlotHostWidget->SetFlags(RF_Transactional);
				SourceNamedSlotHostWidget->Modify();
				FWidgetBlueprintEditorUtils::RemoveNamedSlotHostContent(DroppingWidget, SourceNamedSlotHost);
			}
		}

		DroppingWidget->RemoveFromParent();

		DoDrop(SlotHostWidget, DroppingWidget);

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void FNamedSlotModel::DoDrop(UWidget* NamedSlotHostWidget, UWidget* DroppingWidget)
{
	UWidgetBlueprint* Blueprint = BlueprintEditor.Pin()->GetWidgetBlueprintObj();

	NamedSlotHostWidget->SetFlags(RF_Transactional);
	NamedSlotHostWidget->Modify();

	INamedSlotInterface* NamedSlotInterface = Cast<INamedSlotInterface>(NamedSlotHostWidget);
	NamedSlotInterface->SetContentForSlot(SlotName, DroppingWidget);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSet<FWidgetReference> SelectedTemplates;
	SelectedTemplates.Add(BlueprintEditor.Pin()->GetReferenceFromTemplate(DroppingWidget));

	BlueprintEditor.Pin()->SelectWidgets(SelectedTemplates, false);
}

//////////////////////////////////////////////////////////////////////////

FHierarchyWidget::FHierarchyWidget(FWidgetReference InItem, TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor)
	: FHierarchyModel(InBlueprintEditor)
	, Item(InItem)
	, bEditing(false)
{
}

FName FHierarchyWidget::GetUniqueName() const
{
	UWidget* WidgetTemplate = Item.GetTemplate();
	if ( WidgetTemplate )
	{
		return WidgetTemplate->GetFName();
	}

	return NAME_None;
}

FText FHierarchyWidget::GetText() const
{
	UWidget* WidgetTemplate = Item.GetTemplate();
	if ( WidgetTemplate )
	{
		return bEditing ? WidgetTemplate->GetLabelText() : WidgetTemplate->GetLabelTextWithMetadata();
	}

	return FText::GetEmpty();
}

FText FHierarchyWidget::GetImageToolTipText() const
{
	UWidget* WidgetTemplate = Item.GetTemplate();
	if ( WidgetTemplate )
	{
		UClass* WidgetClass = WidgetTemplate->GetClass();
		if ( WidgetClass->IsChildOf( UUserWidget::StaticClass() ) )
		{
			auto& Description = Cast<UWidgetBlueprint>( WidgetClass->ClassGeneratedBy )->BlueprintDescription;
			if ( Description.Len() > 0 )
			{
				return FText::FromString( Description );
			}
		}
		
		return WidgetClass->GetToolTipText();
	}
	
	return FText::GetEmpty();
}

FText FHierarchyWidget::GetLabelToolTipText() const
{
	// If the user has provided a name, give a tooltip with the widget type for easy reference
	UWidget* WidgetTemplate = Item.GetTemplate();
	if ( WidgetTemplate && !WidgetTemplate->IsGeneratedName() )
	{
		return FText::FromString(TEXT( "[" ) + WidgetTemplate->GetClass()->GetDisplayNameText().ToString() + TEXT( "]" ) );
	}

	return FText::GetEmpty();
}

const FSlateBrush* GetEditorIcon_Deprecated(UWidget* Widget);

const FSlateBrush* FHierarchyWidget::GetImage() const
{
	// @todo UMG: remove after 4.12
	return GetEditorIcon_Deprecated(Item.GetTemplate());
	// return FClassIconFinder::FindIconForClass(Item.GetTemplate()->GetClass());
}

FSlateFontInfo FHierarchyWidget::GetFont() const
{
	UWidget* WidgetTemplate = Item.GetTemplate();
	if ( WidgetTemplate )
	{
		if ( !WidgetTemplate->IsGeneratedName() && WidgetTemplate->bIsVariable )
		{
			// TODO UMG Hacky move into style area
			return FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 10);
		}
	}

	static FName NormalFont("NormalFont");
	return FCoreStyle::Get().GetFontStyle(NormalFont);
}

TOptional<EItemDropZone> FHierarchyWidget::HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone)
{
	bool bIsDrop = false;
	return ProcessHierarchyDragDrop(DragDropEvent, DropZone, bIsDrop, BlueprintEditor.Pin(), Item);
}

void FHierarchyWidget::HandleDragLeave(const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FDecoratedDragDropOp> DecoratedDragDropOp = DragDropEvent.GetOperationAs<FDecoratedDragDropOp>();
	if ( DecoratedDragDropOp.IsValid() )
	{
		DecoratedDragDropOp->ResetToDefaultToolTip();
	}
}

FReply FHierarchyWidget::HandleAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone)
{
	bool bIsDrop = true;
	TOptional<EItemDropZone> Zone = ProcessHierarchyDragDrop(DragDropEvent, DropZone, bIsDrop, BlueprintEditor.Pin(), Item);
	return Zone.IsSet() ? FReply::Handled() : FReply::Unhandled();
}

bool FHierarchyWidget::OnVerifyNameTextChanged(const FText& InText, FText& OutErrorMessage)
{
	return FWidgetBlueprintEditorUtils::VerifyWidgetRename(BlueprintEditor.Pin().ToSharedRef(), Item, InText, OutErrorMessage);
}

void FHierarchyWidget::OnNameTextCommited(const FText& InText, ETextCommit::Type CommitInfo)
{
	FWidgetBlueprintEditorUtils::RenameWidget(BlueprintEditor.Pin().ToSharedRef(), Item.GetTemplate()->GetFName(), InText.ToString());
}

void FHierarchyWidget::GetChildren(TArray< TSharedPtr<FHierarchyModel> >& Children)
{
	TSharedPtr<FWidgetBlueprintEditor> BPEd = BlueprintEditor.Pin();

	// Check for named slots
	if ( INamedSlotInterface* NamedSlotHost = Cast<INamedSlotInterface>(Item.GetTemplate()) )
	{
		TArray<FName> SlotNames;
		NamedSlotHost->GetSlotNames(SlotNames);

		for ( FName& SlotName : SlotNames )
		{
			TSharedPtr<FNamedSlotModel> ChildItem = MakeShareable(new FNamedSlotModel(Item, SlotName, BPEd));
			Children.Add(ChildItem);
		}
	}
	
	// Check if it's a panel widget that can support children
	if ( UPanelWidget* PanelWidget = Cast<UPanelWidget>(Item.GetTemplate()) )
	{
		for ( int32 i = 0; i < PanelWidget->GetChildrenCount(); i++ )
		{
			UWidget* Child = PanelWidget->GetChildAt(i);
			if ( Child )
			{
				TSharedPtr<FHierarchyWidget> ChildItem = MakeShareable(new FHierarchyWidget(BPEd->GetReferenceFromTemplate(Child), BPEd));
				Children.Add(ChildItem);
			}
		}
	}
}

void FHierarchyWidget::OnSelection()
{
	TSet<FWidgetReference> SelectedWidgets;
	SelectedWidgets.Add(Item);

	BlueprintEditor.Pin()->SelectWidgets(SelectedWidgets, true);
}

void FHierarchyWidget::OnMouseEnter()
{
	BlueprintEditor.Pin()->SetHoveredWidget(Item);
}

void FHierarchyWidget::OnMouseLeave()
{
	BlueprintEditor.Pin()->ClearHoveredWidget();
}

bool FHierarchyWidget::IsHovered() const
{
	return BlueprintEditor.Pin()->GetHoveredWidget() == Item;
}

void FHierarchyWidget::UpdateSelection()
{
	const TSet<FWidgetReference>& SelectedWidgets = BlueprintEditor.Pin()->GetSelectedWidgets();
	bIsSelected = SelectedWidgets.Contains(Item);
}

bool FHierarchyWidget::CanRename() const
{
	return true;
}

void FHierarchyWidget::RequestBeginRename()
{
	RenameEvent.ExecuteIfBound();
}

void FHierarchyWidget::OnBeginEditing()
{
	bEditing = true;
}

void FHierarchyWidget::OnEndEditing()
{
	bEditing = false;
}

//////////////////////////////////////////////////////////////////////////

void SHierarchyViewItem::Construct(const FArguments& InArgs, const TSharedRef< STableViewBase >& InOwnerTableView, TSharedPtr<FHierarchyModel> InModel)
{
	Model = InModel;
	Model->RenameEvent.BindSP(this, &SHierarchyViewItem::OnRequestBeginRename);

	STableRow< TSharedPtr<FHierarchyModel> >::Construct(
		STableRow< TSharedPtr<FHierarchyModel> >::FArguments()
		.OnCanAcceptDrop(this, &SHierarchyViewItem::HandleCanAcceptDrop)
		.OnAcceptDrop(this, &SHierarchyViewItem::HandleAcceptDrop)
		.OnDragDetected(this, &SHierarchyViewItem::HandleDragDetected)
		.OnDragEnter(this, &SHierarchyViewItem::HandleDragEnter)
		.OnDragLeave(this, &SHierarchyViewItem::HandleDragLeave)
		.Padding(0.0f)
		.Content()
		[
			SNew(SHorizontalBox)
			
			// Widget icon
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.ColorAndOpacity(FLinearColor(1,1,1,0.5))
				.Image(Model->GetImage())
				.ToolTipText(Model->GetImageToolTipText())
			]

			// Name of the widget
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(2, 0, 0, 0)
			.VAlign(VAlign_Center)
			[
				SAssignNew(EditBox, SInlineEditableTextBlock)
				.Font(this, &SHierarchyViewItem::GetItemFont)
				.Text(this, &SHierarchyViewItem::GetItemText)
				.ToolTipText(Model->GetLabelToolTipText())
				.HighlightText(InArgs._HighlightText)
				.IsReadOnly(this, &SHierarchyViewItem::IsReadOnly)
				.OnEnterEditingMode(this, &SHierarchyViewItem::OnBeginNameTextEdit)
				.OnExitEditingMode(this, &SHierarchyViewItem::OnEndNameTextEdit)
				.OnVerifyTextChanged(this, &SHierarchyViewItem::OnVerifyNameTextChanged)
				.OnTextCommitted(this, &SHierarchyViewItem::OnNameTextCommited)
				.IsSelected(this, &SHierarchyViewItem::IsSelectedExclusively)
			]

			// Locked Icon
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ContentPadding(FMargin(3, 1))
				.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.ForegroundColor(FCoreStyle::Get().GetSlateColor("Foreground"))
				.OnClicked(this, &SHierarchyViewItem::OnToggleLockedInDesigner)
				.Visibility(Model->CanControlLockedInDesigner() ? EVisibility::Visible : EVisibility::Hidden)
				.ToolTipText(LOCTEXT("WidgetLockedButtonToolTip", "Locks or Unlocks this widget and all children.  Locking a widget prevents it from being selected in the designer view by clicking on them.\n\nHolding [Shift] will only affect this widget and no children."))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.MinDesiredWidth(12.0f)
					.HAlign(HAlign_Left)
					[
						SNew(STextBlock)
						.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
						.Text(this, &SHierarchyViewItem::GetLockBrushForWidget)
					]
				]
			]

			// Visibility icon
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ContentPadding(FMargin(3, 1))
				.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.ForegroundColor(FCoreStyle::Get().GetSlateColor("Foreground"))
				.OnClicked(this, &SHierarchyViewItem::OnToggleVisibility)
				.Visibility(Model->CanControlVisibility() ? EVisibility::Visible : EVisibility::Hidden)
				.ToolTipText(LOCTEXT("WidgetVisibilityButtonToolTip", "Toggle Widget's Editor Visibility"))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
					.Text(this, &SHierarchyViewItem::GetVisibilityBrushForWidget)
				]
			]
		],
		InOwnerTableView);
}

SHierarchyViewItem::~SHierarchyViewItem()
{
	Model->RenameEvent.Unbind();
}

void SHierarchyViewItem::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	STableRow< TSharedPtr<FHierarchyModel> >::OnMouseEnter(MyGeometry, MouseEvent);

	Model->OnMouseEnter();
}

void SHierarchyViewItem::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	STableRow< TSharedPtr<FHierarchyModel> >::OnMouseLeave(MouseEvent);

	Model->OnMouseLeave();
}

void SHierarchyViewItem::OnBeginNameTextEdit()
{
	Model->OnBeginEditing();

	InitialText = Model->GetText();
}

void SHierarchyViewItem::OnEndNameTextEdit()
{
	Model->OnEndEditing();
}

bool SHierarchyViewItem::OnVerifyNameTextChanged(const FText& InText, FText& OutErrorMessage)
{
	return Model->OnVerifyNameTextChanged(InText, OutErrorMessage);
}

void SHierarchyViewItem::OnNameTextCommited(const FText& InText, ETextCommit::Type CommitInfo)
{
	// The model can return nice names "Border_53" becomes [Border] in some cases
	// This check makes sure we don't rename the object internally to that nice name.
	// Most common case would be the user enters edit mode by accident then just moves focus away.
	if (InitialText.EqualToCaseIgnored(InText))
	{
		return;
	}

	Model->OnNameTextCommited(InText, CommitInfo);
}

bool SHierarchyViewItem::IsReadOnly() const
{
	return !Model->CanRename();
}

void SHierarchyViewItem::OnRequestBeginRename()
{
	TSharedPtr<SInlineEditableTextBlock> SafeEditBox = EditBox.Pin();
	if ( SafeEditBox.IsValid() )
	{
		SafeEditBox->EnterEditingMode();
	}
}

FSlateFontInfo SHierarchyViewItem::GetItemFont() const
{
	return Model->GetFont();
}

FText SHierarchyViewItem::GetItemText() const
{
	return Model->GetText();
}

bool SHierarchyViewItem::IsHovered() const
{
	return bIsHovered || Model->IsHovered();
}

void SHierarchyViewItem::HandleDragEnter(FDragDropEvent const& DragDropEvent)
{
	Model->HandleDragEnter(DragDropEvent);
}

void SHierarchyViewItem::HandleDragLeave(const FDragDropEvent& DragDropEvent)
{
	Model->HandleDragLeave(DragDropEvent);
}

TOptional<EItemDropZone> SHierarchyViewItem::HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FHierarchyModel> TargetItem)
{
	return Model->HandleCanAcceptDrop(DragDropEvent, DropZone);
}

FReply SHierarchyViewItem::HandleDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return Model->HandleDragDetected(MyGeometry, MouseEvent);
}

FReply SHierarchyViewItem::HandleAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FHierarchyModel> TargetItem)
{
	return Model->HandleAcceptDrop(DragDropEvent, DropZone);
}

FReply SHierarchyViewItem::OnToggleVisibility()
{
	Model->SetIsVisible(!Model->IsVisible());

	return FReply::Handled();
}

FText SHierarchyViewItem::GetVisibilityBrushForWidget() const
{
	return Model->IsVisible() ? FEditorFontGlyphs::Eye : FEditorFontGlyphs::Eye_Slash;
}

FReply SHierarchyViewItem::OnToggleLockedInDesigner()
{
	if ( Model.IsValid() )
	{
		const bool bRecursive = FSlateApplication::Get().GetModifierKeys().IsShiftDown() ? false : true;
		Model->SetIsLockedInDesigner(!Model->IsLockedInDesigner(), bRecursive);
	}

	return FReply::Handled();
}

FText SHierarchyViewItem::GetLockBrushForWidget() const
{
	return Model.IsValid() && Model->IsLockedInDesigner() ? FEditorFontGlyphs::Lock : FEditorFontGlyphs::Unlock;
}

#undef LOCTEXT_NAMESPACE

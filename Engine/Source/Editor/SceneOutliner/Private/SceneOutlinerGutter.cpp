// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SceneOutlinerGutter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "EditorStyleSet.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Widgets/Views/STreeView.h"
#include "ISceneOutliner.h"

#include "SortHelper.h"

#define LOCTEXT_NAMESPACE "SceneOutlinerGutter"

namespace SceneOutliner
{

bool FGetVisibilityVisitor::RecurseChildren(const ITreeItem& Item) const
{
	if (const bool* Info = VisibilityInfo.Find(&Item))
	{
		return *Info;
	}
	else
	{
		bool bIsVisible = false;
		for (const auto& ChildPtr : Item.GetChildren())
		{
			auto Child = ChildPtr.Pin();
			if (Child.IsValid() && Child->Get(*this))
			{
				bIsVisible = true;
				break;
			}
		}
		VisibilityInfo.Add(&Item, bIsVisible);
		
		return bIsVisible;
	}
}

bool FGetVisibilityVisitor::Get(const FActorTreeItem& ActorItem) const
{
	if (const bool* Info = VisibilityInfo.Find(&ActorItem))
	{
		return *Info;
	}
	else
	{
		const AActor* Actor = ActorItem.Actor.Get();

		const bool bIsVisible = Actor && !Actor->IsTemporarilyHiddenInEditor(true);
		VisibilityInfo.Add(&ActorItem, bIsVisible);

		return bIsVisible;
	}
}

bool FGetVisibilityVisitor::Get(const FWorldTreeItem& WorldItem) const
{
	return RecurseChildren(WorldItem);
}

bool FGetVisibilityVisitor::Get(const FFolderTreeItem& FolderItem) const
{
	return RecurseChildren(FolderItem);
}

struct FSetVisibilityVisitor : IMutableTreeItemVisitor
{
	/** Whether this item should be visible or not */
	const bool bSetVisibility;

	FSetVisibilityVisitor(bool bInSetVisibility) : bSetVisibility(bInSetVisibility) {}

	virtual void Visit(FActorTreeItem& ActorItem) const override
	{
		AActor* Actor = ActorItem.Actor.Get();
		if (Actor)
		{
			// Save the actor to the transaction buffer to support undo/redo, but do
			// not call Modify, as we do not want to dirty the actor's package and
			// we're only editing temporary, transient values
			SaveToTransactionBuffer(Actor, false);
			Actor->SetIsTemporarilyHiddenInEditor( !bSetVisibility );
		}
	}

	virtual void Visit(FWorldTreeItem& WorldItem) const override
	{
		for (auto& ChildPtr : WorldItem.GetChildren())
		{
			auto Child = ChildPtr.Pin();
			if (Child.IsValid())
			{
				FSetVisibilityVisitor Visibility(bSetVisibility);
				Child->Visit(Visibility);
			}
		}
	}

	virtual void Visit(FFolderTreeItem& FolderItem) const override
	{
		for (auto& ChildPtr : FolderItem.GetChildren())
		{
			auto Child = ChildPtr.Pin();
			if (Child.IsValid())
			{
				FSetVisibilityVisitor Visibility(bSetVisibility);
				Child->Visit(Visibility);
			}
		}
	}
};

class FVisibilityDragDropOp : public FDragDropOperation, public TSharedFromThis<FVisibilityDragDropOp>
{
public:
	
	DRAG_DROP_OPERATOR_TYPE(FVisibilityDragDropOp, FDragDropOperation)

	/** Flag which defines whether to hide destination actors or not */
	bool bHidden;

	/** Undo transaction stolen from the gutter which is kept alive for the duration of the drag */
	TUniquePtr<FScopedTransaction> UndoTransaction;

	/** The widget decorator to use */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		return SNullWidget::NullWidget;
	}

	/** Create a new drag and drop operation out of the specified flag */
	static TSharedRef<FVisibilityDragDropOp> New(const bool _bHidden, TUniquePtr<FScopedTransaction>& ScopedTransaction)
	{
		TSharedRef<FVisibilityDragDropOp> Operation = MakeShareable(new FVisibilityDragDropOp);

		Operation->bHidden = _bHidden;
		Operation->UndoTransaction = MoveTemp(ScopedTransaction);

		Operation->Construct();
		return Operation;
	}
};

/** Widget responsible for managing the visibility for a single actor */
class SVisibilityWidget : public SImage
{
public:
	SLATE_BEGIN_ARGS(SVisibilityWidget){}
	SLATE_END_ARGS()

	/** Construct this widget */
	void Construct(const FArguments& InArgs, TWeakPtr<FSceneOutlinerGutter> InWeakColumn, TWeakPtr<ISceneOutliner> InWeakOutliner, TWeakPtr<ITreeItem> InWeakTreeItem)
	{
		WeakTreeItem = InWeakTreeItem;
		WeakOutliner = InWeakOutliner;
		WeakColumn = InWeakColumn;

		SImage::Construct(
			SImage::FArguments()
			.Image(this, &SVisibilityWidget::GetBrush)
		);
	}

private:

	/** Start a new drag/drop operation for this widget */
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
		{
			return FReply::Handled().BeginDragDrop(FVisibilityDragDropOp::New(!IsVisible(), UndoTransaction));
		}
		else
		{
			return FReply::Unhandled();
		}
	}

	/** If a visibility drag drop operation has entered this widget, set its actor to the new visibility state */
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
	{
		auto VisibilityOp = DragDropEvent.GetOperationAs<FVisibilityDragDropOp>();
		if (VisibilityOp.IsValid())
		{
			SetIsVisible(!VisibilityOp->bHidden);
		}
	}

	FReply HandleClick()
	{
		auto Outliner = WeakOutliner.Pin();
		auto TreeItem = WeakTreeItem.Pin();
		auto Column = WeakColumn.Pin();
		
		if (!Outliner.IsValid() || !TreeItem.IsValid() || !Column.IsValid())
		{
			return FReply::Unhandled();
		}

		// Open an undo transaction
		UndoTransaction.Reset(new FScopedTransaction(LOCTEXT("SetActorVisibility", "Set Actor Visibility")));

		const auto& Tree = Outliner->GetTree();

		const bool bVisible = !IsVisible();

		// We operate on all the selected items if the specified item is selected
		if (Tree.IsItemSelected(TreeItem.ToSharedRef()))
		{
			const FSetVisibilityVisitor Visitor(bVisible);

			for (auto& SelectedItem : Tree.GetSelectedItems())
			{
				if (IsVisible(SelectedItem, Column) != bVisible)
				{
					SelectedItem->Visit(Visitor);
				}		
			}

			GEditor->RedrawAllViewports();
		}
		else
		{
			SetIsVisible(bVisible);
		}

		return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	}

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override
	{
		return HandleClick();
	}

	/** Called when the mouse button is pressed down on this widget */
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return FReply::Unhandled();
		}

		return HandleClick();
	}

	/** Process a mouse up message */
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			UndoTransaction.Reset();
			return FReply::Handled();
		}

		return FReply::Unhandled();
	}

	/** Called when this widget had captured the mouse, but that capture has been revoked for some reason. */
	virtual void OnMouseCaptureLost() override
	{
		UndoTransaction.Reset();
	}

	/** Get the brush for this widget */
	const FSlateBrush* GetBrush() const
	{
		if (IsVisible())
		{
			return IsHovered() ? FEditorStyle::GetBrush( "Level.VisibleHighlightIcon16x" ) :
				FEditorStyle::GetBrush( "Level.VisibleIcon16x" );
		}
		else
		{
			return IsHovered() ? FEditorStyle::GetBrush( "Level.NotVisibleHighlightIcon16x" ) :
				FEditorStyle::GetBrush( "Level.NotVisibleIcon16x" );
		}
	}

	/** Check if the specified item is visible */
	static bool IsVisible(const FTreeItemPtr& Item, const TSharedPtr<FSceneOutlinerGutter>& Column)
	{
		return Column.IsValid() && Item.IsValid() ? Column->IsItemVisible(*Item) : false;
	}

	/** Check if our wrapped tree item is visible */
	bool IsVisible() const
	{
		return IsVisible(WeakTreeItem.Pin(), WeakColumn.Pin());
	}

	/** Set the actor this widget is responsible for to be hidden or shown */
	void SetIsVisible(const bool bVisible)
	{
		auto TreeItem = WeakTreeItem.Pin();
		if (TreeItem.IsValid() && IsVisible() != bVisible)
		{
			FSetVisibilityVisitor Visitor(bVisible);
			TreeItem->Visit(Visitor);

			GEditor->RedrawAllViewports();
		}
	}

	/** The tree item we relate to */
	TWeakPtr<ITreeItem> WeakTreeItem;
	
	/** Reference back to the outliner so we can set visibility of a whole selection */
	TWeakPtr<ISceneOutliner> WeakOutliner;

	/** Weak pointer back to the column */
	TWeakPtr<FSceneOutlinerGutter> WeakColumn;

	/** Scoped undo transaction */
	TUniquePtr<FScopedTransaction> UndoTransaction;
};

FSceneOutlinerGutter::FSceneOutlinerGutter(ISceneOutliner& Outliner)
{
	WeakOutliner = StaticCastSharedRef<ISceneOutliner>(Outliner.AsShared());
}

void FSceneOutlinerGutter::Tick(double InCurrentTime, float InDeltaTime)
{
	VisibilityCache.VisibilityInfo.Empty();
}

FName FSceneOutlinerGutter::GetColumnID()
{
	return GetID();
}

SHeaderRow::FColumn::FArguments FSceneOutlinerGutter::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnID())
		.FixedWidth(16.f)
		[
			SNew(SSpacer)
		];
}

const TSharedRef<SWidget> FSceneOutlinerGutter::ConstructRowWidget(FTreeItemRef TreeItem, const STableRow<FTreeItemPtr>& Row)
{
	return SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SVisibilityWidget, SharedThis(this), WeakOutliner, TreeItem)
		];
}

void FSceneOutlinerGutter::SortItems(TArray<FTreeItemPtr>& RootItems, const EColumnSortMode::Type SortMode) const
{
	FSortHelper<int32, bool>()
		/** Sort by type first */
		.Primary([](const ITreeItem& Item){ return Item.GetTypeSortPriority(); }, SortMode)
		/** Then by visibility */
		.Secondary(FGetVisibilityVisitor(), SortMode)
		.Sort(RootItems);
}

}		// namespace SceneOutliner

#undef LOCTEXT_NAMESPACE


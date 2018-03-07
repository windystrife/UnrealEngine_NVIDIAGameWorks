// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/DragAndDrop.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "WidgetReference.h"
#include "WidgetBlueprintEditor.h"

class FWidgetBlueprintEditor;

class FHierarchyModel : public TSharedFromThis < FHierarchyModel >
{
public:
	FHierarchyModel(TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor);
	virtual ~FHierarchyModel() { }

	/** Gets the unique name of the item used to restore item expansion. */
	virtual FName GetUniqueName() const = 0;

	/* @returns the widget name to use for the tree item */
	virtual FText GetText() const = 0;

	/** @return The tooltip for the tree item image */
	virtual FText GetImageToolTipText() const { return FText::GetEmpty(); }

	/** @return The tooltip for the tree item label */
	virtual FText GetLabelToolTipText() const { return FText::GetEmpty(); }

	virtual const FSlateBrush* GetImage() const = 0;

	virtual FSlateFontInfo GetFont() const = 0;

	FReply HandleDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual void HandleDragEnter(const FDragDropEvent& DragDropEvent);
	virtual void HandleDragLeave(const FDragDropEvent& DragDropEvent);

	virtual TOptional<EItemDropZone> HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone);
	virtual FReply HandleAcceptDrop(FDragDropEvent const& DragDropEvent, EItemDropZone DropZone);

	virtual bool OnVerifyNameTextChanged(const FText& InText, FText& OutErrorMessage);

	virtual void OnNameTextCommited(const FText& InText, ETextCommit::Type CommitInfo);

	virtual void GatherChildren(TArray< TSharedPtr<FHierarchyModel> >& Children);

	virtual void OnSelection() = 0;

	virtual void OnMouseEnter() {}
	virtual void OnMouseLeave() {}

	void RefreshSelection();
	bool ContainsSelection();
	bool IsSelected() const;

	virtual bool IsHovered() const { return false; }
	virtual bool IsVisible() const { return true; }
	virtual bool CanControlVisibility() const { return false; }
	virtual void SetIsVisible(bool IsVisible) { }

	virtual bool CanControlLockedInDesigner() const { return false; }
	virtual bool IsLockedInDesigner() { return false; }
	virtual void SetIsLockedInDesigner(bool NewIsLocked, bool bRecursive)
	{
		if (bRecursive)
		{
			TArray< TSharedPtr<FHierarchyModel> > Children;
			GetChildren(Children);
			for (TSharedPtr<FHierarchyModel>& child : Children)
			{
				if (child.IsValid())
				{
					child->SetIsLockedInDesigner(NewIsLocked, bRecursive);
				}
			}
		}
	}

	virtual bool IsExpanded() const { return true; }
	virtual void SetExpanded(bool bIsExpanded) { }

	virtual bool CanRename() const { return false; }
	virtual void RequestBeginRename() { }

	virtual void OnBeginEditing() { }
	virtual void OnEndEditing() { }

	virtual bool IsRoot() const { return false; }

public:
	FSimpleDelegate RenameEvent;

protected:
	virtual void GetChildren(TArray< TSharedPtr<FHierarchyModel> >& Children) = 0;
	virtual void UpdateSelection() = 0;
	virtual FWidgetReference AsDraggedWidgetReference() const { return FWidgetReference(); }

private:
	void InitializeChildren();

protected:

	bool bInitialized;
	bool bIsSelected;
	TWeakPtr<FWidgetBlueprintEditor> BlueprintEditor;
	TArray< TSharedPtr<FHierarchyModel> > Models;
};

class FHierarchyRoot : public FHierarchyModel
{
public:
	FHierarchyRoot(TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor);
	
	virtual ~FHierarchyRoot() {}

	virtual FName GetUniqueName() const override;

	/* @returns the widget name to use for the tree item */
	virtual FText GetText() const override;

	virtual const FSlateBrush* GetImage() const override;

	virtual FSlateFontInfo GetFont() const override;

	virtual void OnSelection() override;

	virtual TOptional<EItemDropZone> HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone) override;
	virtual FReply HandleAcceptDrop(FDragDropEvent const& DragDropEvent, EItemDropZone DropZone) override;

	virtual bool IsRoot() const override { return true; }

protected:
	virtual void GetChildren(TArray< TSharedPtr<FHierarchyModel> >& Children) override;
	virtual void UpdateSelection() override;

private:
	FText RootText;
};

class FNamedSlotModel : public FHierarchyModel
{
public:
	FNamedSlotModel(FWidgetReference InItem, FName InSlotName, TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor);

	virtual ~FNamedSlotModel() {}

	virtual FName GetUniqueName() const override;

	/* @returns the widget name to use for the tree item */
	virtual FText GetText() const override;

	virtual const FSlateBrush* GetImage() const override;

	virtual FSlateFontInfo GetFont() const override;

	virtual void OnSelection() override;
	
	virtual TOptional<EItemDropZone> HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone) override;
	virtual FReply HandleAcceptDrop(FDragDropEvent const& DragDropEvent, EItemDropZone DropZone) override;

protected:
	virtual void GetChildren(TArray< TSharedPtr<FHierarchyModel> >& Children) override;
	virtual void UpdateSelection() override;
	virtual FWidgetReference AsDraggedWidgetReference() const;

	void DoDrop(UWidget* NamedSlotHostWidget, UWidget* DroppingWidget);

private:

	FWidgetReference Item;
	FName SlotName;
};

class FHierarchyWidget : public FHierarchyModel
{
public:
	FHierarchyWidget(FWidgetReference InItem, TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor);

	virtual ~FHierarchyWidget() {}

	virtual FName GetUniqueName() const override;
	
	/* @returns the widget name to use for the tree item */
	virtual FText GetText() const override;
	virtual FText GetImageToolTipText() const override;
	virtual FText GetLabelToolTipText() const override;

	virtual const FSlateBrush* GetImage() const override;

	virtual FSlateFontInfo GetFont() const override;

	virtual TOptional<EItemDropZone> HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone) override;

	virtual void HandleDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply HandleAcceptDrop(FDragDropEvent const& DragDropEvent, EItemDropZone DropZone) override;

	virtual bool OnVerifyNameTextChanged(const FText& InText, FText& OutErrorMessage) override;

	virtual void OnNameTextCommited(const FText& InText, ETextCommit::Type CommitInfo) override;

	virtual void OnSelection() override;

	virtual void OnMouseEnter() override;
	virtual void OnMouseLeave() override;

	virtual bool IsHovered() const override;

	virtual bool IsVisible() const override
	{
		if ( UWidget* TemplateWidget = Item.GetTemplate() )
		{
			return !TemplateWidget->bHiddenInDesigner;
		}

		return true;
	}

	virtual bool CanControlVisibility() const override
	{
		return true;
	}

	virtual void SetIsVisible(bool IsVisible) override
	{
		Item.GetTemplate()->bHiddenInDesigner = !IsVisible;

		if ( UWidget* PreviewWidget = Item.GetPreview() )
		{
			PreviewWidget->bHiddenInDesigner = !IsVisible;
		}
	}

	virtual bool CanControlLockedInDesigner() const override
	{
		return true;
	}

	virtual bool IsLockedInDesigner() override
	{
		UWidget* TemplateWidget = Item.GetTemplate();
		if (TemplateWidget)
		{
			return TemplateWidget->IsLockedInDesigner();
		}

		return false;
	}

	virtual void SetIsLockedInDesigner(bool NewIsLocked, bool bRecursive) override
	{
		FHierarchyModel::SetIsLockedInDesigner(NewIsLocked, bRecursive);

		if(Item.GetTemplate() && Item.GetPreview())
		{
			Item.GetTemplate()->SetLockedInDesigner(NewIsLocked);
			Item.GetPreview()->SetLockedInDesigner(NewIsLocked);
		}
	}

	virtual bool IsExpanded() const override
	{
		return Item.GetTemplate()->bExpandedInDesigner;
	}

	virtual void SetExpanded(bool bIsExpanded) override
	{
		Item.GetTemplate()->bExpandedInDesigner = bIsExpanded;
	}

	virtual bool CanRename() const override;
	virtual void RequestBeginRename() override;

	virtual void OnBeginEditing() override;
	virtual void OnEndEditing() override;

protected:
	virtual void GetChildren(TArray< TSharedPtr<FHierarchyModel> >& Children) override;
	virtual void UpdateSelection() override;
	virtual FWidgetReference AsDraggedWidgetReference() const { return Item; }

private:
	FWidgetReference Item;
	bool bEditing;
};

/**
 * An widget item in the hierarchy tree view.
 */
class SHierarchyViewItem : public STableRow< TSharedPtr<FHierarchyModel> >
{
public:
	SLATE_BEGIN_ARGS( SHierarchyViewItem ){}
		
		/** The current text to highlight */
		SLATE_ATTRIBUTE( FText, HighlightText )

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef< STableViewBase >& InOwnerTableView, TSharedPtr<FHierarchyModel> InModel);
	virtual ~SHierarchyViewItem();

	// Begin SWidget
	virtual bool IsHovered() const override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	// End SWidget

public:
	FReply HandleDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	void HandleDragEnter(const FDragDropEvent& DragDropEvent);
	void HandleDragLeave(const FDragDropEvent& DragDropEvent);

	TOptional<EItemDropZone> HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FHierarchyModel> TargetItem);
	FReply HandleAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FHierarchyModel> TargetItem);

private:

	void OnBeginNameTextEdit();
	void OnEndNameTextEdit();

	/** Called when text is being committed to check for validity */
	bool OnVerifyNameTextChanged(const FText& InText, FText& OutErrorMessage);

	/** Called when text is committed on the node */
	void OnNameTextCommited(const FText& InText, ETextCommit::Type CommitInfo);

	bool IsReadOnly() const;
	void OnRequestBeginRename();

	/** Gets the font to use for the text item, bold for customized named items */
	FSlateFontInfo GetItemFont() const;

	/** @returns the widget name to use for the tree item */
	FText GetItemText() const;

	/** Handles clicking the visibility toggle */
	FReply OnToggleVisibility();

	/** Handles clicking the locked toggle */
	FReply OnToggleLockedInDesigner();

	/** Returns a brush representing the lock item of the widget's lock button */
	FText GetLockBrushForWidget() const;

	/** Returns a brush representing the visibility item of the widget's visibility button */
	FText GetVisibilityBrushForWidget() const;

private:

	/** Edit box for the name. */
	TWeakPtr<SInlineEditableTextBlock> EditBox;

	/* The mode that this tree item represents */
	TSharedPtr<FHierarchyModel> Model;

	/** Text when we start editing. */
	FText InitialText;
};

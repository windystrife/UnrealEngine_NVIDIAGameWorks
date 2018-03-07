// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SModifierListview.h"
#include "SModifierItemRow.h"
#include "MultiBoxBuilder.h"
#include "GenericCommands.h"
#include "MultiBoxBuilder.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "SModifierListview"

void SModifierListView::Construct(const FArguments& InArgs)
{
	SAssignNew(Listview, SListView<ModifierListviewItem>)
		.ListItemsSource(InArgs._Items)
		.ItemHeight(36)
		.OnGenerateRow(this, &SModifierListView::OnGenerateWidgetForList)
		.OnSelectionChanged(this, &SModifierListView::OnSelectionChanged)
		.OnContextMenuOpening(this, &SModifierListView::OnContextMenuOpening);

	ListviewItems = InArgs._Items;

	InstanceDetailsView = InArgs._InstanceDetailsView;

	OnApplyModifierDelegate = InArgs._OnApplyModifier;
	OnRevertModifierDelegate = InArgs._OnRevertModifier;
	OnOpenModifierDelegate = InArgs._OnOpenModifier;
	OnRemoveModifierDelegate = InArgs._OnRemoveModifier;
	OnMoveUpModifierDelegate = InArgs._OnMoveUpModifier;
	OnMoveDownModifierDelegate = InArgs._OnMoveDownModifier;
	
	this->ChildSlot
	[
		Listview->AsShared()
	];
}

TSharedRef<ITableRow> SModifierListView::OnGenerateWidgetForList(ModifierListviewItem Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SModifierItemRow, OwnerTable, Item).OnOpenModifier(OnOpenModifierDelegate);
}

void SModifierListView::OnSelectionChanged(ModifierListviewItem SelectedItem, ESelectInfo::Type SelectInfo)
{
	UObject* SelectedObject = nullptr;

	// Find object to select if an item was selected (otherwise we are de-selecting to nullptr)
	if (SelectedItem.IsValid())
	{
		SelectedObject = SelectedItem->Instance.Get();
	}

	// Set the details view to the currently selected modifier blueprint instance
	InstanceDetailsView->SetObject(SelectedObject);
}

void SModifierListView::OnApplyModifier()
{
	if (OnApplyModifierDelegate.IsBound() && Listview->GetNumItemsSelected())
	{
		TArray<TWeakObjectPtr<UAnimationModifier>> Instances = GetSelectedModifierInstances();
		OnApplyModifierDelegate.Execute(Instances);
	}
}

void SModifierListView::OnRemoveModifier()
{
	if (OnRemoveModifierDelegate.IsBound() && Listview->GetNumItemsSelected())
	{
		TArray<TWeakObjectPtr<UAnimationModifier>> Instances = GetSelectedModifierInstances();
		OnRemoveModifierDelegate.Execute(Instances);
	}
}

void SModifierListView::OnOpenModifier()
{
	if (OnOpenModifierDelegate.IsBound() && ( Listview->GetNumItemsSelected() == 1))
	{
		TArray<TWeakObjectPtr<UAnimationModifier>> Instances = GetSelectedModifierInstances();
		OnOpenModifierDelegate.Execute(Instances[0]);
	}
}

void SModifierListView::OnRevertModifier()
{
	if (OnRevertModifierDelegate.IsBound() && Listview->GetNumItemsSelected())
	{
		TArray<TWeakObjectPtr<UAnimationModifier>> Instances = GetSelectedModifierInstances();
		OnRevertModifierDelegate.Execute(Instances);
	}
}

void SModifierListView::OnMoveUpModifier()
{
	if (OnOpenModifierDelegate.IsBound() && (Listview->GetNumItemsSelected() == 1))
	{
		TArray<TWeakObjectPtr<UAnimationModifier>> Instances = GetSelectedModifierInstances();
		OnMoveUpModifierDelegate.Execute(Instances[0]);
	}
}

void SModifierListView::OnMoveDownModifier()
{
	if (OnOpenModifierDelegate.IsBound() && (Listview->GetNumItemsSelected() == 1))
	{
		TArray<TWeakObjectPtr<UAnimationModifier>> Instances = GetSelectedModifierInstances();
		OnMoveDownModifierDelegate.Execute(Instances[0]);
	}
}

const TArray<TWeakObjectPtr<UAnimationModifier>> SModifierListView::GetSelectedModifierInstances()
{
	TArray<ModifierListviewItem> Items;
	Listview->GetSelectedItems(Items);

	TArray<TWeakObjectPtr<UAnimationModifier>> Instances;
	for (ModifierListviewItem& SelectedItem : Items)
	{		
		Instances.Add(SelectedItem->Instance);
	}

	return Instances;
}

void SModifierListView::Refresh()
{	
	Listview->RebuildList();
}

TSharedPtr<SWidget> SModifierListView::OnContextMenuOpening()
{
	const int32 NumItems = Listview->GetNumItemsSelected();
	if (NumItems >= 1)
	{
		const bool bCloseAfterSelection = true;

		FMenuBuilder MenuBuilder(bCloseAfterSelection, TSharedPtr<FUICommandList>());		
		
		MenuBuilder.BeginSection(NAME_None);
		{		
			if (Listview->GetNumItemsSelected() == 1)
			{
				MenuBuilder.AddMenuEntry(LOCTEXT("OpenModifierLabel", "Open Blueprint"), LOCTEXT("OpenModifierToolTip", "Open selected Modifier Blueprint"), FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.Blueprint"), FUIAction(FExecuteAction::CreateSP(this, &SModifierListView::OnOpenModifier)));
			}

			const FText ApplyLabel = FText::FormatOrdered(LOCTEXT("ApplyModifierLabel", "Apply {0}|plural(one=Modifier,other=Modifiers)"), NumItems);
			const FText ApplyTooltip = FText::FormatOrdered(LOCTEXT("ApplyModifierToolTip", "Apply selected {0}|plural(one=Modifier,other=Modifiers)"), NumItems);
			MenuBuilder.AddMenuEntry(ApplyLabel, ApplyTooltip, FSlateIcon(FEditorStyle::GetStyleSetName(), "GenericCommands.Redo"), FUIAction(FExecuteAction::CreateSP(this, &SModifierListView::OnApplyModifier)));	   

			const FText RevertLabel = FText::FormatOrdered(LOCTEXT("ApplyRevertLabel", "Revert {0}|plural(one=Modifier,other=Modifiers)"), NumItems);
			const FText RevertTooltip = FText::FormatOrdered(LOCTEXT("ApplyRevertToolTip", "Revert selected {0}|plural(one=Modifier,other=Modifiers)"), NumItems);
			MenuBuilder.AddMenuEntry(RevertLabel, RevertTooltip, FSlateIcon(FEditorStyle::GetStyleSetName(), "GenericCommands.Undo"), FUIAction(FExecuteAction::CreateSP(this, &SModifierListView::OnRevertModifier)));

			const FText RemoveLabel = FText::FormatOrdered(LOCTEXT("RemoveModifierLabel", "Remove {0}|plural(one=Modifier,other=Modifiers)"), NumItems);
			const FText RemoveTooltip = FText::FormatOrdered(LOCTEXT("RemoveModifierToolTip", "Remove selected {0}|plural(one=Modifier,other=Modifiers)"), NumItems);
			MenuBuilder.AddMenuEntry(RemoveLabel, RemoveTooltip, FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.Delete"), FUIAction(FExecuteAction::CreateSP(this, &SModifierListView::OnRemoveModifier)));
		}
		MenuBuilder.EndSection();

		if (Listview->GetNumItemsSelected() == 1)
		{ 
			MenuBuilder.BeginSection(NAME_None);
			{
				MenuBuilder.AddMenuEntry(LOCTEXT("MoveUpModifierLabel", "Move Up"), LOCTEXT("MoveUpModifierToolTip", "Move selected Modifier Up in list"), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &SModifierListView::OnMoveUpModifier), FCanExecuteAction::CreateSP( this, &SModifierListView::CanMoveSelectedItemUp)));
				
				MenuBuilder.AddMenuEntry(LOCTEXT("MoveDownModifierLabel", "Move Down"), LOCTEXT("MoveDownModifierToolTip", "Move selected Modifier Down in list"), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &SModifierListView::OnMoveDownModifier), FCanExecuteAction::CreateSP(this, &SModifierListView::CanMoveSelectedItemDown)));
			}
			MenuBuilder.EndSection();
		}

		return MenuBuilder.MakeWidget();
	}
	
	return TSharedPtr<SWidget>();
}

bool SModifierListView::CanMoveSelectedItemUp()
{
	bool bCanMove = false;
	if (Listview->GetNumItemsSelected() == 1)
	{
		TArray<ModifierListviewItem> Items;
		Listview->GetSelectedItems(Items);
		bCanMove = (Items[0]->Index > 0);
	}

	return bCanMove;
}

bool SModifierListView::CanMoveSelectedItemDown()
{
	bool bCanMove = false;
	if (Listview->GetNumItemsSelected() == 1)
	{
		TArray<ModifierListviewItem> Items;
		Listview->GetSelectedItems(Items);
		bCanMove = (Items[0]->Index < (ListviewItems->Num() - 1));
	}

	return bCanMove;
}

#undef LOCTEXT_NAMESPACE // "SModifierListview"


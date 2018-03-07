// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WorldTreeItem.h"
#include "Modules/ModuleManager.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "EditorStyleSet.h"
#include "ScopedTransaction.h"
#include "SceneOutlinerDragDrop.h"
#include "SSceneOutliner.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditor.h"
#include "EditorActorFolders.h"

#define LOCTEXT_NAMESPACE "SceneOutliner_WorldTreeItem"

namespace SceneOutliner
{

FWorldTreeItem::FWorldTreeItem(UWorld* InWorld)
	: World(InWorld)
	, ID(InWorld)
{

}

FTreeItemPtr FWorldTreeItem::FindParent(const FTreeItemMap& ExistingItems) const
{
	return nullptr;
}

FTreeItemPtr FWorldTreeItem::CreateParent() const
{
	return nullptr;
}

void FWorldTreeItem::Visit(const ITreeItemVisitor& Visitor) const
{
	Visitor.Visit(*this);
}

void FWorldTreeItem::Visit(const IMutableTreeItemVisitor& Visitor)
{
	Visitor.Visit(*this);
}

FTreeItemID FWorldTreeItem::GetID() const
{
	return ID;
}

FString FWorldTreeItem::GetDisplayString() const
{
	if (UWorld* WorldPtr = World.Get())
	{
		return SceneOutliner::GetWorldDescription(WorldPtr).ToString();
	}
	return FString();
}

FString FWorldTreeItem::GetWorldName() const
{
	if (UWorld* WorldPtr = World.Get())
	{
		return World->GetFName().GetPlainNameString();
	}
	return FString();
}

int32 FWorldTreeItem::GetTypeSortPriority() const
{
	return ETreeItemSortOrder::World;
}

bool FWorldTreeItem::CanInteract() const
{
	if (UWorld* WorldPtr = World.Get())
	{
		return Flags.bInteractive && WorldPtr->WorldType == EWorldType::Editor;
	}
	return Flags.bInteractive;
}

void FWorldTreeItem::GenerateContextMenu(FMenuBuilder& MenuBuilder, SSceneOutliner& Outliner)
{
	auto SharedOutliner = StaticCastSharedRef<SSceneOutliner>(Outliner.AsShared());
	
	const FSlateIcon WorldSettingsIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.WorldProperties.Tab");
	const FSlateIcon NewFolderIcon(FEditorStyle::GetStyleSetName(), "SceneOutliner.NewFolderIcon");

	MenuBuilder.AddMenuEntry(LOCTEXT("CreateFolder", "Create Folder"), FText(), NewFolderIcon, FUIAction(FExecuteAction::CreateSP(this, &FWorldTreeItem::CreateFolder, TWeakPtr<SSceneOutliner>(SharedOutliner))));
	MenuBuilder.AddMenuEntry(LOCTEXT("OpenWorldSettings", "World Settings"), FText(), WorldSettingsIcon, FExecuteAction::CreateSP(this, &FWorldTreeItem::OpenWorldSettings));
}

void FWorldTreeItem::CreateFolder(TWeakPtr<SSceneOutliner> WeakOutliner)
{
	auto Outliner = WeakOutliner.Pin();

	if (Outliner.IsValid() && SharedData->RepresentingWorld)
	{
		const FScopedTransaction Transaction(LOCTEXT("UndoAction_CreateFolder", "Create Folder"));

		const FName NewFolderName = FActorFolders::Get().GetDefaultFolderName(*SharedData->RepresentingWorld, "");
		FActorFolders::Get().CreateFolder(*SharedData->RepresentingWorld, NewFolderName);

		// At this point the new folder will be in our newly added list, so select it and open a rename when it gets refreshed
		Outliner->OnItemAdded(NewFolderName, ENewItemAction::Select | ENewItemAction::Rename);
	}
}

FDragValidationInfo FWorldTreeItem::ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& InWorld) const
{
	// Dropping on the world means 'moving to the root' in folder terms
	FFolderDropTarget Target(NAME_None);
	return Target.ValidateDrop(DraggedObjects, InWorld);
}

void FWorldTreeItem::OnDrop(FDragDropPayload& DraggedObjects, UWorld& InWorld, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget)
{
	FFolderDropTarget Target(NAME_None);
	return Target.OnDrop(DraggedObjects, InWorld, ValidationInfo, DroppedOnWidget);
}

void FWorldTreeItem::OpenWorldSettings() const
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );
	LevelEditorModule.GetLevelEditorTabManager()->InvokeTab(FName("WorldSettingsTab"));	
}

}		// namespace SceneOutliner

#undef LOCTEXT_NAMESPACE

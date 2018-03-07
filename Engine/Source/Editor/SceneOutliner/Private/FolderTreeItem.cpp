// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FolderTreeItem.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "ScopedTransaction.h"
#include "DragAndDrop/ActorDragDropGraphEdOp.h"
#include "SceneOutlinerDragDrop.h"
#include "SSceneOutliner.h"

#include "ActorEditorUtils.h"

#include "EditorActorFolders.h"

#define LOCTEXT_NAMESPACE "SceneOutliner_FolderTreeItem"

namespace SceneOutliner
{

FDragValidationInfo FFolderDropTarget::ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const
{
	if (DraggedObjects.Folders)
	{
		// Iterate over all the folders that have been dragged
		for (FName DraggedFolder : DraggedObjects.Folders.GetValue())
		{
			FName Leaf = GetFolderLeafName(DraggedFolder);
			FName Parent = GetParentPath(DraggedFolder);

			if (Parent == DestinationPath)
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("SourceName"), FText::FromName(Leaf));

				FText Text;
				if (DestinationPath.IsNone())
				{
					Text = FText::Format(LOCTEXT("FolderAlreadyAssignedRoot", "{SourceName} is already assigned to root"), Args);
				}
				else
				{
					Args.Add(TEXT("DestPath"), FText::FromName(DestinationPath));
					Text = FText::Format(LOCTEXT("FolderAlreadyAssigned", "{SourceName} is already assigned to {DestPath}"), Args);
				}
				
				return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric, Text);
			}

			const FString DragFolderPath = DraggedFolder.ToString();
			const FString LeafName = Leaf.ToString();
			const FString DstFolderPath = DestinationPath.IsNone() ? FString() : DestinationPath.ToString();
			const FString NewPath = DstFolderPath / LeafName;

			if (FActorFolders::Get().GetFolderProperties(World, FName(*NewPath)))
			{
				// The folder already exists
				FFormatNamedArguments Args;
				Args.Add(TEXT("DragName"), FText::FromString(LeafName));
				return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric,
					FText::Format(LOCTEXT("FolderAlreadyExistsRoot", "A folder called \"{DragName}\" already exists at this level"), Args));
			}
			else if (DragFolderPath == DstFolderPath || DstFolderPath.StartsWith(DragFolderPath + "/"))
			{
				// Cannot drag as a child of itself
				FFormatNamedArguments Args;
				Args.Add(TEXT("FolderPath"), FText::FromName(DraggedFolder));
				return FDragValidationInfo(
					FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric,
					FText::Format(LOCTEXT("ChildOfItself", "Cannot move \"{FolderPath}\" to be a child of itself"), Args));
			}
		}
	}

	if (DraggedObjects.Actors)
	{
		// Iterate over all the folders that have been dragged
		for (auto WeakActor : DraggedObjects.Actors.GetValue())
		{
			AActor* Actor = WeakActor.Get();
			if (Actor->IsChildActor())
			{
				return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric, FText::Format(LOCTEXT("Error_AttachChildActor", "Cannot move {0} as it is a child actor."), FText::FromString(Actor->GetActorLabel())));
			}
			else if (Actor->GetFolderPath() == DestinationPath && !Actor->GetAttachParentActor())
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("SourceName"), FText::FromString(Actor->GetActorLabel()));

				FText Text;
				if (DestinationPath.IsNone())
				{
					Text = FText::Format(LOCTEXT("FolderAlreadyAssignedRoot", "{SourceName} is already assigned to root"), Args);
				}
				else
				{
					Args.Add(TEXT("DestPath"), FText::FromName(DestinationPath));
					Text = FText::Format(LOCTEXT("FolderAlreadyAssigned", "{SourceName} is already assigned to {DestPath}"), Args);
				}
				
				return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric, Text);
			}
		}
	}

	// Everything else is a valid operation
	if (DestinationPath.IsNone())
	{
		return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_CompatibleGeneric, LOCTEXT("MoveToRoot", "Move to root"));
	}
	else
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("DestPath"), FText::FromName(DestinationPath));
		return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_CompatibleGeneric, FText::Format(LOCTEXT("MoveInto", "Move into \"{DestPath}\""), Args));
	}
}

void FFolderDropTarget::OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget)
{
	const FScopedTransaction Transaction( LOCTEXT("MoveOutlinerItems", "Move World Outliner Items") );

	if (DraggedObjects.Folders)
	{
		for (FName Folder : DraggedObjects.Folders.GetValue())
		{
			MoveFolderTo(Folder, DestinationPath, World);
		}
	}

	// Set the folder path on all the dragged actors, and detach any that need to be moved
	if (DraggedObjects.Actors)
	{
		TSet<AActor*> ParentActors;
		TSet<AActor*> ChildActors;

		for (auto WeakActor : DraggedObjects.Actors.GetValue())
		{
			AActor* Actor = WeakActor.Get();
			if (Actor)
			{
				// First mark this object as a parent, then set its children's path
				ParentActors.Add(Actor);
				Actor->SetFolderPath(DestinationPath);

				FActorEditorUtils::TraverseActorTree_ParentFirst(Actor, [&](AActor* InActor){
					ChildActors.Add(InActor);
					InActor->SetFolderPath(DestinationPath);
					return true;
				}, false);
			}
		}

		// Detach parent actors
		for (AActor* Parent : ParentActors)
		{
			auto* RootComp = Parent->GetRootComponent();

			// We don't detach if it's a child of another that's been dragged
			if (RootComp && RootComp->GetAttachParent() && !ChildActors.Contains(Parent))
			{
				if (AActor* OldParentActor = RootComp->GetAttachParent()->GetOwner())
				{
					OldParentActor->Modify();
				}
				RootComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			}
		}
	}
}

FFolderTreeItem::FFolderTreeItem(FName InPath)
	: Path(InPath)
	, LeafName(GetFolderLeafName(InPath))
{
}

FTreeItemPtr FFolderTreeItem::FindParent(const FTreeItemMap& ExistingItems) const
{
	const FName ParentPath = GetParentPath(Path);
	if (!ParentPath.IsNone())
	{
		return ExistingItems.FindRef(ParentPath);
	}

	if (SharedData->RepresentingWorld)
	{
		return ExistingItems.FindRef(SharedData->RepresentingWorld);
	}

	return nullptr;
}

FTreeItemPtr FFolderTreeItem::CreateParent() const
{
	const FName ParentPath = GetParentPath(Path);
	if (!ParentPath.IsNone())
	{
		return MakeShareable(new FFolderTreeItem(ParentPath));
	}

	if (SharedData->RepresentingWorld)
	{
		return MakeShareable(new FWorldTreeItem(SharedData->RepresentingWorld));
	}

	return nullptr;
}

void FFolderTreeItem::Visit(const ITreeItemVisitor& Visitor) const
{
	Visitor.Visit(*this);
}

void FFolderTreeItem::Visit(const IMutableTreeItemVisitor& Visitor)
{
	Visitor.Visit(*this);
}

FTreeItemID FFolderTreeItem::GetID() const
{
	return FTreeItemID(Path);
}

FString FFolderTreeItem::GetDisplayString() const
{
	return LeafName.ToString();
}

int32 FFolderTreeItem::GetTypeSortPriority() const
{
	return ETreeItemSortOrder::Folder;
}


bool FFolderTreeItem::CanInteract() const
{
	return Flags.bInteractive;
}

void FFolderTreeItem::Delete()
{
	if (!SharedData->RepresentingWorld)
	{
		return;
	}

	struct FResetActorFolders : IMutableTreeItemVisitor
	{
		explicit FResetActorFolders(FName InParentPath) : ParentPath(InParentPath) {}

		virtual void Visit(FActorTreeItem& ActorItem) const override
		{
			if (AActor* Actor = ActorItem.Actor.Get())
			{
				Actor->SetFolderPath_Recursively(ParentPath);
			}
		}
		virtual void Visit(FFolderTreeItem& FolderItem) const override
		{
			UWorld* World = FolderItem.SharedData->RepresentingWorld;
			check(World != nullptr);

			MoveFolderTo(FolderItem.Path, ParentPath, *World);
		}

		FName ParentPath;
	};

	const FScopedTransaction Transaction( LOCTEXT("DeleteFolderTransaction", "Delete Folder") );

	FResetActorFolders ResetFolders(GetParentPath(Path));
	for (auto& Child : GetChildren())
	{
		Child.Pin()->Visit(ResetFolders);
	}

	FActorFolders::Get().DeleteFolder(*SharedData->RepresentingWorld, Path);
}

void FFolderTreeItem::CreateSubFolder(TWeakPtr<SSceneOutliner> WeakOutliner)
{
	auto Outliner = WeakOutliner.Pin();

	if (Outliner.IsValid() && SharedData->RepresentingWorld)
	{
		const FScopedTransaction Transaction(LOCTEXT("UndoAction_CreateFolder", "Create Folder"));

		const FName NewFolderName = FActorFolders::Get().GetDefaultFolderName(*SharedData->RepresentingWorld, Path);
		FActorFolders::Get().CreateFolder(*SharedData->RepresentingWorld, NewFolderName);

		// At this point the new folder will be in our newly added list, so select it and open a rename when it gets refreshed
		Outliner->OnItemAdded(NewFolderName, ENewItemAction::Select | ENewItemAction::Rename);
	}
}

void FFolderTreeItem::OnExpansionChanged()
{
	if (!SharedData->RepresentingWorld)
	{
		return;
	}

	// Update the central store of folder properties with this folder's new expansion state
	if (FActorFolderProps* Props = FActorFolders::Get().GetFolderProperties(*SharedData->RepresentingWorld, Path))
	{
		Props->bIsExpanded = Flags.bIsExpanded;
	}
}

void FFolderTreeItem::GenerateContextMenu(FMenuBuilder& MenuBuilder, SSceneOutliner& Outliner)
{
	auto SharedOutliner = StaticCastSharedRef<SSceneOutliner>(Outliner.AsShared());

	const FSlateIcon NewFolderIcon(FEditorStyle::GetStyleSetName(), "SceneOutliner.NewFolderIcon");
	
	MenuBuilder.AddMenuEntry(LOCTEXT("CreateSubFolder", "Create Sub Folder"), FText(), NewFolderIcon, FUIAction(FExecuteAction::CreateSP(this, &FFolderTreeItem::CreateSubFolder, TWeakPtr<SSceneOutliner>(SharedOutliner))));
	MenuBuilder.AddMenuEntry(LOCTEXT("RenameFolder", "Rename"), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(&Outliner, &SSceneOutliner::InitiateRename, AsShared())));
	MenuBuilder.AddMenuEntry(LOCTEXT("DeleteFolder", "Delete"), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &FFolderTreeItem::Delete)));
}

void FFolderTreeItem::PopulateDragDropPayload(FDragDropPayload& Payload) const
{
	if (!Payload.Folders)
	{
		Payload.Folders = FFolderPaths();
	}
	Payload.Folders->Add(Path);
}

FDragValidationInfo FFolderTreeItem::ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const
{
	FFolderDropTarget Target(Path);
	return Target.ValidateDrop(DraggedObjects, World);
}

void FFolderTreeItem::OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget)
{
	FFolderDropTarget Target(Path);
	return Target.OnDrop(DraggedObjects, World, ValidationInfo, DroppedOnWidget);
}

}	// namespace SceneOutliner

#undef LOCTEXT_NAMESPACE

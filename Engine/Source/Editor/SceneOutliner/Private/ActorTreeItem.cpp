// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ActorTreeItem.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "SceneOutlinerPublicTypes.h"
#include "DragAndDrop/ActorDragDropGraphEdOp.h"
#include "SceneOutlinerDragDrop.h"
#include "SceneOutlinerStandaloneTypes.h"



#include "Logging/MessageLog.h"
#include "SSocketChooser.h"

#define LOCTEXT_NAMESPACE "SceneOutliner_ActorTreeItem"

namespace SceneOutliner
{

FDragValidationInfo FActorDropTarget::ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const
{
	if (DraggedObjects.Folders)
	{
		return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric, LOCTEXT("FoldersOnActorError", "Cannot attach folders to actors"));
	}

	const AActor* DropTarget = Actor.Get();

	if (!DropTarget || !DraggedObjects.Actors)
	{
		return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric, FText());
	}	

	FText AttachErrorMsg;
	bool bCanAttach = true;
	bool bDraggedOntoAttachmentParent = true;

	const auto& DragActors = DraggedObjects.Actors.GetValue();
	for (const auto& DragActorPtr : DragActors)
	{
		AActor* DragActor = DragActorPtr.Get();
		if (DragActor)
		{
			if (bCanAttach)
			{
				if (DragActor->IsChildActor())
				{
					AttachErrorMsg = FText::Format(LOCTEXT("Error_AttachChildActor", "Cannot move {0} as it is a child actor."), FText::FromString(DragActor->GetActorLabel()));
					bCanAttach = bDraggedOntoAttachmentParent = false;
					break;
				}
				if (!GEditor->CanParentActors(DropTarget, DragActor, &AttachErrorMsg))
				{
					bCanAttach = false;
				}
			}

			if (DragActor->GetAttachParentActor() != DropTarget)
			{
				bDraggedOntoAttachmentParent = false;
			}
		}
	}

	const FText ActorLabel = FText::FromString(DropTarget->GetActorLabel());
	if (bDraggedOntoAttachmentParent)
	{
		if (DragActors.Num() == 1)
		{
			return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_CompatibleDetach, ActorLabel);
		}
		else
		{
			return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_CompatibleMultipleDetach, ActorLabel);
		}
	}
	else if (bCanAttach)
	{
		if (DragActors.Num() == 1)
		{
			return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_CompatibleAttach, ActorLabel);
		}
		else
		{
			return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_CompatibleMultipleAttach, ActorLabel);
		}
	}
	else
	{
		if (DragActors.Num() == 1)
		{
			return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric, AttachErrorMsg);
		}
		else
		{
			const FText ReasonText = FText::Format(LOCTEXT("DropOntoText", "{0}. {1}"), ActorLabel, AttachErrorMsg);
			return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleMultipleAttach, ReasonText);
		}
	}
}

void FActorDropTarget::OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget)
{
	AActor* DropActor = Actor.Get();
	if (!DropActor)
	{
		return;
	}

	FActorArray DraggedActors = DraggedObjects.Actors.GetValue();

	FMessageLog EditorErrors("EditorErrors");
	EditorErrors.NewPage(LOCTEXT("ActorAttachmentsPageLabel", "Actor attachment"));

	if (ValidationInfo.TooltipType == FActorDragDropGraphEdOp::ToolTip_CompatibleMultipleDetach || ValidationInfo.TooltipType == FActorDragDropGraphEdOp::ToolTip_CompatibleDetach)
	{
		const FScopedTransaction Transaction( LOCTEXT("UndoAction_DetachActors", "Detach actors") );

		for (const auto& WeakActor : DraggedActors)
		{
			if (auto* DragActor = WeakActor.Get())
			{
				DetachActorFromParent(DragActor);
			}
		}
	}
	else if (ValidationInfo.TooltipType == FActorDragDropGraphEdOp::ToolTip_CompatibleMultipleAttach || ValidationInfo.TooltipType == FActorDragDropGraphEdOp::ToolTip_CompatibleAttach)
	{
		// Show socket chooser if we have sockets to select

		//@TODO: Should create a menu for each component that contains sockets, or have some form of disambiguation within the menu (like a fully qualified path)
		// Instead, we currently only display the sockets on the root component
		USceneComponent* Component = DropActor->GetRootComponent();
		if ((Component != NULL) && (Component->HasAnySockets()))
		{
			// Create the popup
			FSlateApplication::Get().PushMenu(
				DroppedOnWidget,
				FWidgetPath(),
				SNew(SSocketChooserPopup)
				.SceneComponent( Component )
				.OnSocketChosen_Static(&FActorDropTarget::PerformAttachment, Actor, MoveTemp(DraggedActors) ),
				FSlateApplication::Get().GetCursorPos(),
				FPopupTransitionEffect( FPopupTransitionEffect::TypeInPopup )
				);
		}
		else
		{
			PerformAttachment(NAME_None, Actor, MoveTemp(DraggedActors));
		}
	}

	// Report errors
	EditorErrors.Notify(NSLOCTEXT("ActorAttachmentError", "AttachmentsFailed", "Attachments Failed!"));
}

void FActorDropTarget::PerformAttachment(FName SocketName, TWeakObjectPtr<AActor> Parent, const FActorArray NewAttachments)
{
	AActor* ParentActor = Parent.Get();
	if (ParentActor)
	{
		// modify parent and child
		const FScopedTransaction Transaction( LOCTEXT("UndoAction_PerformAttachment", "Attach actors") );

		// Attach each child
		bool bAttached = false;
		for (auto& Child : NewAttachments)
		{
			AActor* ChildActor = Child.Get();
			if (GEditor->CanParentActors(ParentActor, ChildActor))
			{
				GEditor->ParentActors(ParentActor, ChildActor, SocketName);
			}
		}
	}
}

void FActorDropTarget::DetachActorFromParent(AActor* ChildActor)
{
	USceneComponent* RootComp = ChildActor->GetRootComponent();
	if (RootComp && RootComp->GetAttachParent())
	{
		AActor* OldParent = RootComp->GetAttachParent()->GetOwner();
		OldParent->Modify();
		RootComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		
		ChildActor->SetFolderPath_Recursively(OldParent->GetFolderPath());
	}
}

FActorTreeItem::FActorTreeItem(AActor* InActor)
	: Actor(InActor)
	, ID(InActor)
{
	bExistsInCurrentWorldAndPIE = GEditor->ObjectsThatExistInEditorWorld.Get(InActor);
}

FTreeItemPtr FActorTreeItem::FindParent(const FTreeItemMap& ExistingItems) const
{
	AActor* ActorPtr = Actor.Get();
	if (!ActorPtr)
	{
		return nullptr;
	}

	// Parents should have already been added to the tree
	AActor* ParentActor = ActorPtr->GetAttachParentActor();
	if (ParentActor)
	{
		return ExistingItems.FindRef(ParentActor);
	}
	else
	{
		const bool bShouldShowFolders = SharedData->Mode == ESceneOutlinerMode::ActorBrowsing || SharedData->bOnlyShowFolders;

		const FName ActorFolder = ActorPtr->GetFolderPath();
		if (bShouldShowFolders && !ActorFolder.IsNone())
		{
			return ExistingItems.FindRef(ActorFolder);
		}
	}

	if (UWorld* World = ActorPtr->GetWorld())
	{
		return ExistingItems.FindRef(World);
	}

	return nullptr;
}

FTreeItemPtr FActorTreeItem::CreateParent() const
{
	AActor* ActorPtr = Actor.Get();
	if (!ActorPtr)
	{
		return nullptr;
	}

	AActor* ParentActor = ActorPtr->GetAttachParentActor();
	if (ParentActor && ensureMsgf(ParentActor != ActorPtr, TEXT("Encountered an Actor attached to itself (%s)"), *ParentActor->GetName()))
	{
		return MakeShareable(new FActorTreeItem(ParentActor));
	}
	else if(!ParentActor)
	{
		const bool bShouldShowFolders = SharedData->Mode == ESceneOutlinerMode::ActorBrowsing || SharedData->bOnlyShowFolders;

		const FName ActorFolder = ActorPtr->GetFolderPath();
		if (bShouldShowFolders && !ActorFolder.IsNone())
		{
			return MakeShareable(new FFolderTreeItem(ActorFolder));
		}

		if (UWorld* World = ActorPtr->GetWorld())
		{
			return MakeShareable(new FWorldTreeItem(World));
		}
	}

	return nullptr;
}

void FActorTreeItem::Visit(const ITreeItemVisitor& Visitor) const
{
	Visitor.Visit(*this);
}

void FActorTreeItem::Visit(const IMutableTreeItemVisitor& Visitor)
{
	Visitor.Visit(*this);
}

FTreeItemID FActorTreeItem::GetID() const
{
	return ID;
}

FString FActorTreeItem::GetDisplayString() const
{
	const AActor* ActorPtr = Actor.Get();
	return ActorPtr ? ActorPtr->GetActorLabel() : LOCTEXT("ActorLabelForMissingActor", "(Deleted Actor)").ToString();
}

int32 FActorTreeItem::GetTypeSortPriority() const
{
	return ETreeItemSortOrder::Actor;
}

bool FActorTreeItem::CanInteract() const
{
	AActor* ActorPtr = Actor.Get();
	if (!ActorPtr || !Flags.bInteractive)
	{
		return false;
	}

	const bool bInSelected = true;
	const bool bSelectEvenIfHidden = true;		// @todo outliner: Is this actually OK?
	if (!GEditor->CanSelectActor(ActorPtr, bInSelected, bSelectEvenIfHidden))
	{
		return false;
	}

	return true;
}

void FActorTreeItem::PopulateDragDropPayload(FDragDropPayload& Payload) const
{
	AActor* ActorPtr = Actor.Get();
	if (ActorPtr)
	{
		if (!Payload.Actors)
		{
			Payload.Actors = FActorArray();
		}
		Payload.Actors->Add(Actor);
	}
}

FDragValidationInfo FActorTreeItem::ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const
{
	FActorDropTarget Target(Actor.Get());
	return Target.ValidateDrop(DraggedObjects, World);
}

void FActorTreeItem::OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget)
{
	FActorDropTarget Target(Actor.Get());
	return Target.OnDrop(DraggedObjects, World, ValidationInfo, DroppedOnWidget);
}

}	// namespace SceneOutliner

#undef LOCTEXT_NAMESPACE

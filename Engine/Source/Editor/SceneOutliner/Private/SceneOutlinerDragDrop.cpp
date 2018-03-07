// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SceneOutlinerDragDrop.h"
#include "ITreeItem.h"
#include "SceneOutlinerStandaloneTypes.h"

#define LOCTEXT_NAMESPACE "SSceneOutliner"

namespace SceneOutliner
{

	FSceneOutlinerDragDropOp::FSceneOutlinerDragDropOp(const FDragDropPayload& DraggedObjects)
		: OverrideText()
		, OverrideIcon(nullptr)
	{
		if (DraggedObjects.Actors)
		{
			ActorOp = MakeShareable(new FActorDragDropOp);
			ActorOp->Init(DraggedObjects.Actors.GetValue());
		}

		if (DraggedObjects.Folders)
		{
			FolderOp = MakeShareable(new FFolderDragDropOp);
			FolderOp->Init(DraggedObjects.Folders.GetValue());
		}
	}
	
	EVisibility FSceneOutlinerDragDropOp::GetOverrideVisibility() const
	{
		return OverrideText.IsEmpty() && OverrideIcon == nullptr ? EVisibility::Collapsed : EVisibility::Visible;
	}
	
	EVisibility FSceneOutlinerDragDropOp::GetDefaultVisibility() const
	{
		return OverrideText.IsEmpty() && OverrideIcon == nullptr ? EVisibility::Visible : EVisibility::Collapsed;
	}

	TSharedPtr<SWidget> FSceneOutlinerDragDropOp::GetDefaultDecorator() const
	{
		TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

		VerticalBox->AddSlot()
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("Graph.ConnectorFeedback.Border"))
			.Visibility(this, &FSceneOutlinerDragDropOp::GetOverrideVisibility)
			.Content()
			[			
				SNew(SHorizontalBox)
				
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( 0.0f, 0.0f, 3.0f, 0.0f )
				[
					SNew( SImage )
					.Image( this, &FSceneOutlinerDragDropOp::GetOverrideIcon )
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign( VAlign_Center )
				[
					SNew(STextBlock) 
					.Text( this, &FSceneOutlinerDragDropOp::GetOverrideText )
				]
			]
		];

		if (FolderOp.IsValid())
		{
			auto Content = FolderOp->GetDefaultDecorator();
			if (Content.IsValid())
			{
				Content->SetVisibility(TAttribute<EVisibility>(this, &FSceneOutlinerDragDropOp::GetDefaultVisibility));
				VerticalBox->AddSlot()[ Content.ToSharedRef() ];
			}
		}

		if (ActorOp.IsValid())
		{
			auto Content = ActorOp->GetDefaultDecorator();
			if (Content.IsValid())
			{
				Content->SetVisibility(TAttribute<EVisibility>(this, &FSceneOutlinerDragDropOp::GetDefaultVisibility));
				VerticalBox->AddSlot()[ Content.ToSharedRef() ];
			}
		}

		return VerticalBox;
	}

	void FFolderDragDropOp::Init(FFolderPaths InFolders)
	{
		Folders = MoveTemp(InFolders);

		CurrentIconBrush = FEditorStyle::Get().GetBrush(TEXT("SceneOutliner.FolderClosed"));
		if (Folders.Num() == 1)
		{
			CurrentHoverText = FText::FromName(GetFolderLeafName(Folders[0]));
		}
		else
		{
			CurrentHoverText = FText::Format(NSLOCTEXT("FFolderDragDropOp", "FormatFolders", "{0} Folders"), FText::AsNumber(Folders.Num()));
		}
	}

	TSharedPtr<FDragDropOperation> CreateDragDropOperation(const TArray<FTreeItemPtr>& InTreeItems)
	{
		FDragDropPayload DraggedObjects;
		for (const auto& Item : InTreeItems)
		{
			Item->PopulateDragDropPayload(DraggedObjects);
		}

		if (DraggedObjects.Folders)
		{
			TSharedPtr<FSceneOutlinerDragDropOp> OutlinerOp = MakeShareable(new FSceneOutlinerDragDropOp(DraggedObjects));
			OutlinerOp->Construct();
			return OutlinerOp;
		}
		else if (DraggedObjects.Actors)
		{
			return FActorDragDropGraphEdOp::New(DraggedObjects.Actors.GetValue());
		}
		
		return nullptr;
	}

	FDragDropPayload::FDragDropPayload()
	{

	}

	bool FDragDropPayload::ParseDrag(const FDragDropOperation& Operation)
	{
		bool bApplicable = false;

		if (Operation.IsOfType<FSceneOutlinerDragDropOp>())
		{
			const auto& OutlinerOp = static_cast<const FSceneOutlinerDragDropOp&>(Operation);
			if (OutlinerOp.FolderOp.IsValid())
			{
				Folders = OutlinerOp.FolderOp->Folders;
			}
			if (OutlinerOp.ActorOp.IsValid())
			{
				Actors = OutlinerOp.ActorOp->Actors;
			}
			bApplicable = true;
		}
		else if (Operation.IsOfType<FActorDragDropOp>())
		{
			Actors = static_cast<const FActorDragDropOp&>(Operation).Actors;
			bApplicable = true;
		}
		else if (Operation.IsOfType<FFolderDragDropOp>())
		{
			Folders = static_cast<const FFolderDragDropOp&>(Operation).Folders;
			bApplicable = true;
		}

		return bApplicable;
	}
}

#undef LOCTEXT_NAMESPACE

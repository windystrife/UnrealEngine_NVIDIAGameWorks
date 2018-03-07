// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DisplayNodes/SequencerFolderNode.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Views/STableRow.h"
#include "EditorStyleSet.h"
#include "Engine/Engine.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EngineGlobals.h"
#include "MovieScene.h"
#include "ISequencer.h"
#include "DisplayNodes/SequencerObjectBindingNode.h"
#include "DisplayNodes/SequencerTrackNode.h"
#include "SSequencer.h"
#include "MovieSceneFolder.h"
#include "SequencerUtilities.h"
#include "MovieSceneSequence.h"
#include "SequencerDisplayNodeDragDropOp.h"
#include "ScopedTransaction.h"
#include "Widgets/Colors/SColorPicker.h"

#define LOCTEXT_NAMESPACE "SequencerFolderNode"

FSequencerFolderNode::FSequencerFolderNode( UMovieSceneFolder& InMovieSceneFolder, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree )
	: FSequencerDisplayNode( InMovieSceneFolder.GetFolderName(), InParentNode, InParentTree )
	, MovieSceneFolder(InMovieSceneFolder)
{
	FolderOpenBrush = FEditorStyle::GetBrush( "ContentBrowser.AssetTreeFolderOpen" );
	FolderClosedBrush = FEditorStyle::GetBrush( "ContentBrowser.AssetTreeFolderClosed" );
}


ESequencerNode::Type FSequencerFolderNode::GetType() const
{
	return ESequencerNode::Folder;
}


float FSequencerFolderNode::GetNodeHeight() const
{
	// TODO: Add another constant.
	return SequencerLayoutConstants::FolderNodeHeight;
}


FNodePadding FSequencerFolderNode::GetNodePadding() const
{
	return FNodePadding(4, 4);
}


bool FSequencerFolderNode::CanRenameNode() const
{
	return true;
}


FText FSequencerFolderNode::GetDisplayName() const
{
	return FText::FromName( MovieSceneFolder.GetFolderName() );
}


void FSequencerFolderNode::SetDisplayName( const FText& NewDisplayName )
{
	if ( MovieSceneFolder.GetFolderName() != FName( *NewDisplayName.ToString() ) )
	{
		TArray<FName> SiblingNames;
		TSharedPtr<FSequencerDisplayNode> ParentSeqNode = GetParent();
		if ( ParentSeqNode.IsValid() )
		{
			for ( TSharedRef<FSequencerDisplayNode> SiblingNode : ParentSeqNode->GetChildNodes() )
			{
				if ( SiblingNode != AsShared() )
				{
					SiblingNames.Add( SiblingNode->GetNodeName() );
				}
			}
		}

		FName UniqueName = FSequencerUtilities::GetUniqueName( FName( *NewDisplayName.ToString() ), SiblingNames );

		const FScopedTransaction Transaction( NSLOCTEXT( "SequencerFolderNode", "RenameFolder", "Rename folder." ) );
		MovieSceneFolder.Modify();
		MovieSceneFolder.SetFolderName( UniqueName );
	}
}


const FSlateBrush* FSequencerFolderNode::GetIconBrush() const
{
	return IsExpanded()
		? FolderOpenBrush
		: FolderClosedBrush;
}


FSlateColor FSequencerFolderNode::GetIconColor() const
{
	return FSlateColor(MovieSceneFolder.GetFolderColor());
}

bool FSequencerFolderNode::CanDrag() const
{
	return true;
}


TOptional<EItemDropZone> FSequencerFolderNode::CanDrop( FSequencerDisplayNodeDragDropOp& DragDropOp, EItemDropZone ItemDropZone ) const
{
	DragDropOp.ResetToDefaultToolTip();

	if ( ItemDropZone == EItemDropZone::AboveItem )
	{
		if ( GetParent().IsValid() )
		{
			// When dropping above, only allow it for root level nodes.
			return TOptional<EItemDropZone>();
		}
		else
		{
			// Make sure there are no folder name collisions with the root folders
			UMovieScene* FocusedMovieScene = GetParentTree().GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();
			TSet<FName> RootFolderNames;
			for ( UMovieSceneFolder* RootFolder : FocusedMovieScene->GetRootFolders() )
			{
				RootFolderNames.Add( RootFolder->GetFolderName() );
			}

			for ( TSharedRef<FSequencerDisplayNode> DraggedNode : DragDropOp.GetDraggedNodes() )
			{
				if ( DraggedNode->GetType() == ESequencerNode::Folder )
				{
					TSharedRef<FSequencerFolderNode> DraggedFolder = StaticCastSharedRef<FSequencerFolderNode>( DraggedNode );
					if ( RootFolderNames.Contains( DraggedFolder->GetFolder().GetFolderName() ) )
					{
						DragDropOp.CurrentHoverText = FText::Format(
							NSLOCTEXT( "SeqeuencerFolderNode", "DuplicateRootFolderDragErrorFormat", "Root folder with name '{0}' already exists." ),
							FText::FromName( DraggedFolder->GetFolder().GetFolderName() ) );
						return TOptional<EItemDropZone>();
					}
				}
			}
		}
		return TOptional<EItemDropZone>( EItemDropZone::AboveItem );
	}
	else
	{
		// When dropping onto, don't allow dropping into the same folder, don't allow dropping
		// parents into children, and don't allow duplicate folder names.
		TSet<FName> ChildFolderNames;
		for ( UMovieSceneFolder* ChildFolder : GetFolder().GetChildFolders() )
		{
			ChildFolderNames.Add( ChildFolder->GetFolderName() );
		}

		for ( TSharedRef<FSequencerDisplayNode> DraggedNode : DragDropOp.GetDraggedNodes() )
		{
			TSharedPtr<FSequencerDisplayNode> ParentSeqNode = DraggedNode->GetParent();
			if ( ParentSeqNode.IsValid() && ParentSeqNode.Get() == this )
			{
				DragDropOp.CurrentHoverText = NSLOCTEXT( "SeqeuencerFolderNode", "SameParentDragError", "Can't drag a node onto the same parent." );
				return TOptional<EItemDropZone>();
			}

			if ( DraggedNode->GetType() == ESequencerNode::Folder )
			{
				TSharedRef<FSequencerFolderNode> DraggedFolder = StaticCastSharedRef<FSequencerFolderNode>( DraggedNode );
				if ( ChildFolderNames.Contains( DraggedFolder->GetFolder().GetFolderName() ) )
				{
					DragDropOp.CurrentHoverText = FText::Format(
						NSLOCTEXT( "SeqeuencerFolderNode", "DuplicateChildFolderDragErrorFormat", "Folder with name '{0}' already exists." ),
						FText::FromName( DraggedFolder->GetFolder().GetFolderName() ) );
					return TOptional<EItemDropZone>();
				}
			}
		}
		TSharedPtr<FSequencerDisplayNode> CurrentNode = SharedThis( ( FSequencerDisplayNode* )this );
		while ( CurrentNode.IsValid() )
		{
			if ( DragDropOp.GetDraggedNodes().Contains( CurrentNode ) )
			{
				DragDropOp.CurrentHoverText = NSLOCTEXT( "SeqeuencerFolderNode", "ParentIntoChildDragError", "Can't drag a parent node into one of it's children." );
				return TOptional<EItemDropZone>();
			}
			CurrentNode = CurrentNode->GetParent();
		}
		return TOptional<EItemDropZone>( EItemDropZone::OntoItem );
	}
}


void FSequencerFolderNode::Drop( const TArray<TSharedRef<FSequencerDisplayNode>>& DraggedNodes, EItemDropZone ItemDropZone )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "SequencerFolderNode", "MoveIntoFolder", "Move items into folder." ) );
	GetFolder().SetFlags(RF_Transactional);
	GetFolder().Modify();
	for ( TSharedRef<FSequencerDisplayNode> DraggedNode : DraggedNodes )
	{
		TSharedPtr<FSequencerDisplayNode> ParentSeqNode = DraggedNode->GetParent();
		switch ( DraggedNode->GetType() )
		{
			case ESequencerNode::Folder:
			{
				TSharedRef<FSequencerFolderNode> DraggedFolderNode = StaticCastSharedRef<FSequencerFolderNode>( DraggedNode );
				UMovieScene* FocusedMovieScene = GetParentTree().GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();

				if ( ItemDropZone == EItemDropZone::OntoItem )
				{
					GetFolder().AddChildFolder( &DraggedFolderNode->GetFolder() );
				}
				else
				{
					FocusedMovieScene->Modify();
					FocusedMovieScene->GetRootFolders().Add( &DraggedFolderNode->GetFolder() );
				}
				
				if ( ParentSeqNode.IsValid() )
				{
					checkf( ParentSeqNode->GetType() == ESequencerNode::Folder, TEXT( "Can not remove from unsupported parent node." ) );
					TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>( ParentSeqNode );
					ParentFolder->GetFolder().Modify();
					ParentFolder->GetFolder().RemoveChildFolder( &DraggedFolderNode->GetFolder() );
				}
				else
				{
					FocusedMovieScene->Modify();
					FocusedMovieScene->GetRootFolders().Remove( &DraggedFolderNode->GetFolder() );
				}

				break;
			}
			case ESequencerNode::Track:
			{
				TSharedRef<FSequencerTrackNode> DraggedTrackNode = StaticCastSharedRef<FSequencerTrackNode>( DraggedNode );
				
				if( ItemDropZone == EItemDropZone::OntoItem )
				{
					GetFolder().AddChildMasterTrack( DraggedTrackNode->GetTrack() );
				}
				
				if ( ParentSeqNode.IsValid() )
				{
					checkf( ParentSeqNode->GetType() == ESequencerNode::Folder, TEXT( "Can not remove from unsupported parent node." ) );
					TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>( ParentSeqNode );
					ParentFolder->GetFolder().Modify();
					ParentFolder->GetFolder().RemoveChildMasterTrack( DraggedTrackNode->GetTrack() );
				}

				break;
			}
			case ESequencerNode::Object:
			{
				TSharedRef<FSequencerObjectBindingNode> DraggedObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>( DraggedNode );
				
				if ( ItemDropZone == EItemDropZone::OntoItem )
				{
					GetFolder().AddChildObjectBinding( DraggedObjectBindingNode->GetObjectBinding() );
				}

				if ( ParentSeqNode.IsValid() )
				{
					checkf( ParentSeqNode->GetType() == ESequencerNode::Folder, TEXT( "Can not remove from unsupported parent node." ) );
					TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>( ParentSeqNode );
					ParentFolder->GetFolder().Modify();
					ParentFolder->GetFolder().RemoveChildObjectBinding( DraggedObjectBindingNode->GetObjectBinding() );
				}

				break;
			}
		}
	}
	SetExpansionState( true );
	ParentTree.GetSequencer().NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemsChanged );
}

void FSequencerFolderNode::BuildContextMenu(FMenuBuilder& MenuBuilder)
{
	FSequencerDisplayNode::BuildContextMenu(MenuBuilder);

	TSharedRef<FSequencerFolderNode> ThisNode = SharedThis(this);

	MenuBuilder.BeginSection("Folder", LOCTEXT("FolderContextMenuSectionName", "Folder"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetColor", "Set Color"),
			LOCTEXT("SetColorTooltip", "Set the folder color"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FSequencerFolderNode::SetFolderColor))
		);
	}
	MenuBuilder.EndSection();
}

FColor InitialFolderColor;
bool bFolderPickerWasCancelled;

void FSequencerFolderNode::SetFolderColor()
{
	InitialFolderColor = MovieSceneFolder.GetFolderColor();
	bFolderPickerWasCancelled = false;

	FColorPickerArgs PickerArgs;
	PickerArgs.bUseAlpha = false;
	PickerArgs.DisplayGamma = TAttribute<float>::Create( TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma) );
	PickerArgs.InitialColorOverride = InitialFolderColor.ReinterpretAsLinear();
	PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP( this, &FSequencerFolderNode::OnColorPickerPicked);
	PickerArgs.OnColorPickerWindowClosed = FOnWindowClosed::CreateSP( this, &FSequencerFolderNode::OnColorPickerClosed);
	PickerArgs.OnColorPickerCancelled  = FOnColorPickerCancelled::CreateSP( this, &FSequencerFolderNode::OnColorPickerCancelled );

	OpenColorPicker(PickerArgs);
}

void FSequencerFolderNode::OnColorPickerPicked(FLinearColor NewFolderColor)
{			
	MovieSceneFolder.SetFolderColor(NewFolderColor.ToFColor(false));
}

void FSequencerFolderNode::OnColorPickerClosed(const TSharedRef<SWindow>& Window)
{	
	if (!bFolderPickerWasCancelled)
	{
		const FScopedTransaction Transaction( NSLOCTEXT( "SequencerFolderNode", "SetFolderColor", "Set Folder Color" ) );
		
		FColor CurrentColor = MovieSceneFolder.GetFolderColor();
		MovieSceneFolder.SetFolderColor(InitialFolderColor);
		MovieSceneFolder.Modify();
		MovieSceneFolder.SetFolderColor(CurrentColor);
	}
}

void FSequencerFolderNode::OnColorPickerCancelled(FLinearColor NewFolderColor)
{
	bFolderPickerWasCancelled = true;

	MovieSceneFolder.SetFolderColor(InitialFolderColor);
}

void FSequencerFolderNode::AddChildNode( TSharedRef<FSequencerDisplayNode> ChildNode )
{
	AddChildAndSetParent( ChildNode );
}


UMovieSceneFolder& FSequencerFolderNode::GetFolder() const
{
	return MovieSceneFolder;
}

#undef LOCTEXT_NAMESPACE

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SequencerCurveOwner.h"
#include "Containers/List.h"
#include "ISequencer.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "DisplayNodes/SequencerSectionKeyAreaNode.h"
#include "IKeyArea.h"
#include "DisplayNodes/SequencerTrackNode.h"
#include "Sequencer.h"

struct FDisplayNodeAndKeyArea
{
	FDisplayNodeAndKeyArea( TSharedPtr<FSequencerDisplayNode> InDisplayNode, TSharedPtr<IKeyArea> InKeyArea )
	{
		DisplayNode = InDisplayNode;
		KeyArea = InKeyArea;
	}
	TSharedPtr<FSequencerDisplayNode> DisplayNode;
	TSharedPtr<IKeyArea> KeyArea;
};

void GetAllKeyAreas( TSharedPtr<FSequencerNodeTree> InSequencerNodeTree, TArray<FDisplayNodeAndKeyArea>& DisplayNodesAndKeyAreas )
{
	TDoubleLinkedList<TSharedRef<FSequencerDisplayNode>> NodesToProcess;
	for ( auto RootNode : InSequencerNodeTree->GetRootNodes() )
	{
		NodesToProcess.AddTail( RootNode );
	}

	while ( NodesToProcess.Num() > 0 )
	{
		TSharedRef<FSequencerDisplayNode> Node = NodesToProcess.GetHead()->GetValue();
		NodesToProcess.RemoveNode( Node );
		if ( Node->GetType() == ESequencerNode::Track )
		{
			TSharedRef<FSequencerTrackNode> TrackNode = StaticCastSharedRef<FSequencerTrackNode>( Node );
			TSharedPtr<FSequencerSectionKeyAreaNode> TopLevelKeyNode = TrackNode->GetTopLevelKeyNode();
			if ( TopLevelKeyNode.IsValid() )
			{
				for ( TSharedRef<IKeyArea> KeyArea : TopLevelKeyNode->GetAllKeyAreas() )
				{
					DisplayNodesAndKeyAreas.Add( FDisplayNodeAndKeyArea( TrackNode, KeyArea ) );
				}
			}
		}
		if ( Node->GetType() == ESequencerNode::KeyArea )
		{
			for ( TSharedRef<IKeyArea> KeyArea : StaticCastSharedRef<FSequencerSectionKeyAreaNode>( Node )->GetAllKeyAreas() )
			{
				DisplayNodesAndKeyAreas.Add( FDisplayNodeAndKeyArea( Node, KeyArea ) );
			}
		}
		for ( auto ChildNode : Node->GetChildNodes() )
		{
			NodesToProcess.AddTail( ChildNode );
		}
	}
}

FName BuildCurveName( TSharedPtr<FSequencerDisplayNode> KeyAreaNode )
{
	FString CurveName;
	TSharedPtr<FSequencerDisplayNode> CurrentNameNode = KeyAreaNode;
	TArray<FString> NameParts;
	while ( CurrentNameNode.IsValid() )
	{
		NameParts.Insert( CurrentNameNode->GetDisplayName().ToString(), 0);
		CurrentNameNode = CurrentNameNode->GetParent();
	}
	return FName(*FString::Join(NameParts, TEXT(" - ")));
}

bool IsNodeOrAncestorSelected(TSharedPtr<FSequencerDisplayNode> DisplayNode)
{
	if (!DisplayNode.IsValid())
	{
		return false;
	}
	
	if (DisplayNode->GetSequencer().GetSelection().IsSelected(DisplayNode.ToSharedRef()))
	{
		return true;
	}

	return IsNodeOrAncestorSelected(DisplayNode->GetParent());
}

FSequencerCurveOwner::FSequencerCurveOwner( TSharedPtr<FSequencerNodeTree> InSequencerNodeTree, ECurveEditorCurveVisibility::Type CurveVisibility )
{
	SequencerNodeTree = InSequencerNodeTree;

	TArray<FDisplayNodeAndKeyArea> DisplayNodesAndKeyAreas;
	GetAllKeyAreas( SequencerNodeTree, DisplayNodesAndKeyAreas );
	for (const FDisplayNodeAndKeyArea& DisplayNodeAndKeyArea : DisplayNodesAndKeyAreas )
	{
		FRichCurve* RichCurve = DisplayNodeAndKeyArea.KeyArea->GetRichCurve();
		if ( RichCurve != nullptr )
		{
			bool bAddCurve = false;
			switch ( CurveVisibility )
			{
			case ECurveEditorCurveVisibility::AllCurves:
				bAddCurve = true;
				break;
			case ECurveEditorCurveVisibility::SelectedCurves:
				bAddCurve = IsNodeOrAncestorSelected(DisplayNodeAndKeyArea.DisplayNode);
				break;
			case ECurveEditorCurveVisibility::AnimatedCurves:
				bAddCurve = RichCurve->GetNumKeys() > 0;
				break;
			}

			if ( bAddCurve )
			{
				FName CurveName = BuildCurveName( DisplayNodeAndKeyArea.DisplayNode );
				Curves.Add( FRichCurveEditInfo( RichCurve, CurveName ) );
				ConstCurves.Add( FRichCurveEditInfoConst( RichCurve, CurveName ) );
				EditInfoToSectionMap.Add( FRichCurveEditInfo( RichCurve, CurveName ), DisplayNodeAndKeyArea.KeyArea->GetOwningSection() );
			}
		}
	}
}

TArray<FRichCurve*> FSequencerCurveOwner::GetSelectedCurves() const
{
	TArray<FRichCurve*> SelectedCurves;

	TArray<FDisplayNodeAndKeyArea> DisplayNodesAndKeyAreas;
	GetAllKeyAreas( SequencerNodeTree, DisplayNodesAndKeyAreas );
	for ( const FDisplayNodeAndKeyArea& DisplayNodeAndKeyArea : DisplayNodesAndKeyAreas )
	{
		FRichCurve* RichCurve = DisplayNodeAndKeyArea.KeyArea->GetRichCurve();
		if ( RichCurve != nullptr && SelectedCurves.Find(RichCurve) == INDEX_NONE )
		{
			if (IsNodeOrAncestorSelected(DisplayNodeAndKeyArea.DisplayNode))
			{
				SelectedCurves.Add(RichCurve);
			}
		}
	}
	return SelectedCurves;
}

TArray<FRichCurveEditInfoConst> FSequencerCurveOwner::GetCurves() const
{
	return ConstCurves;
}

TArray<FRichCurveEditInfo> FSequencerCurveOwner::GetCurves()
{
	return Curves;
};

void FSequencerCurveOwner::ModifyOwner()
{
	TArray<UMovieSceneSection*> Owners;
	EditInfoToSectionMap.GenerateValueArray( Owners );
	for ( auto Owner : Owners )
	{
		Owner->Modify();
	}
}

TArray<const UObject*> FSequencerCurveOwner::GetOwners() const
{
	TArray<UMovieSceneSection*> Owners;
	EditInfoToSectionMap.GenerateValueArray( Owners );

	TArray<const UObject*> CastedOwners;
	CastedOwners.Reserve(Owners.Num());
	for (auto Owner : Owners)
	{
		const UObject* CastedOwner = Owner;

		if (CastedOwner)
		{
			CastedOwners.Add(CastedOwner);
		}
	}

	return CastedOwners;
}

void FSequencerCurveOwner::MakeTransactional()
{
	TArray<UMovieSceneSection*> Owners;
	EditInfoToSectionMap.GenerateValueArray( Owners );
	for ( auto Owner : Owners )
	{
		Owner->SetFlags( Owner->GetFlags() | RF_Transactional );
	}
}

void FSequencerCurveOwner::OnCurveChanged( const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos )
{
	// Whenever a curve changes make sure to resize its section so that the curve fits.
	for ( auto& ChangedCurveEditInfo : ChangedCurveEditInfos )
	{
		UMovieSceneSection** OwningSection = EditInfoToSectionMap.Find(ChangedCurveEditInfo);
		if ( OwningSection != nullptr )
		{
			float CurveStart;
			float CurveEnd;
			ChangedCurveEditInfo.CurveToEdit->GetTimeRange(CurveStart, CurveEnd);
			if ( (*OwningSection)->GetStartTime() > CurveStart )
			{
				(*OwningSection)->SetStartTime(CurveStart);
			}			
			if ( (*OwningSection)->GetEndTime() < CurveEnd )
			{
				(*OwningSection)->SetEndTime( CurveEnd );
			}
		}
	}

	SequencerNodeTree->GetSequencer().NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
}

bool FSequencerCurveOwner::IsValidCurve( FRichCurveEditInfo CurveInfo )
{
	return EditInfoToSectionMap.Contains(CurveInfo);
}

FLinearColor FSequencerCurveOwner::GetCurveColor( FRichCurveEditInfo CurveInfo ) const
{
	FString CurveName = CurveInfo.CurveName.ToString();
	if (CurveName.EndsWith(TEXT("- X")) || CurveName.EndsWith(TEXT("- Red")))
	{
		return FLinearColor(1.0f, 0.0f, 0.0f);
	}
	else if (CurveName.EndsWith(TEXT("- Y")) || CurveName.EndsWith(TEXT("- Green")))
	{
		return FLinearColor(0.0f, 1.0f, 0.0f);
	}
	else if (CurveName.EndsWith(TEXT("- Z")) || CurveName.EndsWith(TEXT("- Blue")))
	{
		return FLinearColor(0.05f, 0.05f, 1.0f);
	}

	return FCurveOwnerInterface::GetCurveColor(CurveInfo);
}

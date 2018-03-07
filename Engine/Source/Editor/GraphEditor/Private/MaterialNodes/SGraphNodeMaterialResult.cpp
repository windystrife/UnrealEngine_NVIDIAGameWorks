// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MaterialNodes/SGraphNodeMaterialResult.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphNode_Root.h"
#include "SGraphPanel.h"
#include "TutorialMetaData.h"
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"

/////////////////////////////////////////////////////
// SGraphNodeMaterialResult

void SGraphNodeMaterialResult::Construct(const FArguments& InArgs, UMaterialGraphNode_Root* InNode)
{
	this->GraphNode = InNode;
	this->RootNode = InNode;

	this->SetCursor(EMouseCursor::CardinalCross);

	this->UpdateGraphNode();
}

void SGraphNodeMaterialResult::CreatePinWidgets()
{
	// Create Pin widgets for each of the pins.
	for( int32 PinIndex=0; PinIndex < GraphNode->Pins.Num(); ++PinIndex )
	{
		UEdGraphPin* CurPin = GraphNode->Pins[PinIndex];

		bool bHideNoConnectionPins = false;
		
		if (OwnerGraphPanelPtr.IsValid())
		{
			bHideNoConnectionPins = OwnerGraphPanelPtr.Pin()->GetPinVisibility() == SGraphEditor::Pin_HideNoConnection;
		}

		const bool bPinHasConections = CurPin->LinkedTo.Num() > 0;

		//const bool bPinDesiresToBeHidden = CurPin->bHidden || (bHideNoConnectionPins && !bPinHasConections);

		UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(GraphNode->GetGraph());

		check(PinIndex < MaterialGraph->MaterialInputs.Num());

		const bool bPinDesiresToBeHidden = !MaterialGraph->MaterialInputs[PinIndex].IsVisiblePin(MaterialGraph->Material) || (bHideNoConnectionPins && !bPinHasConections);

		if (!bPinDesiresToBeHidden)
		{
			TSharedPtr<SGraphPin> NewPin = CreatePinWidget(CurPin);
			check(NewPin.IsValid());

			TSharedPtr<SToolTip> ToolTipWidget = IDocumentation::Get()->CreateToolTip(MaterialGraph->MaterialInputs[PinIndex].GetToolTip(), nullptr, FString( TEXT("") ), FString( TEXT("") ) );
			NewPin->SetToolTip( ToolTipWidget.ToSharedRef() );

			this->AddPin(NewPin.ToSharedRef());
		}
	}
}

void SGraphNodeMaterialResult::MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter)
{
	SGraphNode::MoveTo(NewPosition, NodeFilter);

	RootNode->Material->EditorX = RootNode->NodePosX;
	RootNode->Material->EditorY = RootNode->NodePosY;
	RootNode->Material->MarkPackageDirty();
	RootNode->Material->MaterialGraph->MaterialDirtyDelegate.ExecuteIfBound();
}


void SGraphNodeMaterialResult::PopulateMetaTag(FGraphNodeMetaData* TagMeta) const
{
	if( (GraphNode != nullptr) && (RootNode != nullptr) )
	{		
		UMaterialGraph* OuterGraph = RootNode->GetTypedOuter<UMaterialGraph>();
		if (OuterGraph != nullptr)
		{
			TagMeta->OuterName = OuterGraph->OriginalMaterialFullName;
			// There is only one root node - so we dont need a guid. 
			TagMeta->Tag = FName(*FString::Printf(TEXT("MaterialResNode_%s"), *TagMeta->OuterName));
			TagMeta->GUID.Invalidate();
			TagMeta->FriendlyName = FString::Printf(TEXT("Material Result node in %s"), *TagMeta->OuterName);
 		}		
	}
}

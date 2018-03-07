// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SoundSubmixGraph/SoundSubmixGraphSchema.h"
#include "SoundSubmixGraph/SoundSubmixGraphNode.h"
#include "SoundSubmixGraph/SoundSubmixGraph.h"
#include "AssetData.h"
#include "ScopedTransaction.h"
#include "GraphEditorActions.h"
#include "SoundSubmixEditorUtilities.h"
#include "Sound/SoundSubmix.h"
#include "GenericCommands.h"
#include "MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "SoundSubmixSchema"

UEdGraphNode* FSoundSubmixGraphSchemaAction_NewNode::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	FSoundSubmixEditorUtilities::CreateSoundSubmix(ParentGraph, FromPin, Location, NewSoundSubmixName);
	return NULL;
}

USoundSubmixGraphSchema::USoundSubmixGraphSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USoundSubmixGraphSchema::ConnectionCausesLoop(const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const
{
	USoundSubmixGraphNode* InputNode = CastChecked<USoundSubmixGraphNode>(InputPin->GetOwningNode());
	USoundSubmixGraphNode* OutputNode = CastChecked<USoundSubmixGraphNode>(OutputPin->GetOwningNode());

	return InputNode->SoundSubmix->RecurseCheckChild(OutputNode->SoundSubmix);
}

void USoundSubmixGraphSchema::GetBreakLinkToSubMenuActions(class FMenuBuilder& MenuBuilder, UEdGraphPin* InGraphPin)
{
	// Make sure we have a unique name for every entry in the list
	TMap<FString, uint32> LinkTitleCount;

	// Add all the links we could break from
	for(TArray<class UEdGraphPin*>::TConstIterator Links(InGraphPin->LinkedTo); Links; ++Links)
	{
		UEdGraphPin* Pin = *Links;
		FString TitleString = Pin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString();
		FText Title = FText::FromString( TitleString );
		if (Pin->PinName != TEXT(""))
		{
			TitleString = FString::Printf(TEXT("%s (%s)"), *TitleString, *Pin->PinName);

			// Add name of connection if possible
			FFormatNamedArguments Args;
			Args.Add( TEXT("NodeTitle"), Title );
			Args.Add( TEXT("PinName"), Pin->GetDisplayName() );
			Title = FText::Format( LOCTEXT("BreakDescPin", "{NodeTitle} ({PinName})"), Args );
		}

		uint32 &Count = LinkTitleCount.FindOrAdd(TitleString);

		FText Description;
		FFormatNamedArguments Args;
		Args.Add(TEXT("NodeTitle"), Title);
		Args.Add(TEXT("NumberOfNodes"), Count);

		if (Count == 0)
		{
			Description = FText::Format( LOCTEXT("BreakDesc", "Break link to {NodeTitle}"), Args );
		}
		else
		{
			Description = FText::Format( LOCTEXT("BreakDescMulti", "Break link to {NodeTitle} ({NumberOfNodes})"), Args );
		}
		++Count;

		MenuBuilder.AddMenuEntry( Description, Description, FSlateIcon(), FUIAction(
			FExecuteAction::CreateUObject((USoundSubmixGraphSchema*const)this, &USoundSubmixGraphSchema::BreakSinglePinLink, const_cast< UEdGraphPin* >(InGraphPin), *Links)));
	}
}

void USoundSubmixGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	const FText Name = LOCTEXT("NewSoundSubmix", "New Sound Submix");
	const FText ToolTip = LOCTEXT("NewSoundSubmixTooltip", "Create a new sound submix");
	
	TSharedPtr<FSoundSubmixGraphSchemaAction_NewNode> NewAction(new FSoundSubmixGraphSchemaAction_NewNode(FText::GetEmpty(), Name, ToolTip, 0));

	ContextMenuBuilder.AddAction(NewAction);
}

void USoundSubmixGraphSchema::GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, class FMenuBuilder* MenuBuilder, bool bIsDebugging) const
{
	if (InGraphPin)
	{
		MenuBuilder->BeginSection("SoundSubmixGraphSchemaPinActions", LOCTEXT("PinActionsMenuHeader", "Pin Actions"));
		{
			// Only display the 'Break Links' option if there is a link to break!
			if (InGraphPin->LinkedTo.Num() > 0)
			{
				MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().BreakPinLinks);

				// add sub menu for break link to
				if(InGraphPin->LinkedTo.Num() > 1)
				{
					MenuBuilder->AddSubMenu(
						LOCTEXT("BreakLinkTo", "Break Link To..." ),
						LOCTEXT("BreakSpecificLinks", "Break a specific link..." ),
						FNewMenuDelegate::CreateUObject((USoundSubmixGraphSchema*const)this, &USoundSubmixGraphSchema::GetBreakLinkToSubMenuActions, const_cast<UEdGraphPin*>(InGraphPin)));
				}
				else
				{
					((USoundSubmixGraphSchema*const)this)->GetBreakLinkToSubMenuActions(*MenuBuilder, const_cast<UEdGraphPin*>(InGraphPin));
				}
			}
		}
		MenuBuilder->EndSection();
	}
	else if (InGraphNode)
	{
		const USoundSubmixGraphNode* SoundGraphNode = Cast<const USoundSubmixGraphNode>(InGraphNode);

		MenuBuilder->BeginSection("SoundSubmixGraphSchemaNodeActions", LOCTEXT("ClassActionsMenuHeader", "SoundSubmix Actions"));
		{
			MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);
			MenuBuilder->AddMenuEntry(FGenericCommands::Get().Delete);
		}
		MenuBuilder->EndSection();
	}

	// No Super call so Node comments option is not shown
}

const FPinConnectionResponse USoundSubmixGraphSchema::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	// Make sure the pins are not on the same node
	if (PinA->GetOwningNode() == PinB->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionSameNode", "Both are on the same node"));
	}

	// Compare the directions
	const UEdGraphPin* InputPin = NULL;
	const UEdGraphPin* OutputPin = NULL;

	if (!CategorizePinsByDirection(PinA, PinB, /*out*/ InputPin, /*out*/ OutputPin))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionIncompatible", "Directions are not compatible"));
	}

	if (ConnectionCausesLoop(InputPin, OutputPin))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionLoop", "Connection would cause loop"));
	}

	// Break existing connections on inputs only - multiple output connections are acceptable
	if (InputPin->LinkedTo.Num() > 0)
	{
		ECanCreateConnectionResponse ReplyBreakOutputs;
		if (InputPin == PinA)
		{
			ReplyBreakOutputs = CONNECT_RESPONSE_BREAK_OTHERS_A;
		}
		else
		{
			ReplyBreakOutputs = CONNECT_RESPONSE_BREAK_OTHERS_B;
		}
		return FPinConnectionResponse(ReplyBreakOutputs, LOCTEXT("ConnectionReplace", "Replace existing connections"));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, FText::GetEmpty());
}

bool USoundSubmixGraphSchema::TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	bool bModified = UEdGraphSchema::TryCreateConnection(PinA, PinB);

	if (bModified)
	{
		CastChecked<USoundSubmixGraph>(PinA->GetOwningNode()->GetGraph())->LinkSoundSubmixes();
	}

	return bModified;
}

bool USoundSubmixGraphSchema::ShouldHidePinDefaultValue(UEdGraphPin* Pin) const
{
	return true;
}

FLinearColor USoundSubmixGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FLinearColor::White;
}

void USoundSubmixGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
	Super::BreakNodeLinks(TargetNode);

	CastChecked<USoundSubmixGraph>(TargetNode.GetGraph())->LinkSoundSubmixes();
}

void USoundSubmixGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const
{
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GraphEd_BreakPinLinks", "Break Pin Links") );

	Super::BreakPinLinks(TargetPin, bSendsNodeNotifcation);
	
	// if this would notify the node then we need to re-link sound classes
	if (bSendsNodeNotifcation)
	{
		CastChecked<USoundSubmixGraph>(TargetPin.GetOwningNode()->GetGraph())->LinkSoundSubmixes();
	}
}

void USoundSubmixGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin)
{
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GraphEd_BreakSinglePinLink", "Break Pin Link") );
	Super::BreakSinglePinLink(SourcePin, TargetPin);

	CastChecked<USoundSubmixGraph>(SourcePin->GetOwningNode()->GetGraph())->LinkSoundSubmixes();
}

void USoundSubmixGraphSchema::DroppedAssetsOnGraph(const TArray<struct FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const
{
	USoundSubmixGraph* SoundSubmixGraph = CastChecked<USoundSubmixGraph>(Graph);

	TArray<USoundSubmix*> UndisplayedSubmixes;
	for (int32 AssetIdx = 0; AssetIdx < Assets.Num(); ++AssetIdx)
	{
		USoundSubmix* SoundSubmix = Cast<USoundSubmix>(Assets[AssetIdx].GetAsset());
		if (SoundSubmix && !SoundSubmixGraph->IsSubmixDisplayed(SoundSubmix))
		{
			UndisplayedSubmixes.Add(SoundSubmix);
		}
	}

	if (UndisplayedSubmixes.Num() > 0)
	{
		const FScopedTransaction Transaction( LOCTEXT("SoundSubmixEditorDropSubmixes", "Sound Submix Editor: Drag and Drop Sound Submix") );

		SoundSubmixGraph->AddDroppedSoundSubmixes(UndisplayedSubmixes, GraphPosition.X, GraphPosition.Y);
	}
}

#undef LOCTEXT_NAMESPACE

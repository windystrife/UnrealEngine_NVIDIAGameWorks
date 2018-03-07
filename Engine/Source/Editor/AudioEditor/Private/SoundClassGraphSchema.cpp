// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SoundClassGraph/SoundClassGraphSchema.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SoundClassGraph/SoundClassGraph.h"
#include "SoundClassGraph/SoundClassGraphNode.h"
#include "Sound/SoundClass.h"
#include "ScopedTransaction.h"
#include "GraphEditorActions.h"
#include "SoundClassEditorUtilities.h"
#include "Framework/Commands/GenericCommands.h"

#define LOCTEXT_NAMESPACE "SoundClassSchema"

UEdGraphNode* FSoundClassGraphSchemaAction_NewNode::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	FSoundClassEditorUtilities::CreateSoundClass(ParentGraph, FromPin, Location, NewSoundClassName);
	return NULL;
}

USoundClassGraphSchema::USoundClassGraphSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USoundClassGraphSchema::ConnectionCausesLoop(const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const
{
	USoundClassGraphNode* InputNode = CastChecked<USoundClassGraphNode>(InputPin->GetOwningNode());
	USoundClassGraphNode* OutputNode = CastChecked<USoundClassGraphNode>(OutputPin->GetOwningNode());

	return InputNode->SoundClass->RecurseCheckChild( OutputNode->SoundClass );
}

void USoundClassGraphSchema::GetBreakLinkToSubMenuActions( class FMenuBuilder& MenuBuilder, UEdGraphPin* InGraphPin )
{
	// Make sure we have a unique name for every entry in the list
	TMap< FString, uint32 > LinkTitleCount;

	// Add all the links we could break from
	for(TArray<class UEdGraphPin*>::TConstIterator Links(InGraphPin->LinkedTo); Links; ++Links)
	{
		UEdGraphPin* Pin = *Links;
		FString TitleString = Pin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString();
		FText Title = FText::FromString( TitleString );
		if ( Pin->PinName != TEXT("") )
		{
			TitleString = FString::Printf(TEXT("%s (%s)"), *TitleString, *Pin->PinName);

			// Add name of connection if possible
			FFormatNamedArguments Args;
			Args.Add( TEXT("NodeTitle"), Title );
			Args.Add( TEXT("PinName"), Pin->GetDisplayName() );
			Title = FText::Format( LOCTEXT("BreakDescPin", "{NodeTitle} ({PinName})"), Args );
		}

		uint32 &Count = LinkTitleCount.FindOrAdd( TitleString );

		FText Description;
		FFormatNamedArguments Args;
		Args.Add( TEXT("NodeTitle"), Title );
		Args.Add( TEXT("NumberOfNodes"), Count );

		if ( Count == 0 )
		{
			Description = FText::Format( LOCTEXT("BreakDesc", "Break link to {NodeTitle}"), Args );
		}
		else
		{
			Description = FText::Format( LOCTEXT("BreakDescMulti", "Break link to {NodeTitle} ({NumberOfNodes})"), Args );
		}
		++Count;

		MenuBuilder.AddMenuEntry( Description, Description, FSlateIcon(), FUIAction(
			FExecuteAction::CreateUObject((USoundClassGraphSchema*const)this, &USoundClassGraphSchema::BreakSinglePinLink, const_cast< UEdGraphPin* >(InGraphPin), *Links) ) );
	}
}

void USoundClassGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	const FText Name = LOCTEXT("NewSoundClass", "New Sound Class");
	const FText ToolTip = LOCTEXT("NewSoundClassTooltip", "Create a new sound class");
	
	TSharedPtr<FSoundClassGraphSchemaAction_NewNode> NewAction(new FSoundClassGraphSchemaAction_NewNode(FText::GetEmpty(), Name, ToolTip, 0));

	ContextMenuBuilder.AddAction( NewAction );
}

void USoundClassGraphSchema::GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, class FMenuBuilder* MenuBuilder, bool bIsDebugging) const
{
	if (InGraphPin)
	{
		MenuBuilder->BeginSection("SoundClassGraphSchemaPinActions", LOCTEXT("PinActionsMenuHeader", "Pin Actions"));
		{
			// Only display the 'Break Links' option if there is a link to break!
			if (InGraphPin->LinkedTo.Num() > 0)
			{
				MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().BreakPinLinks );

				// add sub menu for break link to
				if(InGraphPin->LinkedTo.Num() > 1)
				{
					MenuBuilder->AddSubMenu(
						LOCTEXT("BreakLinkTo", "Break Link To..." ),
						LOCTEXT("BreakSpecificLinks", "Break a specific link..." ),
						FNewMenuDelegate::CreateUObject( (USoundClassGraphSchema*const)this, &USoundClassGraphSchema::GetBreakLinkToSubMenuActions, const_cast<UEdGraphPin*>(InGraphPin)));
				}
				else
				{
					((USoundClassGraphSchema*const)this)->GetBreakLinkToSubMenuActions(*MenuBuilder, const_cast<UEdGraphPin*>(InGraphPin));
				}
			}
		}
		MenuBuilder->EndSection();
	}
	else if (InGraphNode)
	{
		const USoundClassGraphNode* SoundGraphNode = Cast<const USoundClassGraphNode>(InGraphNode);

		MenuBuilder->BeginSection("SoundClassGraphSchemaNodeActions", LOCTEXT("ClassActionsMenuHeader", "SoundClass Actions"));
		{
			MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);
			MenuBuilder->AddMenuEntry(FGenericCommands::Get().Delete);
		}
		MenuBuilder->EndSection();
	}

	// No Super call so Node comments option is not shown
}

const FPinConnectionResponse USoundClassGraphSchema::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
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

bool USoundClassGraphSchema::TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	bool bModified = UEdGraphSchema::TryCreateConnection(PinA, PinB);

	if (bModified)
	{
		CastChecked<USoundClassGraph>(PinA->GetOwningNode()->GetGraph())->LinkSoundClasses();
	}

	return bModified;
}

bool USoundClassGraphSchema::ShouldHidePinDefaultValue(UEdGraphPin* Pin) const
{
	return true;
}

FLinearColor USoundClassGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FLinearColor::White;
}

void USoundClassGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
	Super::BreakNodeLinks(TargetNode);

	CastChecked<USoundClassGraph>(TargetNode.GetGraph())->LinkSoundClasses();
}

void USoundClassGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const
{
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GraphEd_BreakPinLinks", "Break Pin Links") );

	Super::BreakPinLinks(TargetPin, bSendsNodeNotifcation);
	
	// if this would notify the node then we need to re-link sound classes
	if (bSendsNodeNotifcation)
	{
		CastChecked<USoundClassGraph>(TargetPin.GetOwningNode()->GetGraph())->LinkSoundClasses();
	}
}

void USoundClassGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin)
{
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GraphEd_BreakSinglePinLink", "Break Pin Link") );
	Super::BreakSinglePinLink(SourcePin, TargetPin);

	CastChecked<USoundClassGraph>(SourcePin->GetOwningNode()->GetGraph())->LinkSoundClasses();
}

void USoundClassGraphSchema::DroppedAssetsOnGraph(const TArray<struct FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const
{
	USoundClassGraph* SoundClassGraph = CastChecked<USoundClassGraph>(Graph);

	TArray<USoundClass*> UndisplayedClasses;
	for (int32 AssetIdx = 0; AssetIdx < Assets.Num(); ++AssetIdx)
	{
		USoundClass* SoundClass = Cast<USoundClass>(Assets[AssetIdx].GetAsset());
		if (SoundClass && !SoundClassGraph->IsClassDisplayed(SoundClass))
		{
			UndisplayedClasses.Add(SoundClass);
		}
	}

	if (UndisplayedClasses.Num() > 0)
	{
		const FScopedTransaction Transaction( LOCTEXT("SoundClassEditorDropClasses", "Sound Class Editor: Drag and Drop Sound Class") );

		SoundClassGraph->AddDroppedSoundClasses(UndisplayedClasses, GraphPosition.X, GraphPosition.Y);
	}
}

#undef LOCTEXT_NAMESPACE

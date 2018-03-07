// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialGraphSchema.cpp
=============================================================================*/

#include "MaterialGraph/MaterialGraphSchema.h"
#include "Misc/FeedbackContext.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyPortFlags.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunction.h"
#include "MaterialGraph/MaterialGraph.h"
#include "Engine/Texture.h"
#include "MaterialGraph/MaterialGraphNode_Base.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialGraph/MaterialGraphNode_Root.h"
#include "Materials/MaterialParameterCollection.h"

#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionReroute.h"

#include "ScopedTransaction.h"
#include "MaterialEditorUtilities.h"
#include "GraphEditorActions.h"
#include "AssetRegistryModule.h"
#include "MaterialEditorActions.h"
#include "MaterialGraphNode_Knot.h"

#define LOCTEXT_NAMESPACE "MaterialGraphSchema"

int32 UMaterialGraphSchema::CurrentCacheRefreshID = 0;

////////////////////////////////////////
// FMaterialGraphSchemaAction_NewNode //

UEdGraphNode* FMaterialGraphSchemaAction_NewNode::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	check(MaterialExpressionClass);

	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "MaterialEditorNewExpression", "Material Editor: New Expression"));

	UMaterialExpression* NewExpression = FMaterialEditorUtilities::CreateNewMaterialExpression(ParentGraph, MaterialExpressionClass, Location, bSelectNewNode, /*bAutoAssignResource*/true);

	if (NewExpression)
	{
		if (MaterialExpressionClass == UMaterialExpressionFunctionInput::StaticClass() && FromPin)
		{
			// Set this to be an input of the type we dragged from
			SetFunctionInputType(CastChecked<UMaterialExpressionFunctionInput>(NewExpression), UMaterialGraphSchema::GetMaterialValueType(FromPin));
		}

		NewExpression->GraphNode->AutowireNewNode(FromPin);

		return NewExpression->GraphNode;
	}

	return NULL;
}

void FMaterialGraphSchemaAction_NewNode::SetFunctionInputType(UMaterialExpressionFunctionInput* FunctionInput, uint32 MaterialValueType) const
{
	switch (MaterialValueType)
	{
	case MCT_Float:
	case MCT_Float1:
		FunctionInput->InputType = FunctionInput_Scalar;
		break;
	case MCT_Float2:
		FunctionInput->InputType = FunctionInput_Vector2;
		break;
	case MCT_Float3:
		FunctionInput->InputType = FunctionInput_Vector3;
		break;
	case MCT_Float4:
		FunctionInput->InputType = FunctionInput_Vector4;
		break;
	case MCT_Texture:
	case MCT_Texture2D:
		FunctionInput->InputType = FunctionInput_Texture2D;
		break;
	case MCT_TextureCube:
		FunctionInput->InputType = FunctionInput_TextureCube;
		break;
	case MCT_StaticBool:
		FunctionInput->InputType = FunctionInput_StaticBool;
		break;
	case MCT_MaterialAttributes:
		FunctionInput->InputType = FunctionInput_MaterialAttributes;
		break;
	default:
		break;
	}
}

////////////////////////////////////////////////
// FMaterialGraphSchemaAction_NewFunctionCall //

UEdGraphNode* FMaterialGraphSchemaAction_NewFunctionCall::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "MaterialEditorNewFunctionCall", "Material Editor: New Function Call"));

	UMaterialExpressionMaterialFunctionCall* FunctionNode = CastChecked<UMaterialExpressionMaterialFunctionCall>(
																FMaterialEditorUtilities::CreateNewMaterialExpression(
																	ParentGraph, UMaterialExpressionMaterialFunctionCall::StaticClass(), Location, bSelectNewNode, /*bAutoAssignResource*/false));

	if (!FunctionNode->MaterialFunction)
	{
		UMaterialFunction* MaterialFunction = LoadObject<UMaterialFunction>(NULL, *FunctionPath, NULL, 0, NULL);
		UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(ParentGraph);
		if(FunctionNode->SetMaterialFunction(MaterialFunction))
		{
			FunctionNode->PostEditChange();
			FMaterialEditorUtilities::UpdateSearchResults(ParentGraph);
			FunctionNode->GraphNode->AutowireNewNode(FromPin);
			return FunctionNode->GraphNode;
		}
		else
		{
			FMaterialEditorUtilities::AddToSelection(ParentGraph, FunctionNode);
			FMaterialEditorUtilities::DeleteSelectedNodes(ParentGraph);
		}
	}

	return NULL;
}

///////////////////////////////////////////
// FMaterialGraphSchemaAction_NewComment //

UEdGraphNode* FMaterialGraphSchemaAction_NewComment::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "MaterialEditorNewComment", "Material Editor: New Comment") );

	UMaterialExpressionComment* NewComment = FMaterialEditorUtilities::CreateNewMaterialExpressionComment(ParentGraph, Location);

	if (NewComment)
	{
		return NewComment->GraphNode;
	}

	return NULL;
}

//////////////////////////////////////
// FMaterialGraphSchemaAction_Paste //

UEdGraphNode* FMaterialGraphSchemaAction_Paste::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	FMaterialEditorUtilities::PasteNodesHere(ParentGraph, Location);
	return NULL;
}

//////////////////////////
// UMaterialGraphSchema //

UMaterialGraphSchema::UMaterialGraphSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PC_Mask = TEXT("mask");
	PC_Required = TEXT("required");
	PC_Optional = TEXT("optional");
	PC_MaterialInput = TEXT("materialinput");

	PSC_Red = TEXT("red");
	PSC_Green = TEXT("green");
	PSC_Blue = TEXT("blue");
	PSC_Alpha = TEXT("alpha");

	ActivePinColor = FLinearColor::White;
	InactivePinColor = FLinearColor(0.05f, 0.05f, 0.05f);
	AlphaPinColor = FLinearColor(0.5f, 0.5f, 0.5f);
}

void UMaterialGraphSchema::SelectAllInputNodes(UEdGraph* Graph, UEdGraphPin* InGraphPin)
{
	TArray<UEdGraphPin*> AllPins = InGraphPin->LinkedTo;

	if (AllPins.Num() == 0)
	{
		return;
	}

	for (UEdGraphPin* Pin : AllPins)
	{
		UMaterialGraphNode* MaterialNode = Cast<UMaterialGraphNode>(Pin->GetOwningNode());
		FMaterialEditorUtilities::AddToSelection(Graph, MaterialNode->MaterialExpression);

		TArray<UEdGraphPin*> LinkedPins = Pin->GetOwningNode()->GetAllPins();
		for (UEdGraphPin* InputPin : LinkedPins)
		{
			if (InputPin->Direction == EEdGraphPinDirection::EGPD_Output)
			{
				continue;
			}
			else
			{
				SelectAllInputNodes(Graph, InputPin);
			}
		}
	}
}

void UMaterialGraphSchema::GetBreakLinkToSubMenuActions( class FMenuBuilder& MenuBuilder, UEdGraphPin* InGraphPin )
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

		uint32 &Count = LinkTitleCount.FindOrAdd(TitleString);

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
			FExecuteAction::CreateUObject((UMaterialGraphSchema*const)this, &UMaterialGraphSchema::BreakSinglePinLink, const_cast< UEdGraphPin* >(InGraphPin), *Links) ) );
	}
}

void UMaterialGraphSchema::OnConnectToFunctionOutput(UEdGraphPin* InGraphPin, UEdGraphPin* InFuncPin)
{
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GraphEd_CreateConnection", "Create Pin Link") );

	TryCreateConnection(InGraphPin, InFuncPin);
}

void UMaterialGraphSchema::OnConnectToMaterial(class UEdGraphPin* InGraphPin, int32 ConnIndex)
{
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GraphEd_CreateConnection", "Create Pin Link") );

	UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(InGraphPin->GetOwningNode()->GetGraph());

	TryCreateConnection(InGraphPin, MaterialGraph->RootNode->GetInputPin(ConnIndex));
}

void UMaterialGraphSchema::GetPaletteActions(FGraphActionMenuBuilder& ActionMenuBuilder, const FString& CategoryName, bool bMaterialFunction) const
{
	if (CategoryName != TEXT("Functions"))
	{
		FMaterialEditorUtilities::GetMaterialExpressionActions(ActionMenuBuilder, bMaterialFunction);
		GetCommentAction(ActionMenuBuilder);
	}
	if (CategoryName != TEXT("Expressions"))
	{
		GetMaterialFunctionActions(ActionMenuBuilder);
	}
}

bool UMaterialGraphSchema::ConnectionCausesLoop(const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const
{
	// Only nodes representing Expressions have outputs
	UMaterialGraphNode* OutputNode = CastChecked<UMaterialGraphNode>(OutputPin->GetOwningNode());

	TArray<UMaterialExpression*> InputExpressions;
	OutputNode->MaterialExpression->GetAllInputExpressions(InputExpressions);

	if (UMaterialGraphNode* InputNode = Cast<UMaterialGraphNode>(InputPin->GetOwningNode()))
	{
		return InputExpressions.Contains(InputNode->MaterialExpression);
	}

	// Simple connection to root node
	return false;
}

bool UMaterialGraphSchema::ArePinsCompatible(const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin, FText& ResponseMessage) const
{
	uint32 InputType = GetMaterialValueType(InputPin);
	uint32 OutputType = GetMaterialValueType(OutputPin);

	bool bPinsCompatible = CanConnectMaterialValueTypes(InputType, OutputType);
	if (!bPinsCompatible)
	{
		TArray<FText> InputDescriptions;
		TArray<FText> OutputDescriptions;
		GetMaterialValueTypeDescriptions(InputType, InputDescriptions);
		GetMaterialValueTypeDescriptions(OutputType, OutputDescriptions);

		FString CombinedInputDescription;
		FString CombinedOutputDescription;
		for (int32 Index = 0; Index < InputDescriptions.Num(); ++Index)
		{
			if ( CombinedInputDescription.Len() > 0 )
			{
				CombinedInputDescription += TEXT(", ");
			}
			CombinedInputDescription += InputDescriptions[Index].ToString();
		}
		for (int32 Index = 0; Index < OutputDescriptions.Num(); ++Index)
		{
			if ( CombinedOutputDescription.Len() > 0 )
			{
				CombinedOutputDescription += TEXT(", ");
			}
			CombinedOutputDescription += OutputDescriptions[Index].ToString();
		}

		FFormatNamedArguments Args;
		Args.Add( TEXT("InputType"), FText::FromString(CombinedInputDescription) );
		Args.Add( TEXT("OutputType"), FText::FromString(CombinedOutputDescription) );
		ResponseMessage = FText::Format( LOCTEXT("IncompatibleDesc", "{OutputType} is not compatible with {InputType}"), Args );
	}

	return bPinsCompatible;
}

uint32 UMaterialGraphSchema::GetMaterialValueType(const UEdGraphPin* MaterialPin)
{
	if (MaterialPin->Direction == EGPD_Output)
	{
		UMaterialGraphNode* OwningNode = CastChecked<UMaterialGraphNode>(MaterialPin->GetOwningNode());
		return OwningNode->GetOutputType(MaterialPin);
	}
	else
	{
		UMaterialGraphNode_Base* OwningNode = CastChecked<UMaterialGraphNode_Base>(MaterialPin->GetOwningNode());
		return OwningNode->GetInputType(MaterialPin);
	}
}

void UMaterialGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	const UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(ContextMenuBuilder.CurrentGraph);

	// Run through all nodes and add any menu items they want to add
	Super::GetGraphContextActions(ContextMenuBuilder);

	// Get the Context Actions from Material Editor Module
	FMaterialEditorUtilities::GetMaterialExpressionActions(ContextMenuBuilder, MaterialGraph->MaterialFunction != NULL);

	// Get the Material Functions as well
	GetMaterialFunctionActions(ContextMenuBuilder);

	GetCommentAction(ContextMenuBuilder, ContextMenuBuilder.CurrentGraph);

	// Add Paste here if appropriate
	if (!ContextMenuBuilder.FromPin && FMaterialEditorUtilities::CanPasteNodes(ContextMenuBuilder.CurrentGraph))
	{
		const FText PasteDesc = LOCTEXT("PasteDesc", "Paste Here");
		const FText PasteToolTip = LOCTEXT("PasteToolTip", "Pastes copied items at this location.");
		TSharedPtr<FMaterialGraphSchemaAction_Paste> PasteAction(new FMaterialGraphSchemaAction_Paste(FText::GetEmpty(), PasteDesc, PasteToolTip, 0));
		ContextMenuBuilder.AddAction(PasteAction);
	}
}

void UMaterialGraphSchema::GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, class FMenuBuilder* MenuBuilder, bool bIsDebugging) const
{
	if (InGraphPin)
	{
		const UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(CurrentGraph);
		MenuBuilder->BeginSection("MaterialGraphSchemaPinActions", LOCTEXT("PinActionsMenuHeader", "Pin Actions"));
		{
			// Only display the 'Break Link' option if there is a link to break!
			if (InGraphPin->LinkedTo.Num() > 0)
			{
				if(InGraphPin->Direction == EEdGraphPinDirection::EGPD_Input)
				{
					MenuBuilder->AddMenuEntry(
						LOCTEXT("SelectLinkedNodes", "Select Linked Nodes"),
						LOCTEXT("SelectLinkedNodesTooltip", "Adds all input Nodes linked to this Pin to selection"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateUObject((UMaterialGraphSchema*const)this, &UMaterialGraphSchema::SelectAllInputNodes, const_cast<UEdGraph*>(CurrentGraph), const_cast<UEdGraphPin*>(InGraphPin)))
						);
				}

				MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().BreakPinLinks);

				// add sub menu for break link to
				if(InGraphPin->LinkedTo.Num() > 1)
				{
					MenuBuilder->AddSubMenu(
						LOCTEXT("BreakLinkTo", "Break Link To..." ),
						LOCTEXT("BreakSpecificLinks", "Break a specific link..." ),
						FNewMenuDelegate::CreateUObject( (UMaterialGraphSchema*const)this, &UMaterialGraphSchema::GetBreakLinkToSubMenuActions, const_cast<UEdGraphPin*>(InGraphPin)));
				}
				else
				{
					((UMaterialGraphSchema*const)this)->GetBreakLinkToSubMenuActions(*MenuBuilder, const_cast<UEdGraphPin*>(InGraphPin));
				}
			}

			// Only display Promote to Parameters on input pins
			if (InGraphPin->Direction == EEdGraphPinDirection::EGPD_Input)
			{
				MenuBuilder->AddMenuEntry(FMaterialEditorCommands::Get().PromoteToParameter);
			}
		}
		MenuBuilder->EndSection();

		// add menu items to expression output for material connection
		if ( InGraphPin->Direction == EGPD_Output )
		{
			MenuBuilder->BeginSection("MaterialEditorMenuConnector2");
			{
				// If we are editing a material function, display options to connect to function outputs
				if (MaterialGraph->MaterialFunction)
				{
					for (int32 Index = 0; Index < MaterialGraph->Nodes.Num(); Index++)
					{
						UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(MaterialGraph->Nodes[Index]);
						if (GraphNode)
						{
							UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(GraphNode->MaterialExpression);
							if (FunctionOutput)
							{
								FFormatNamedArguments Arguments;
								Arguments.Add(TEXT("Name"), FText::FromString( *FunctionOutput->OutputName ));
								const FText Label = FText::Format( LOCTEXT( "ConnectToFunction", "Connect To {Name}" ), Arguments );
								const FText ToolTip = FText::Format( LOCTEXT( "ConnectToFunctionTooltip", "Connects to the function output {Name}" ), Arguments );
								MenuBuilder->AddMenuEntry(Label,
									ToolTip,
									FSlateIcon(),
									FUIAction(FExecuteAction::CreateUObject((UMaterialGraphSchema*const)this, &UMaterialGraphSchema::OnConnectToFunctionOutput, const_cast< UEdGraphPin* >(InGraphPin), GraphNode->GetInputPin(0))));
							}
						}
					}
				}
				else
				{
					for (int32 Index = 0; Index < MaterialGraph->MaterialInputs.Num(); ++Index)
					{
						if(MaterialGraph->MaterialInputs[Index].IsVisiblePin(MaterialGraph->Material))
						{
							FFormatNamedArguments Arguments;
							Arguments.Add(TEXT("Name"), MaterialGraph->MaterialInputs[Index].GetName());
							const FText Label = FText::Format( LOCTEXT( "ConnectToInput", "Connect To {Name}" ), Arguments );
							const FText ToolTip = FText::Format( LOCTEXT( "ConnectToInputTooltip", "Connects to the material input {Name}" ), Arguments );
							MenuBuilder->AddMenuEntry(Label,
								ToolTip,
								FSlateIcon(),
								FUIAction(FExecuteAction::CreateUObject((UMaterialGraphSchema*const)this, &UMaterialGraphSchema::OnConnectToMaterial, const_cast< UEdGraphPin* >(InGraphPin), Index)));
						}
					}
				}
			}
			MenuBuilder->EndSection(); //MaterialEditorMenuConnector2
		}
	}
	else if (InGraphNode)
	{
		//Moved all functionality to relevant node classes
	}

	Super::GetContextMenuActions(CurrentGraph, InGraphNode, InGraphPin, MenuBuilder, bIsDebugging);
}

static TAutoConsoleVariable<int32> CVarPreventInvalidMaterialConnections(
	TEXT("r.PreventInvalidMaterialConnections"),
	1,
	TEXT("Controls whether users can make connections in the material editor if the system\n")
	TEXT("determines that they may cause compile errors\n")
	TEXT("0: Allow all connections\n")
	TEXT("1: Prevent invalid connections"),
	ECVF_Cheat);

const FPinConnectionResponse UMaterialGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
	bool bPreventInvalidConnections = CVarPreventInvalidMaterialConnections.GetValueOnGameThread() != 0;

	// Make sure the pins are not on the same node
	if (A->GetOwningNode() == B->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionSameNode", "Both are on the same node"));
	}

	// Compare the directions
	const UEdGraphPin* InputPin = NULL;
	const UEdGraphPin* OutputPin = NULL;

	if (!CategorizePinsByDirection(A, B, /*out*/ InputPin, /*out*/ OutputPin))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionIncompatible", "Directions are not compatible"));
	}

	// Check for new and existing loops
	FText ResponseMessage;
	if (ConnectionCausesLoop(InputPin, OutputPin))
	{
		ResponseMessage = LOCTEXT("ConnectionLoop", "Connection could cause loop");
		// TODO: re-enable this if loops are going to be removed completely
		//return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionLoop", "Connection would cause loop").ToString());
	}

	// Check for incompatible pins and get description if they cannot connect
	if (!ArePinsCompatible(InputPin, OutputPin, ResponseMessage) && bPreventInvalidConnections)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, ResponseMessage);
	}

	// Break existing connections on inputs only - multiple output connections are acceptable
	if (InputPin->LinkedTo.Num() > 0)
	{
		ECanCreateConnectionResponse ReplyBreakOutputs;
		if (InputPin == A)
		{
			ReplyBreakOutputs = CONNECT_RESPONSE_BREAK_OTHERS_A;
		}
		else
		{
			ReplyBreakOutputs = CONNECT_RESPONSE_BREAK_OTHERS_B;
		}
		if (ResponseMessage.IsEmpty())
		{
			ResponseMessage = LOCTEXT("ConnectionReplace", "Replace existing connections");
		}
		return FPinConnectionResponse(ReplyBreakOutputs, ResponseMessage);
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, ResponseMessage);
}

bool UMaterialGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
	bool bModified = UEdGraphSchema::TryCreateConnection(A, B);

	if (bModified)
	{
		FMaterialEditorUtilities::UpdateMaterialAfterGraphChange(A->GetOwningNode()->GetGraph());
	}

	return bModified;
}

FLinearColor UMaterialGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	if (PinType.PinCategory == PC_Mask)
	{
		if (PinType.PinSubCategory == PSC_Red)
		{
			return FLinearColor::Red;
		}
		else if (PinType.PinSubCategory == PSC_Green)
		{
			return FLinearColor::Green;
		}
		else if (PinType.PinSubCategory == PSC_Blue)
		{
			return FLinearColor::Blue;
		}
		else if (PinType.PinSubCategory == PSC_Alpha)
		{
			return AlphaPinColor;
		}
	}
	else if (PinType.PinCategory == PC_Required)
	{
		return ActivePinColor;
	}
	else if (PinType.PinCategory == PC_Optional)
	{
		return InactivePinColor;
	}

	return ActivePinColor;
}

void UMaterialGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
	bool bHasLinksToBreak = false;
	for (auto PinIt = TargetNode.Pins.CreateConstIterator(); PinIt; ++PinIt)
	{
		UEdGraphPin* Pin = *PinIt;
		for (auto LinkIt = Pin->LinkedTo.CreateConstIterator(); LinkIt; ++LinkIt)
		{
			if (*LinkIt)
			{
				bHasLinksToBreak = true;
			}
		}
	}

	Super::BreakNodeLinks(TargetNode);

	if (bHasLinksToBreak)
	{
		FMaterialEditorUtilities::UpdateMaterialAfterGraphChange(TargetNode.GetGraph());
	}
}

void UMaterialGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const
{
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GraphEd_BreakPinLinks", "Break Pin Links") );

	bool bHasLinksToBreak = false;
	for (auto LinkIt = TargetPin.LinkedTo.CreateConstIterator(); LinkIt; ++LinkIt)
	{
		if (*LinkIt)
		{
			bHasLinksToBreak = true;
		}
	}

	Super::BreakPinLinks(TargetPin, bSendsNodeNotifcation);

	// if this would notify the node then we need to re-compile material
	if (bSendsNodeNotifcation && bHasLinksToBreak)
	{
		FMaterialEditorUtilities::UpdateMaterialAfterGraphChange(TargetPin.GetOwningNode()->GetGraph());
	}
}

void UMaterialGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin)
{
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GraphEd_BreakSinglePinLink", "Break Pin Link") );

	bool bHasLinkToBreak = false;
	for (auto LinkIt = SourcePin->LinkedTo.CreateConstIterator(); LinkIt; ++LinkIt)
	{
		if (*LinkIt == TargetPin)
		{
			bHasLinkToBreak = true;
		}
	}

	Super::BreakSinglePinLink(SourcePin, TargetPin);

	if (bHasLinkToBreak)
	{
		FMaterialEditorUtilities::UpdateMaterialAfterGraphChange(SourcePin->GetOwningNode()->GetGraph());
	}
}

void UMaterialGraphSchema::DroppedAssetsOnGraph(const TArray<struct FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const
{
	UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(Graph);
	const int32 LocOffsetBetweenNodes = 32;

	FVector2D ExpressionPosition = GraphPosition;

	for (int32 AssetIdx = 0; AssetIdx < Assets.Num(); ++AssetIdx)
	{
		bool bAddedNode = false;
		UObject* Asset = Assets[AssetIdx].GetAsset();
		UClass* MaterialExpressionClass = Cast<UClass>(Asset);
		UMaterialFunction* Func = Cast<UMaterialFunction>(Asset);
		UTexture* Tex = Cast<UTexture>(Asset);
		UMaterialParameterCollection* ParameterCollection = Cast<UMaterialParameterCollection>(Asset);
		
		if (MaterialExpressionClass && MaterialExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
		{
			FMaterialEditorUtilities::CreateNewMaterialExpression(Graph, MaterialExpressionClass, ExpressionPosition, true, true);
			bAddedNode = true;
		}
		else if ( Func )
		{
			UMaterialExpressionMaterialFunctionCall* FunctionNode = CastChecked<UMaterialExpressionMaterialFunctionCall>(
				FMaterialEditorUtilities::CreateNewMaterialExpression(Graph, UMaterialExpressionMaterialFunctionCall::StaticClass(), ExpressionPosition, true, false));

			if (!FunctionNode->MaterialFunction)
			{
				if(FunctionNode->SetMaterialFunction(Func))
				{
					FunctionNode->PostEditChange();
					FMaterialEditorUtilities::UpdateSearchResults(Graph);
				}
				else
				{
					FMaterialEditorUtilities::AddToSelection(Graph, FunctionNode);
					FMaterialEditorUtilities::DeleteSelectedNodes(Graph);

					continue;
				}
			}

			bAddedNode = true;
		}
		else if ( Tex )
		{
			UMaterialExpressionTextureSample* TextureSampleNode = CastChecked<UMaterialExpressionTextureSample>(
				FMaterialEditorUtilities::CreateNewMaterialExpression(Graph, UMaterialExpressionTextureSample::StaticClass(), ExpressionPosition, true, true) );
			TextureSampleNode->Texture = Tex;
			TextureSampleNode->AutoSetSampleType();

			FMaterialEditorUtilities::ForceRefreshExpressionPreviews(Graph);

			bAddedNode = true;
		}
		else if ( ParameterCollection )
		{
			UMaterialExpressionCollectionParameter* CollectionParameterNode = CastChecked<UMaterialExpressionCollectionParameter>(
				FMaterialEditorUtilities::CreateNewMaterialExpression(Graph, UMaterialExpressionCollectionParameter::StaticClass(), ExpressionPosition, true, true) );
			CollectionParameterNode->Collection = ParameterCollection;

			FMaterialEditorUtilities::ForceRefreshExpressionPreviews(Graph);

			bAddedNode = true;
		}

		if ( bAddedNode )
		{
			ExpressionPosition.X += LocOffsetBetweenNodes;
			ExpressionPosition.Y += LocOffsetBetweenNodes;
		}
	}
}

int32 UMaterialGraphSchema::GetNodeSelectionCount(const UEdGraph* Graph) const
{
	return FMaterialEditorUtilities::GetNumberOfSelectedNodes(Graph);
}

TSharedPtr<FEdGraphSchemaAction> UMaterialGraphSchema::GetCreateCommentAction() const
{
	return TSharedPtr<FEdGraphSchemaAction>(static_cast<FEdGraphSchemaAction*>(new FMaterialGraphSchemaAction_NewComment));
}

void UMaterialGraphSchema::GetMaterialFunctionActions(FGraphActionMenuBuilder& ActionMenuBuilder) const
{
	// Get type of dragged pin
	uint32 FromPinType = 0;
	if (ActionMenuBuilder.FromPin)
	{
		FromPinType = GetMaterialValueType(ActionMenuBuilder.FromPin);
	}

	// Load the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Collect a full list of assets with the specified class
	TArray<FAssetData> AssetDataList;
	AssetRegistryModule.Get().GetAssetsByClass(UMaterialFunction::StaticClass()->GetFName(), AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		const bool bExposeToLibrary = AssetData.GetTagValueRef<bool>("bExposeToLibrary");

		// If this was a function that was selected to be exposed to the library
		if ( bExposeToLibrary )
		{
			if(AssetData.IsAssetLoaded())
			{
				if(AssetData.GetAsset()->GetOutermost() == GetTransientPackage())
				{
					continue;
				}
			}

			if (!ActionMenuBuilder.FromPin || HasCompatibleConnection(AssetData, FromPinType, ActionMenuBuilder.FromPin->Direction))
			{
				// Gather the relevant information from the asset data
				const FString FunctionPathName = AssetData.ObjectPath.ToString();
				const FText Description = AssetData.GetTagValueRef<FText>("Description");
				TArray<FString> LibraryCategories;
				{
					const FString LibraryCategoriesString = AssetData.GetTagValueRef<FString>("LibraryCategories");
					if ( !LibraryCategoriesString.IsEmpty() )
					{
						if (UArrayProperty* LibraryCategoriesProperty = FindFieldChecked<UArrayProperty>(UMaterialFunction::StaticClass(), TEXT("LibraryCategories")))
						{
							uint8* DestAddr = (uint8*)(&LibraryCategories);
							LibraryCategoriesProperty->ImportText(*LibraryCategoriesString, DestAddr, PPF_None, NULL, GWarn);
						}
					}
				}
				TArray<FText> LibraryCategoriesText;
				{
					const FString LibraryCategoriesString = AssetData.GetTagValueRef<FString>("LibraryCategoriesText");
					if ( !LibraryCategoriesString.IsEmpty() )
					{
						UArrayProperty* LibraryCategoriesProperty = FindFieldChecked<UArrayProperty>(UMaterialFunction::StaticClass(), GET_MEMBER_NAME_CHECKED(UMaterialFunction, LibraryCategoriesText));
						uint8* DestAddr = (uint8*)(&LibraryCategoriesText);
						LibraryCategoriesProperty->ImportText(*LibraryCategoriesString, DestAddr, PPF_None, NULL, GWarn);
					}

					for (const FString& Category : LibraryCategories)
					{
						if (!LibraryCategoriesText.ContainsByPredicate(
							[&Category](const FText& Text)
							{
								return Text.ToString() == Category;
							}
							))
						{
							LibraryCategoriesText.Add(FText::FromString(Category));
						}
					}

					if ( LibraryCategoriesText.Num() == 0 )
					{
						LibraryCategoriesText.Add( LOCTEXT("UncategorizedMaterialFunction", "Uncategorized") );
					}
				}

				// Extract the object name from the path
				FString FunctionName = FunctionPathName;
				int32 PeriodIndex = FunctionPathName.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);

				if (PeriodIndex != INDEX_NONE)
				{
					FunctionName = FunctionPathName.Right(FunctionPathName.Len() - PeriodIndex - 1);
				}

				// For each category the function should belong to...
				for (const FText& CategoryName : LibraryCategoriesText)
				{
					TSharedPtr<FMaterialGraphSchemaAction_NewFunctionCall> NewFunctionAction(new FMaterialGraphSchemaAction_NewFunctionCall(
						CategoryName,
						FText::FromString(FunctionName),
						Description, 0));
					ActionMenuBuilder.AddAction(NewFunctionAction);
					NewFunctionAction->FunctionPath = FunctionPathName;
				}
			}
		}
	}
}

void UMaterialGraphSchema::GetCommentAction(FGraphActionMenuBuilder& ActionMenuBuilder, const UEdGraph* CurrentGraph) const
{
	if (!ActionMenuBuilder.FromPin)
	{
		const bool bIsManyNodesSelected = CurrentGraph ? (FMaterialEditorUtilities::GetNumberOfSelectedNodes(CurrentGraph) > 0) : false;
		const FText CommentDesc = LOCTEXT("CommentDesc", "New Comment");
		const FText MultiCommentDesc = LOCTEXT("MultiCommentDesc", "Create Comment from Selection");
		const FText CommentToolTip = LOCTEXT("CommentToolTip", "Creates a comment.");
		const FText MenuDescription = bIsManyNodesSelected ? MultiCommentDesc : CommentDesc;
		TSharedPtr<FMaterialGraphSchemaAction_NewComment> NewAction(new FMaterialGraphSchemaAction_NewComment(FText::GetEmpty(), MenuDescription, CommentToolTip, 0));
		ActionMenuBuilder.AddAction( NewAction );
	}
}

bool UMaterialGraphSchema::HasCompatibleConnection(const FAssetData& FunctionAssetData, uint32 TestType, EEdGraphPinDirection TestDirection) const
{
	if (TestType != 0)
	{
		uint32 CombinedInputTypes = FunctionAssetData.GetTagValueRef<uint32>(GET_MEMBER_NAME_CHECKED(UMaterialFunction, CombinedInputTypes));
		uint32 CombinedOutputTypes = FunctionAssetData.GetTagValueRef<uint32>(GET_MEMBER_NAME_CHECKED(UMaterialFunction, CombinedOutputTypes));

		if (CombinedOutputTypes == 0)
		{
			// Need to load function to build combined output types
			UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(FunctionAssetData.GetAsset());
			if (MaterialFunction != nullptr)
			{
				CombinedInputTypes = MaterialFunction->CombinedInputTypes;
				CombinedOutputTypes = MaterialFunction->CombinedOutputTypes;
			}
		}

		if (TestDirection == EGPD_Output)
		{
			if (CanConnectMaterialValueTypes(CombinedInputTypes, TestType))
			{
				return true;
			}
		}
		else
		{
			if (CanConnectMaterialValueTypes(TestType, CombinedOutputTypes))
			{
				return true;
			}
		}
	}

	return false;
}

bool UMaterialGraphSchema::IsCacheVisualizationOutOfDate(int32 InVisualizationCacheID) const
{
	return CurrentCacheRefreshID != InVisualizationCacheID;
}

int32 UMaterialGraphSchema::GetCurrentVisualizationCacheID() const
{
	return CurrentCacheRefreshID;
}

void UMaterialGraphSchema::ForceVisualizationCacheClear() const
{
	++CurrentCacheRefreshID;
}

void UMaterialGraphSchema::OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const
{
	const FScopedTransaction Transaction(LOCTEXT("CreateRerouteNodeOnWire", "Create Reroute Node"));

	//@TODO: This constant is duplicated from inside of SGraphNodeKnot
	const FVector2D NodeSpacerSize(42.0f, 24.0f);
	const FVector2D KnotTopLeft = GraphPosition - (NodeSpacerSize * 0.5f);

	// Create a new knot
	UEdGraph* ParentGraph = PinA->GetOwningNode()->GetGraph();

	{
		UMaterialExpression* Expression = FMaterialEditorUtilities::CreateNewMaterialExpression(ParentGraph, UMaterialExpressionReroute::StaticClass(), KnotTopLeft, true, true);

		// Move the connections across (only notifying the knot, as the other two didn't really change)
		PinA->BreakLinkTo(PinB);
		PinA->MakeLinkTo((PinA->Direction == EGPD_Output) ? CastChecked<UMaterialGraphNode_Knot>(Expression->GraphNode)->GetInputPin() : CastChecked<UMaterialGraphNode_Knot>(Expression->GraphNode)->GetOutputPin());
		PinB->MakeLinkTo((PinB->Direction == EGPD_Output) ? CastChecked<UMaterialGraphNode_Knot>(Expression->GraphNode)->GetInputPin() : CastChecked<UMaterialGraphNode_Knot>(Expression->GraphNode)->GetOutputPin());
		FMaterialEditorUtilities::UpdateMaterialAfterGraphChange(ParentGraph);
	}
}

bool UMaterialGraphSchema::SafeDeleteNodeFromGraph(UEdGraph* Graph, UEdGraphNode* NodeToDelete) const
{
	if (NodeToDelete == nullptr || Graph == nullptr || NodeToDelete->GetGraph() != Graph)
	{
		return false;
	}

	TArray<UEdGraphNode*> NodesToDelete;
	NodesToDelete.Add(NodeToDelete);
	FMaterialEditorUtilities::DeleteNodes(Graph, NodesToDelete);
	return true;
}

#undef LOCTEXT_NAMESPACE

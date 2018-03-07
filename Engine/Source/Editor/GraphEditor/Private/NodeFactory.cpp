// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "NodeFactory.h"
#include "UObject/Class.h"
#include "InputCoreTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "MaterialGraph/MaterialGraphNode_Comment.h"
#include "MaterialGraph/MaterialGraphNode_Base.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialGraph/MaterialGraphNode_Root.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "SoundCueGraph/SoundCueGraphNode.h"
#include "SoundCueGraph/SoundCueGraphNode_Root.h"
#include "SoundCueGraph/SoundCueGraphSchema.h"
#include "Engine/CollisionProfile.h"
#include "SGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"
#include "K2Node_Event.h"
#include "K2Node_AddPinInterface.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_CallMaterialParameterCollectionFunction.h"
#include "K2Node_Composite.h"
#include "K2Node_Copy.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_FormatText.h"
#include "K2Node_GetArrayItem.h"
#include "K2Node_Knot.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_SpawnActor.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_Switch.h"
#include "K2Node_Timeline.h"


#include "SGraphNodeDefault.h"
#include "SGraphNodeComment.h"
#include "SGraphNodeDocumentation.h"
#include "EdGraph/EdGraphNode_Documentation.h"
#include "SGraphNodeKnot.h"

#include "KismetNodes/SGraphNodeK2Default.h"
#include "KismetNodes/SGraphNodeK2Var.h"
#include "KismetNodes/SGraphNodeK2Composite.h"
#include "KismetNodes/SGraphNodeSwitchStatement.h"
#include "KismetNodes/SGraphNodeK2Sequence.h"
#include "KismetNodes/SGraphNodeK2Timeline.h"
#include "KismetNodes/SGraphNodeSpawnActor.h"
#include "KismetNodes/SGraphNodeSpawnActorFromClass.h"
#include "KismetNodes/SGraphNodeK2CreateDelegate.h"
#include "KismetNodes/SGraphNodeCallParameterCollectionFunction.h"
#include "KismetNodes/SGraphNodeK2Event.h"
#include "KismetNodes/SGraphNodeFormatText.h"
#include "KismetNodes/SGraphNodeMakeStruct.h"
#include "KismetNodes/SGraphNodeK2Copy.h"

#include "KismetPins/SGraphPinBool.h"
#include "SGraphPinString.h"
#include "KismetPins/SGraphPinText.h"
#include "SGraphPinObject.h"
#include "KismetPins/SGraphPinClass.h"
#include "KismetPins/SGraphPinExec.h"
#include "SGraphPinNum.h"
#include "SGraphPinInteger.h"
#include "SGraphPinColor.h"
#include "KismetPins/SGraphPinEnum.h"
#include "KismetPins/SGraphPinKey.h"
#include "SGraphPinVector.h"
#include "SGraphPinVector2D.h"
#include "SGraphPinVector4.h"
#include "KismetPins/SGraphPinIndex.h"
#include "KismetPins/SGraphPinCollisionProfile.h"

#include "MaterialNodes/SGraphNodeMaterialBase.h"
#include "MaterialNodes/SGraphNodeMaterialComment.h"
#include "MaterialNodes/SGraphNodeMaterialResult.h"
#include "MaterialGraphNode_Knot.h"

#include "MaterialPins/SGraphPinMaterialInput.h"

#include "ConnectionDrawingPolicy.h"
#include "BlueprintConnectionDrawingPolicy.h"
#include "MaterialGraphConnectionDrawingPolicy.h"

#include "EdGraphUtilities.h"
TSharedPtr<SGraphNode> FNodeFactory::CreateNodeWidget(UEdGraphNode* InNode)
{
	check(InNode != NULL);

	// First give a shot to the node itself
	{
		TSharedPtr<SGraphNode> NodeCreatedResult = InNode->CreateVisualWidget();
		if (NodeCreatedResult.IsValid())
		{
			return NodeCreatedResult;
		}
	}

	// First give a shot to the registered node factories
	for (auto FactoryIt = FEdGraphUtilities::VisualNodeFactories.CreateIterator(); FactoryIt; ++FactoryIt)
	{
		TSharedPtr<FGraphPanelNodeFactory> FactoryPtr = *FactoryIt;
		if (FactoryPtr.IsValid())
		{
			TSharedPtr<SGraphNode> ResultVisualNode = FactoryPtr->CreateNode(InNode);
			if (ResultVisualNode.IsValid())
			{
				return ResultVisualNode;
			}
		}
	}

	if (UMaterialGraphNode_Base* BaseMaterialNode = Cast<UMaterialGraphNode_Base>(InNode))
	{
		if (UMaterialGraphNode_Root* RootMaterialNode = Cast<UMaterialGraphNode_Root>(InNode))
		{
			return SNew(SGraphNodeMaterialResult, RootMaterialNode);
		}
		else if (UMaterialGraphNode_Knot* MaterialKnot = Cast<UMaterialGraphNode_Knot>(InNode))
		{
			return SNew(SGraphNodeKnot, MaterialKnot);
		}
		else if (UMaterialGraphNode* MaterialNode = Cast<UMaterialGraphNode>(InNode))
		{
			return SNew(SGraphNodeMaterialBase, MaterialNode);
		}
	}

	if (UK2Node* K2Node = Cast<UK2Node>(InNode))
	{
		if (UK2Node_Composite* CompositeNode = Cast<UK2Node_Composite>(InNode))
		{
			return SNew(SGraphNodeK2Composite, CompositeNode);
		}
		else if (K2Node->DrawNodeAsVariable())
		{
			return SNew(SGraphNodeK2Var, K2Node);
		}
		else if (UK2Node_Switch* SwitchNode = Cast<UK2Node_Switch>(InNode))
		{
			return SNew(SGraphNodeSwitchStatement, SwitchNode);
		}
		else if (InNode->GetClass()->ImplementsInterface(UK2Node_AddPinInterface::StaticClass()))
		{
			return SNew(SGraphNodeK2Sequence, CastChecked<UK2Node>(InNode));
		}
		else if (UK2Node_Timeline* TimelineNode = Cast<UK2Node_Timeline>(InNode))
		{
			return SNew(SGraphNodeK2Timeline, TimelineNode);
		}
		else if(UK2Node_SpawnActor* SpawnActorNode = Cast<UK2Node_SpawnActor>(InNode))
		{
			return SNew(SGraphNodeSpawnActor, SpawnActorNode);
		}
		else if(UK2Node_SpawnActorFromClass* SpawnActorNodeFromClass = Cast<UK2Node_SpawnActorFromClass>(InNode))
		{
			return SNew(SGraphNodeSpawnActorFromClass, SpawnActorNodeFromClass);
		}
		else if(UK2Node_CreateDelegate* CreateDelegateNode = Cast<UK2Node_CreateDelegate>(InNode))
		{
			return SNew(SGraphNodeK2CreateDelegate, CreateDelegateNode);
		}
		else if (UK2Node_CallMaterialParameterCollectionFunction* CallFunctionNode = Cast<UK2Node_CallMaterialParameterCollectionFunction>(InNode))
		{
			return SNew(SGraphNodeCallParameterCollectionFunction, CallFunctionNode);
		}
		else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(InNode))
		{
			return SNew(SGraphNodeK2Event, EventNode);
		}
		else if (UK2Node_FormatText* FormatTextNode = Cast<UK2Node_FormatText>(InNode))
		{
			return SNew(SGraphNodeFormatText, FormatTextNode);
		}
		else if (UK2Node_Knot* Knot = Cast<UK2Node_Knot>(InNode))
		{
			return SNew(SGraphNodeKnot, Knot);
		}
		else if (UK2Node_MakeStruct* MakeStruct = Cast<UK2Node_MakeStruct>(InNode))
		{
			return SNew(SGraphNodeMakeStruct, MakeStruct);
		}
		else if (UK2Node_Copy* CopyNode = Cast<UK2Node_Copy>(InNode))
		{
			return SNew(SGraphNodeK2Copy, CopyNode);
		}
		else
		{
			return SNew(SGraphNodeK2Default, K2Node);
		}
	}
	else if(UEdGraphNode_Documentation* DocNode = Cast<UEdGraphNode_Documentation>(InNode))
	{
		return SNew(SGraphNodeDocumentation, DocNode);
	}
	else if (UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(InNode))
	{
		if (UMaterialGraphNode_Comment* MaterialCommentNode = Cast<UMaterialGraphNode_Comment>(InNode))
		{
			return SNew(SGraphNodeMaterialComment, MaterialCommentNode);
		}
		else
		{
			return SNew(SGraphNodeComment, CommentNode);
		}
	}
	else
	{
		return SNew(SGraphNodeDefault)
			.GraphNodeObj(InNode);
	}
}

TSharedPtr<SGraphPin> FNodeFactory::CreatePinWidget(UEdGraphPin* InPin)
{
	check(InPin != NULL);

	// First give a shot to the registered pin factories
	for (auto FactoryIt = FEdGraphUtilities::VisualPinFactories.CreateIterator(); FactoryIt; ++FactoryIt)
	{
		TSharedPtr<FGraphPanelPinFactory> FactoryPtr = *FactoryIt;
		if (FactoryPtr.IsValid())
		{
			TSharedPtr<SGraphPin> ResultVisualPin = FactoryPtr->CreatePin(InPin);
			if (ResultVisualPin.IsValid())
			{
				return ResultVisualPin;
			}
		}
	}

	if (const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(InPin->GetSchema()))
	{
		if (InPin->PinType.PinCategory == K2Schema->PC_Boolean)
		{
			return SNew(SGraphPinBool, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_Text)
		{
			return SNew(SGraphPinText, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_Exec)
		{
			return SNew(SGraphPinExec, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_Object)
		{
			return SNew(SGraphPinObject, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_Interface)
		{
			return SNew(SGraphPinObject, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_SoftObject)
		{
			return SNew(SGraphPinObject, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_Class)
		{
			return SNew(SGraphPinClass, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_SoftClass)
		{
			return SNew(SGraphPinClass, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_Int)
		{
			return SNew(SGraphPinInteger, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_Float)
		{
			return SNew(SGraphPinNum, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_String || InPin->PinType.PinCategory == K2Schema->PC_Name)
		{
			return SNew(SGraphPinString, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_Struct)
		{
			// If you update this logic you'll probably need to update UEdGraphSchema_K2::ShouldHidePinDefaultValue!
			UScriptStruct* ColorStruct = TBaseStructure<FLinearColor>::Get();
			UScriptStruct* VectorStruct = TBaseStructure<FVector>::Get();
			UScriptStruct* Vector2DStruct = TBaseStructure<FVector2D>::Get();
			UScriptStruct* RotatorStruct = TBaseStructure<FRotator>::Get();

			if (InPin->PinType.PinSubCategoryObject == ColorStruct)
			{
				return SNew(SGraphPinColor, InPin);
			}
			else if ((InPin->PinType.PinSubCategoryObject == VectorStruct) || (InPin->PinType.PinSubCategoryObject == RotatorStruct))
			{
				return SNew(SGraphPinVector, InPin);
			}
			else if (InPin->PinType.PinSubCategoryObject == Vector2DStruct)
			{
				return SNew(SGraphPinVector2D, InPin);
			}
			else if (InPin->PinType.PinSubCategoryObject == FKey::StaticStruct())
			{
				return SNew(SGraphPinKey, InPin);
			}
			else if (InPin->PinType.PinSubCategoryObject == FCollisionProfileName::StaticStruct())
			{
				return SNew(SGraphPinCollisionProfile, InPin);
			}
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_Byte)
		{
			// Check for valid enum object reference
			if ((InPin->PinType.PinSubCategoryObject != NULL) && (InPin->PinType.PinSubCategoryObject->IsA(UEnum::StaticClass())))
			{
				return SNew(SGraphPinEnum, InPin);
			}
			else
			{
				return SNew(SGraphPinInteger, InPin);
			}
		}
		else if ((InPin->PinType.PinCategory == K2Schema->PC_Wildcard) && (InPin->PinType.PinSubCategory == K2Schema->PSC_Index))
		{
			return SNew(SGraphPinIndex, InPin);
		}
		else if(InPin->PinType.PinCategory == K2Schema->PC_MCDelegate)
		{
			return SNew(SGraphPinString, InPin);
		}
	}

	if (const UMaterialGraphSchema* MaterialGraphSchema = Cast<const UMaterialGraphSchema>(InPin->GetSchema()))
	{
		if (InPin->PinType.PinCategory == MaterialGraphSchema->PC_MaterialInput)
		{
			return SNew(SGraphPinMaterialInput, InPin);
		}
		else
		{
			return SNew(SGraphPin, InPin);
		}
	}
	
	// If we didn't pick a custom pin widget, use an uncustomized basic pin
	return SNew(SGraphPin, InPin);
}

FConnectionDrawingPolicy* FNodeFactory::CreateConnectionPolicy(const UEdGraphSchema* Schema, int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
{
    FConnectionDrawingPolicy* ConnectionDrawingPolicy;

    // First give the schema a chance to provide the connection drawing policy
    ConnectionDrawingPolicy = Schema->CreateConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj);

    // First give a shot to the registered connection factories
    if (!ConnectionDrawingPolicy)
    {
        for (auto FactoryIt = FEdGraphUtilities::VisualPinConnectionFactories.CreateIterator(); FactoryIt; ++FactoryIt)
        {
            TSharedPtr<FGraphPanelPinConnectionFactory> FactoryPtr = *FactoryIt;
            if (FactoryPtr.IsValid())
            {
                ConnectionDrawingPolicy = FactoryPtr->CreateConnectionPolicy(Schema, InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj);
            }
        }
    }

    // If neither the schema nor the factory provides a policy, try the hardcoded ones
    //@TODO: Fold all of this code into registered factories for the various schemas!
    if (!ConnectionDrawingPolicy)
    {
		if (Schema->IsA(UEdGraphSchema_K2::StaticClass()))
        {
            ConnectionDrawingPolicy = new FKismetConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj);
        }
        else if (Schema->IsA(UMaterialGraphSchema::StaticClass()))
        {
            ConnectionDrawingPolicy = new FMaterialGraphConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj);
        }
        else
        {
            ConnectionDrawingPolicy = new FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements);
        }
    }

    // If we never picked a custom policy, use the uncustomized standard policy
    return ConnectionDrawingPolicy;
}
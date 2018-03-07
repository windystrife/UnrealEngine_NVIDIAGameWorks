// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/////////////////////////////////////////////////////
// UMaterialGraph

#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphNode_Comment.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialGraph/MaterialGraphNode_Root.h"

#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "Materials/MaterialExpressionReroute.h"

#include "MaterialGraphNode_Knot.h"

#define LOCTEXT_NAMESPACE "MaterialGraph"

UMaterialGraph::UMaterialGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMaterialGraph::RebuildGraph()
{
	check(Material);

	Modify();

	RemoveAllNodes();

	if (!MaterialFunction)
	{
		// Initialize the material input list.
		MaterialInputs.Add( FMaterialInputInfo( GetBaseColorPinName(), MP_BaseColor, LOCTEXT( "BaseColorToolTip", "Defines the overall color of the Material. Each channel is automatically clamped between 0 and 1" ) ) );
		MaterialInputs.Add( FMaterialInputInfo( GetMetallicPinName(), MP_Metallic, LOCTEXT( "MetallicToolTip", "Controls how \"metal-like\" your surface looks like") ) );
		MaterialInputs.Add( FMaterialInputInfo( LOCTEXT("Specular", "Specular"), MP_Specular, LOCTEXT("SpecularToolTip", "Used to scale the current amount of specularity on non-metallic surfaces and is a value between 0 and 1, default at 0.5") ) );
		MaterialInputs.Add( FMaterialInputInfo( LOCTEXT( "Roughness", "Roughness" ), MP_Roughness, LOCTEXT( "RoughnessToolTip", "Controls how rough the Material is. Roughness of 0 (smooth) is a mirror reflection and 1 (rough) is completely matte or diffuse" ) ) );
		MaterialInputs.Add( FMaterialInputInfo( GetEmissivePinName(), MP_EmissiveColor, LOCTEXT( "EmissiveToolTip", "Controls which parts of your Material will appear to glow" ) ) );
		MaterialInputs.Add( FMaterialInputInfo( GetOpacityPinName(), MP_Opacity, LOCTEXT( "OpacityToolTip", "Controls the transluecency of the Material" ) ) );
		MaterialInputs.Add( FMaterialInputInfo( LOCTEXT("OpacityMask", "Opacity Mask"), MP_OpacityMask, LOCTEXT( "OpacityMaskToolTip", "When in Masked mode, a Material is either completely visible or completely invisible" ) ) );
		MaterialInputs.Add( FMaterialInputInfo( GetNormalPinName(), MP_Normal, LOCTEXT( "NormalToolTip", "Takes the input of a normal map" ) ) );
		MaterialInputs.Add( FMaterialInputInfo( GetWorldPositionOffsetPinName(), MP_WorldPositionOffset, LOCTEXT( "WorldPositionOffsetToolTip", "Allows for the vertices of a mesh to be manipulated in world space by the Material" ) ) );
		MaterialInputs.Add( FMaterialInputInfo( LOCTEXT( "WorldDisplacement", "World Displacement" ), MP_WorldDisplacement, LOCTEXT( "WorldDisplacementToolTip", "Allows for the tessellation vertices to be manipulated in world space by the Material" ) ) );
		MaterialInputs.Add( FMaterialInputInfo( LOCTEXT("TessellationMultiplier", "Tessellation Multiplier"), MP_TessellationMultiplier, LOCTEXT( "TessllationMultiplierToolTip", "Controls the amount tessellation along the surface" ) ) );
		MaterialInputs.Add( FMaterialInputInfo( GetSubsurfacePinName(), MP_SubsurfaceColor, LOCTEXT( "SubsurfaceToolTip", "Allows you to add a color to your Material to simulate shifts in color when light passes through the surface" ) ) );
		MaterialInputs.Add( FMaterialInputInfo( GetCustomDataPinName(0), MP_CustomData0, GetCustomDataPinName( 0 ) ) );
		MaterialInputs.Add( FMaterialInputInfo( GetCustomDataPinName(1), MP_CustomData1, GetCustomDataPinName( 1 ) ) );
		MaterialInputs.Add( FMaterialInputInfo( LOCTEXT("AmbientOcclusion", "Ambient Occlusion"), MP_AmbientOcclusion, LOCTEXT( "AmbientOcclusionToolTip", "Simulate the self-shadowing that happens within crevices of a surface" ) ) );
		MaterialInputs.Add( FMaterialInputInfo( LOCTEXT("Refraction", "Refraction"), MP_Refraction, LOCTEXT( "RefractionToolTip", "Takes in a texture or value that simulates the index of refraction of the surface" ) ) );

		for (int32 UVIndex = 0; UVIndex < ARRAY_COUNT(Material->CustomizedUVs); UVIndex++)
		{
			//@todo - localize
			MaterialInputs.Add( FMaterialInputInfo( FText::FromString(FString::Printf(TEXT("Customized UV%u"), UVIndex)), (EMaterialProperty)(MP_CustomizedUVs0 + UVIndex), FText::FromString(FString::Printf( TEXT( "CustomizedUV%uToolTip" ), UVIndex ) ) ) );
		}

		MaterialInputs.Add(FMaterialInputInfo(LOCTEXT("PixelDepthOffset", "Pixel Depth Offset"), MP_PixelDepthOffset, LOCTEXT( "PixelDepthOffsetToolTip", "Pixel Depth Offset" ) ));

		//^^^ New material properties go above here. ^^^^
		MaterialInputs.Add(FMaterialInputInfo(LOCTEXT("MaterialAttributes", "Material Attributes"), MP_MaterialAttributes, LOCTEXT( "MaterialAttributesToolTip", "Material Attributes" ) ));

		// Add Root Node
		FGraphNodeCreator<UMaterialGraphNode_Root> NodeCreator(*this);
		RootNode = NodeCreator.CreateNode();
		RootNode->Material = Material;
		NodeCreator.Finalize();
	}

	for (int32 Index = 0; Index < Material->Expressions.Num(); Index++)
	{
		AddExpression(Material->Expressions[Index]);
	}

	for (int32 Index = 0; Index < Material->EditorComments.Num(); Index++)
	{
		AddComment(Material->EditorComments[Index]);
	}

	LinkGraphNodesFromMaterial();
}

UMaterialGraphNode* UMaterialGraph::AddExpression(UMaterialExpression* Expression)
{
	UMaterialGraphNode* NewNode = NULL;
	if (Expression && Expression->IsA(UMaterialExpressionReroute::StaticClass()))
	{
		Modify();
		FGraphNodeCreator<UMaterialGraphNode_Knot> NodeCreator(*this);
		NewNode = NodeCreator.CreateNode(false);
		NewNode->MaterialExpression = Expression;
		NewNode->RealtimeDelegate = RealtimeDelegate;
		NewNode->MaterialDirtyDelegate = MaterialDirtyDelegate;
		Expression->GraphNode = NewNode;
		NodeCreator.Finalize();
	}
	else if (Expression)
	{
		Modify();
		FGraphNodeCreator<UMaterialGraphNode> NodeCreator(*this);
		NewNode = NodeCreator.CreateNode(false);
		NewNode->MaterialExpression = Expression;
		NewNode->RealtimeDelegate = RealtimeDelegate;
		NewNode->MaterialDirtyDelegate = MaterialDirtyDelegate;
		Expression->GraphNode = NewNode;
		NodeCreator.Finalize();
	}

	return NewNode;
}

UMaterialGraphNode_Comment* UMaterialGraph::AddComment(UMaterialExpressionComment* Comment, bool bIsUserInvoked)
{
	UMaterialGraphNode_Comment* NewComment = NULL;
	if (Comment)
	{
		Modify();
		FGraphNodeCreator<UMaterialGraphNode_Comment> NodeCreator(*this);
		if (bIsUserInvoked)
		{
			NewComment = NodeCreator.CreateUserInvokedNode(true);
		}
		else
		{
			NewComment = NodeCreator.CreateNode(false);
		}
		NewComment->MaterialExpressionComment = Comment;
		NewComment->MaterialDirtyDelegate = MaterialDirtyDelegate;
		Comment->GraphNode = NewComment;
		NodeCreator.Finalize();
	}

	return NewComment;
}

void UMaterialGraph::LinkGraphNodesFromMaterial()
{
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		Nodes[Index]->BreakAllNodeLinks();
	}

	if (RootNode)
	{
		// Use Material Inputs to make GraphNode Connections
		for (int32 Index = 0; Index < MaterialInputs.Num(); ++Index)
		{
			UEdGraphPin* InputPin = RootNode->GetInputPin(Index);
			auto ExpressionInput = MaterialInputs[Index].GetExpressionInput(Material);

			if (ExpressionInput.Expression)
			{
				UMaterialGraphNode* GraphNode = CastChecked<UMaterialGraphNode>(ExpressionInput.Expression->GraphNode);
				InputPin->MakeLinkTo(GraphNode->GetOutputPin(GetValidOutputIndex(&ExpressionInput)));
			}
		}
	}

	for (int32 Index = 0; Index < Material->Expressions.Num(); Index++)
	{
		UMaterialExpression* Expression = Material->Expressions[Index];

		if (Expression)
		{
			const TArray<FExpressionInput*> ExpressionInputs = Expression->GetInputs();
			for (int32 InputIndex = 0; InputIndex < ExpressionInputs.Num(); ++InputIndex)
			{
				UEdGraphPin* InputPin = CastChecked<UMaterialGraphNode>(Expression->GraphNode)->GetInputPin(InputIndex);

				// InputPin can be null during a PostEditChange when there is a circular dependency between nodes, and nodes have pins that are dynamically created
				if (InputPin != nullptr && ExpressionInputs[InputIndex]->Expression)
				{
					UMaterialGraphNode* GraphNode = CastChecked<UMaterialGraphNode>(ExpressionInputs[InputIndex]->Expression->GraphNode);
					InputPin->MakeLinkTo(GraphNode->GetOutputPin(GetValidOutputIndex(ExpressionInputs[InputIndex])));
				}
			}
		}
	}

	NotifyGraphChanged();
}

void UMaterialGraph::LinkMaterialExpressionsFromGraph() const
{
	// Use GraphNodes to make Material Expression Connections
	TArray<UEdGraphPin*> InputPins;
	TArray<UEdGraphPin*> OutputPins;

	for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
	{
		if (RootNode && RootNode == Nodes[NodeIndex])
		{
			// Setup Material's inputs from root node
			Material->Modify();
			InputPins = RootNode->Pins;
			Material->EditorX = RootNode->NodePosX;
			Material->EditorY = RootNode->NodePosY;
			check(InputPins.Num() == MaterialInputs.Num());
			for (int32 PinIndex = 0; PinIndex < InputPins.Num() && PinIndex < MaterialInputs.Num(); ++PinIndex)
			{
				FExpressionInput& MaterialInput = MaterialInputs[PinIndex].GetExpressionInput(Material);

				if (InputPins[PinIndex]->LinkedTo.Num() > 0)
				{
					UMaterialGraphNode* ConnectedNode = CastChecked<UMaterialGraphNode>(InputPins[PinIndex]->LinkedTo[0]->GetOwningNode());
					ConnectedNode->GetOutputPins(OutputPins);

					// Work out the index of the connected pin
					for (int32 OutPinIndex = 0; OutPinIndex < OutputPins.Num(); ++OutPinIndex)
					{
						if (OutputPins[OutPinIndex] == InputPins[PinIndex]->LinkedTo[0])
						{
							if (MaterialInput.OutputIndex != OutPinIndex || MaterialInput.Expression != ConnectedNode->MaterialExpression)
							{
								ConnectedNode->MaterialExpression->Modify();
								MaterialInput.Connect(OutPinIndex, ConnectedNode->MaterialExpression);
							}
							break;
						}
					}
				}
				else if (MaterialInput.Expression)
				{
					MaterialInput.Expression = NULL;
				}
			}
		}
		else
		{
			if (UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(Nodes[NodeIndex]))
			{
				// Need to be sure that we are changing the expression before calling modify -
				// triggers a rebuild of its preview when it is called
				UMaterialExpression* Expression = GraphNode->MaterialExpression;
				bool bModifiedExpression = false;

				if (Expression->MaterialExpressionEditorX != GraphNode->NodePosX
					|| Expression->MaterialExpressionEditorY != GraphNode->NodePosY
					|| Expression->Desc != GraphNode->NodeComment)
				{
					bModifiedExpression = true;

					Expression->Modify();

					// Update positions and comments
					Expression->MaterialExpressionEditorX = GraphNode->NodePosX;
					Expression->MaterialExpressionEditorY = GraphNode->NodePosY;
					Expression->Desc = GraphNode->NodeComment;
				}

				GraphNode->GetInputPins(InputPins);
				const TArray<FExpressionInput*> ExpressionInputs = Expression->GetInputs();
				checkf(InputPins.Num() == ExpressionInputs.Num(), TEXT("Mismatched inputs for '%s'"), *Expression->GetFullName());
				for (int32 PinIndex = 0; PinIndex < InputPins.Num() && PinIndex < ExpressionInputs.Num(); ++PinIndex)
				{
					FExpressionInput* ExpressionInput = ExpressionInputs[PinIndex];
					if (InputPins[PinIndex]->LinkedTo.Num() > 0)
					{
						UMaterialGraphNode* ConnectedNode = CastChecked<UMaterialGraphNode>(InputPins[PinIndex]->LinkedTo[0]->GetOwningNode());
						ConnectedNode->GetOutputPins(OutputPins);

						// Work out the index of the connected pin
						for (int32 OutPinIndex = 0; OutPinIndex < OutputPins.Num(); ++OutPinIndex)
						{
							if (OutputPins[OutPinIndex] == InputPins[PinIndex]->LinkedTo[0])
							{
								if (ExpressionInput->OutputIndex != OutPinIndex || ExpressionInput->Expression != ConnectedNode->MaterialExpression)
								{
									if (!bModifiedExpression)
									{
										bModifiedExpression = true;
										Expression->Modify();
									}
									ConnectedNode->MaterialExpression->Modify();
									ExpressionInput->Connect(OutPinIndex, ConnectedNode->MaterialExpression);
								}
								break;
							}
						}
					}
					else if (ExpressionInput->Expression)
					{
						if (!bModifiedExpression)
						{
							bModifiedExpression = true;
							Expression->Modify();
						}
						ExpressionInput->Expression = NULL;
					}
				}
			}
			else if (UMaterialGraphNode_Comment* CommentNode = Cast<UMaterialGraphNode_Comment>(Nodes[NodeIndex]))
			{
				UMaterialExpressionComment* Comment = CommentNode->MaterialExpressionComment;

				if (Comment->MaterialExpressionEditorX != CommentNode->NodePosX
					|| Comment->MaterialExpressionEditorY != CommentNode->NodePosY
					|| Comment->Text != CommentNode->NodeComment
					|| Comment->SizeX != CommentNode->NodeWidth
					|| Comment->SizeY != CommentNode->NodeHeight
					|| Comment->CommentColor != CommentNode->CommentColor)
				{
					Comment->Modify();

					// Update positions and comments
					Comment->MaterialExpressionEditorX = CommentNode->NodePosX;
					Comment->MaterialExpressionEditorY = CommentNode->NodePosY;
					Comment->Text = CommentNode->NodeComment;
					Comment->SizeX = CommentNode->NodeWidth;
					Comment->SizeY = CommentNode->NodeHeight;
					Comment->CommentColor = CommentNode->CommentColor;
				}
			}
		}
	}
}

bool UMaterialGraph::IsInputActive(UEdGraphPin* GraphPin) const
{
	if (Material && RootNode)
	{
		for (int32 Index = 0; Index < RootNode->Pins.Num(); ++Index)
		{
			if (RootNode->Pins[Index] == GraphPin)
			{
				return Material->IsPropertyActive(MaterialInputs[Index].GetProperty());
			}
		}
	}
	return true;
}

void UMaterialGraph::GetUnusedExpressions(TArray<UEdGraphNode*>& UnusedNodes) const
{
	UnusedNodes.Empty();

	TArray<UEdGraphNode*> NodesToCheck;

	if (RootNode)
	{
		TArray<UEdGraphPin*> InputPins;
		RootNode->GetInputPins(InputPins);
		for (int32 Index = 0; Index < InputPins.Num(); ++Index)
		{
			check(Index < MaterialInputs.Num());
			
			if (MaterialInputs[Index].IsVisiblePin(Material)
				&& InputPins[Index]->LinkedTo.Num() > 0 && InputPins[Index]->LinkedTo[0])
			{
				NodesToCheck.Push(InputPins[Index]->LinkedTo[0]->GetOwningNode());
			}
		}

		for (int32 Index = 0; Index < Nodes.Num(); Index++)
		{
			UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(Nodes[Index]);
			if (GraphNode)
			{
				UMaterialExpressionCustomOutput* CustomOutput = Cast<UMaterialExpressionCustomOutput>(GraphNode->MaterialExpression);
				if (CustomOutput)
				{
					NodesToCheck.Push(GraphNode);
				}
			}
		}
	}
	else if (MaterialFunction)
	{
		for (int32 Index = 0; Index < Nodes.Num(); Index++)
		{
			UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(Nodes[Index]);
			if (GraphNode)
			{
				UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(GraphNode->MaterialExpression);
				if (FunctionOutput)
				{
					NodesToCheck.Push(GraphNode);
				}
			}
		}
	}

	// Depth-first traverse the material expression graph.
	TArray<UEdGraphNode*> UsedNodes;
	TMap<UEdGraphNode*, int32> ReachableNodes;
	while (NodesToCheck.Num() > 0)
	{
		UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(NodesToCheck.Pop());
		if (GraphNode)
		{
			int32* AlreadyVisited = ReachableNodes.Find(GraphNode);
			if (!AlreadyVisited)
			{
				// Mark the expression as reachable.
				ReachableNodes.Add(GraphNode, 0);
				UsedNodes.Add(GraphNode);

				// Iterate over the expression's inputs and add them to the pending stack.
				TArray<UEdGraphPin*> InputPins;
				GraphNode->GetInputPins(InputPins);
				for (int32 Index = 0; Index < InputPins.Num(); ++Index)
				{
					if (InputPins[Index]->LinkedTo.Num() > 0 && InputPins[Index]->LinkedTo[0])
					{
						NodesToCheck.Push(InputPins[Index]->LinkedTo[0]->GetOwningNode());
					}
				}
			}
		}
	}

	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(Nodes[Index]);

		if (GraphNode && !UsedNodes.Contains(GraphNode))
		{
			UnusedNodes.Add(GraphNode);
		}
	}
}

void UMaterialGraph::RemoveAllNodes()
{
	MaterialInputs.Empty();

	RootNode = NULL;

	TArray<UEdGraphNode*> NodesToRemove = Nodes;
	for (int32 NodeIndex = 0; NodeIndex < NodesToRemove.Num(); ++NodeIndex)
	{
		NodesToRemove[NodeIndex]->Modify();
		RemoveNode(NodesToRemove[NodeIndex]);
	}
}

int32 UMaterialGraph::GetValidOutputIndex(FExpressionInput* Input) const
{
	int32 OutputIndex = 0;

	if (Input->Expression)
	{
		TArray<FExpressionOutput>& Outputs = Input->Expression->GetOutputs();

		if (Outputs.Num() > 0)
		{
			const bool bOutputIndexIsValid = Outputs.IsValidIndex(Input->OutputIndex)
				// Attempt to handle legacy connections before OutputIndex was used that had a mask
				&& (Input->OutputIndex != 0 || Input->Mask == 0);

			for( ; OutputIndex < Outputs.Num() ; ++OutputIndex )
			{
				const FExpressionOutput& Output = Outputs[OutputIndex];

				if((bOutputIndexIsValid && OutputIndex == Input->OutputIndex)
					|| (!bOutputIndexIsValid
					&& Output.Mask == Input->Mask
					&& Output.MaskR == Input->MaskR
					&& Output.MaskG == Input->MaskG
					&& Output.MaskB == Input->MaskB
					&& Output.MaskA == Input->MaskA))
				{
					break;
				}
			}

			if (OutputIndex >= Outputs.Num())
			{
				// Work around for non-reproducible crash where OutputIndex would be out of bounds
				OutputIndex = Outputs.Num() - 1;
			}
		}
	}

	return OutputIndex;
}

FText UMaterialGraph::GetEmissivePinName() const
{
	return Material->IsUIMaterial() ? LOCTEXT("UIOutputColor", "Final Color") : LOCTEXT("EmissiveColor", "Emissive Color");
}

FText UMaterialGraph::GetBaseColorPinName() const
{
	return Material->MaterialDomain == MD_Volume ? LOCTEXT("Albedo", "Albedo") : LOCTEXT("BaseColor", "Base Color");
}

FText UMaterialGraph::GetOpacityPinName() const
{
	return Material->MaterialDomain == MD_Volume ? LOCTEXT("Extinction", "Extinction") : LOCTEXT("Opacity", "Opacity");
}

FText UMaterialGraph::GetMetallicPinName() const
{
	return Material->GetShadingModel() == MSM_Hair ? LOCTEXT("Scatter", "Scatter") : LOCTEXT("Metallic", "Metallic");
}

FText UMaterialGraph::GetNormalPinName() const
{
	return Material->GetShadingModel() == MSM_Hair ? LOCTEXT("Tangent", "Tangent") : LOCTEXT("Normal", "Normal");
}

FText UMaterialGraph::GetWorldPositionOffsetPinName() const
{
	return Material->IsUIMaterial() ? LOCTEXT("ScreenPosition", "Screen Position") : LOCTEXT("WorldPositionOffset", "World Position Offset");
}

FText UMaterialGraph::GetSubsurfacePinName() const
{
	switch (Material->GetShadingModel())
	{
	case MSM_Cloth:
		return LOCTEXT("FuzzColor", "Fuzz Color");
	default:
		return LOCTEXT("SubsurfaceColor", "Subsurface Color");
	}
}

FText UMaterialGraph::GetCustomDataPinName( uint32 Index ) const
{
	if( Index == 0 )
	{
		switch( Material->GetShadingModel() )
		{
		case MSM_ClearCoat:
			return LOCTEXT("ClearCoat", "Clear Coat");
		case MSM_Hair:
			return LOCTEXT("Backlit", "Backlit");
		case MSM_Cloth:
			return LOCTEXT("Cloth", "Cloth");
		case MSM_Eye:
			return LOCTEXT("IrisMask", "Iris Mask");
		default:
			return LOCTEXT("CustomData0", "Custom Data 0");
		}
	}
	else if( Index == 1 )
	{
		switch( Material->GetShadingModel() )
		{
		case MSM_ClearCoat:
			return LOCTEXT("ClearCoatRoughness", "Clear Coat Roughness");
		case MSM_Eye:
			return LOCTEXT("IrisDistance", "Iris Distance");
		default:
			return LOCTEXT("CustomData1", "Custom Data 1");
		}
	}

	return LOCTEXT("CustomData", "Custom Data");	
}

#undef LOCTEXT_NAMESPACE

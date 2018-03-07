// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialGraphNode_Root.cpp
=============================================================================*/

#include "MaterialGraph/MaterialGraphNode_Root.h"
#include "MaterialShared.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "MaterialEditorUtilities.h"
#include "GraphEditorSettings.h"

#define LOCTEXT_NAMESPACE "MaterialGraphNode_Root"

/////////////////////////////////////////////////////
// UMaterialGraphNode_Root

UMaterialGraphNode_Root::UMaterialGraphNode_Root(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UMaterialGraphNode_Root::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FMaterialEditorUtilities::GetOriginalObjectName(this->GetGraph());
}

FLinearColor UMaterialGraphNode_Root::GetNodeTitleColor() const
{
	return GetDefault<UGraphEditorSettings>()->ResultNodeTitleColor;
}

FText UMaterialGraphNode_Root::GetTooltipText() const
{
	return LOCTEXT("MaterialNode", "Result node of the Material");
}

void UMaterialGraphNode_Root::PostPlacedNewNode()
{
	if (Material)
	{
		NodePosX = Material->EditorX;
		NodePosY = Material->EditorY;
	}
}

void UMaterialGraphNode_Root::CreateInputPins()
{
	UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(GetGraph());
	const UMaterialGraphSchema* Schema = CastChecked<UMaterialGraphSchema>(GetSchema());

	for (const FMaterialInputInfo& MaterialInput : MaterialGraph->MaterialInputs)
	{
		UEdGraphPin* InputPin = CreatePin(EGPD_Input, Schema->PC_MaterialInput, FString::Printf(TEXT("%d"), (int32)MaterialInput.GetProperty()), nullptr, MaterialInput.GetName().ToString());
	}

}

int32 UMaterialGraphNode_Root::GetInputIndex(const UEdGraphPin* InputPin) const
{
	for (int32 Index = 0; Index < Pins.Num(); ++Index)
	{
		if (InputPin == Pins[Index])
		{
			return Index;
		}
	}

	return -1;
}

uint32 UMaterialGraphNode_Root::GetInputType(const UEdGraphPin* InputPin) const
{
	UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(GetGraph());
	EMaterialProperty Property = MaterialGraph->MaterialInputs[GetInputIndex(InputPin)].GetProperty();
	if (Property == MP_MaterialAttributes)
	{
		return MCT_MaterialAttributes;
	}
	else
	{
		return FMaterialAttributeDefinitionMap::GetValueType(Property);
	}
}

#undef LOCTEXT_NAMESPACE

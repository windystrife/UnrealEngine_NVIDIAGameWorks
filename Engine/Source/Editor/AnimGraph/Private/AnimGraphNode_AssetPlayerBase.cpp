// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_AssetPlayerBase.h"
#include "EdGraphSchema_K2.h"
#include "Animation/AnimComposite.h"
#include "Animation/BlendSpaceBase.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_SequenceEvaluator.h"
#include "AnimGraphNode_RotationOffsetBlendSpace.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_BlendSpaceEvaluator.h"
#include "Animation/PoseAsset.h"
#include "AnimGraphNode_PoseBlendNode.h"
#include "AnimGraphNode_PoseByName.h"
#include "AnimGraphNode_PoseDriver.h"
#include "UObjectIterator.h"

void UAnimGraphNode_AssetPlayerBase::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

	if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
	{
		// recache visualization now an asset pin's connection is changed
		if (const UEdGraphSchema* Schema = GetSchema())
		{
			Schema->ForceVisualizationCacheClear();
		}
	}
}

void UAnimGraphNode_AssetPlayerBase::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);

	if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
	{
		// recache visualization now an asset pin's default value has changed
		if (const UEdGraphSchema* Schema = GetSchema())
		{
			Schema->ForceVisualizationCacheClear();
		}
	}
}

UClass* GetNodeClassForAsset(const UClass* AssetClass)
{
	UClass* NodeClass = nullptr;

	// Iterate over all classes..
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass *Class = *ClassIt;
		// Look for AnimGraphNode classes
		if (Class->IsChildOf(UAnimGraphNode_Base::StaticClass()))
		{
			// See if this node is the 'primary handler' for this asset type
			const UAnimGraphNode_Base* NodeCDO = Class->GetDefaultObject<UAnimGraphNode_Base>();
			if (NodeCDO->SupportsAssetClass(AssetClass) == EAnimAssetHandlerType::PrimaryHandler)
			{
				NodeClass = Class;
				break;
			}
		}
	}

	return NodeClass;
}

bool SupportNodeClassForAsset(const UClass* AssetClass, UClass* NodeClass)
{
	// Get node CDO
	const UAnimGraphNode_Base* NodeCDO = NodeClass->GetDefaultObject<UAnimGraphNode_Base>();
	// See if this node supports this asset type (primary or not)
	return (NodeCDO->SupportsAssetClass(AssetClass) != EAnimAssetHandlerType::NotSupported);
}

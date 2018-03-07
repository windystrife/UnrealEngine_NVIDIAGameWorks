// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NodeMappingContainer.h"
#include "Engine/Blueprint.h"

////////////////////////////////////////////////////////////////////////////////////////
UNodeMappingContainer::UNodeMappingContainer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
const INodeMappingProviderInterface* UNodeMappingContainer::GetSourceAssetCDO()
{
	const UBlueprint* SourceBP = GetSourceAsset();

	if (SourceBP)
	{
		UObject* SourceAssetCDO = SourceBP->GeneratedClass->GetDefaultObject();
		const INodeMappingProviderInterface* Interface = Cast<INodeMappingProviderInterface>(SourceAssetCDO);
		return Interface;
	}

	return nullptr;
}

void UNodeMappingContainer::SetSourceAsset(UBlueprint* InSourceAsset)
{
	SourceAsset = InSourceAsset;
}

void UNodeMappingContainer::SetNodeMapping(const FName& SourceNode, const FName& TargetNode, const FTransform& SourceTransform, const FTransform& TargetTransform)
{
	DeleteMapping(SourceNode);
	AddMapping(SourceNode, TargetNode, SourceTransform, TargetTransform);
}
#endif // WITH_EDITOR

UBlueprint* UNodeMappingContainer::GetSourceAsset()
{
	if (!SourceAsset.IsValid())
	{
		SourceAsset.LoadSynchronous();
	}

	return SourceAsset.Get();
}

void UNodeMappingContainer::AddMapping(const FName& SourceNode, const FName& TargetNode, const FTransform& SourceTransform, const FTransform& TargetTransform)
{
	FNodeMap NewMapping;
	NewMapping.TargetNodeName = TargetNode;
	NewMapping.SourceToTargetTransform = TargetTransform.GetRelativeTransform(SourceTransform);
	NewMapping.SourceToTargetTransform.NormalizeRotation();
	NodeMapping.Add(SourceNode, NewMapping);
}

void UNodeMappingContainer::DeleteMapping(const FName& SourceNode)
{
	NodeMapping.Remove(SourceNode);
}

#if WITH_EDITOR
FString UNodeMappingContainer::GetDisplayName() const
{
	//@todo : may have to do path name here because we have to make sure it's unique
	return SourceAsset.GetAssetName();
}
#endif

FName UNodeMappingContainer::GetTargetNodeName(const FName& SourceNode) const
{
	const FNodeMap* Found = NodeMapping.Find(SourceNode);
	if (Found)
	{
		return (Found->TargetNodeName);
	}

	return NAME_None;
}

FName UNodeMappingContainer::GetSourceName(const FName& TargetNode) const
{
	for (const TPair<FName, FNodeMap>& Iter : NodeMapping)
	{
		if (Iter.Value.TargetNodeName == TargetNode)
		{
			return Iter.Key;
		}
	}

	return NAME_None;
}
// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "NodeMappingProviderInterface.h"
#include "NodeMappingContainer.generated.h"

USTRUCT()
struct FNodeMap
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName TargetNodeName;

	UPROPERTY()
	FTransform SourceToTargetTransform;

	FNodeMap(FName InTargetName = NAME_None, const FTransform& InSourceToTarget = FTransform::Identity)
		: TargetNodeName(InTargetName)
		, SourceToTargetTransform(InSourceToTarget)
	{
	}
};
/** Animation Controller Mapping data container - this contains node mapping data */
UCLASS(hidecategories = Object, ClassGroup = "Animation", BlueprintType)
class ENGINE_API UNodeMappingContainer : public UObject
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(EditAnywhere, Category = Mapping)
	TMap<FName, FNodeMap>	NodeMapping;

	UPROPERTY(EditAnywhere, Category = Mapping)
	TSoftObjectPtr<UBlueprint>	SourceAsset; 

public:
#if WITH_EDITOR
	FString GetDisplayName() const;
	const INodeMappingProviderInterface* GetSourceAssetCDO();

	void SetSourceAsset(UBlueprint* InSourceAsset);
	void SetNodeMapping(const FName& SourceNode, const FName& TargetNode, const FTransform& SourceTransform, const FTransform& TargetTransform);
#endif// WITH_EDITOR
	UBlueprint* GetSourceAsset();
	FName GetTargetNodeName(const FName& SourceNode) const;
	// @todo: this is slow
	FName GetSourceName(const FName& TargetNode) const;
	const FNodeMap* GetNodeMapping(const FName& SourceNode) const
	{
		return NodeMapping.Find(SourceNode);
	}

private:
	void AddMapping(const FName& SourceNode, const FName& TargetNode, const FTransform& SourceTransform, const FTransform& TargetTransform);
	void DeleteMapping(const FName& SourceNode);

	int32 FindIndex(const FName& SourceNode) const;
};
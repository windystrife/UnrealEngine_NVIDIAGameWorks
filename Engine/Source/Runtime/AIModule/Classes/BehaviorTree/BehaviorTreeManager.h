// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "BehaviorTreeManager.generated.h"

class UBehaviorTree;
class UBehaviorTreeComponent;
class UBTCompositeNode;
class UBTDecorator;

USTRUCT()
struct FBehaviorTreeTemplateInfo
{
	GENERATED_USTRUCT_BODY()

	/** behavior tree asset */
	UPROPERTY()
	UBehaviorTree* Asset;

	/** initialized template */
	UPROPERTY(transient)
	UBTCompositeNode* Template;

	/** size required for instance memory */
	uint16 InstanceMemorySize;
};

UCLASS(config=Engine)
class AIMODULE_API UBehaviorTreeManager : public UObject
{
	GENERATED_UCLASS_BODY()

	/** limit for recording execution steps for debugger */
	UPROPERTY(config)
	int32 MaxDebuggerSteps;

	/** get behavior tree template for given blueprint */
	bool LoadTree(UBehaviorTree& Asset, UBTCompositeNode*& Root, uint16& InstanceMemorySize);

	/** get aligned memory size */
	static int32 GetAlignedDataSize(int32 Size);

	/** helper function for sorting and aligning node memory */
	static void InitializeMemoryHelper(const TArray<UBTDecorator*>& Nodes, TArray<uint16>& MemoryOffsets, int32& MemorySize, bool bForceInstancing = false);

	/** cleanup hooks for map loading */
	virtual void FinishDestroy() override;

	void DumpUsageStats() const;

	/** register new behavior tree component for tracking */
	void AddActiveComponent(UBehaviorTreeComponent& Component);

	/** unregister behavior tree component from tracking */
	void RemoveActiveComponent(UBehaviorTreeComponent& Component);

	static UBehaviorTreeManager* GetCurrent(UWorld* World);
	static UBehaviorTreeManager* GetCurrent(UObject* WorldContextObject);

protected:

	/** initialized tree templates */
	UPROPERTY()
	TArray<FBehaviorTreeTemplateInfo> LoadedTemplates;

	UPROPERTY()
	TArray<UBehaviorTreeComponent*> ActiveComponents;
};

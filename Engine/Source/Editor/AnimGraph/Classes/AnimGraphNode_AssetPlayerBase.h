// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimationAsset.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_AssetPlayerBase.generated.h"

/** Get the default anim node class for playing a particular asset */
ANIMGRAPH_API UClass* GetNodeClassForAsset(const UClass* AssetClass);

/** See if a particular anim NodeClass can play a particular anim AssetClass */
ANIMGRAPH_API bool SupportNodeClassForAsset(const UClass* AssetClass, UClass* NodeClass);

/** Helper / intermediate for asset player graphical nodes */
UCLASS(Abstract, MinimalAPI)
class UAnimGraphNode_AssetPlayerBase : public UAnimGraphNode_Base
{
	GENERATED_BODY()
public:
	// Sync group settings for this player.  Sync groups keep related animations with different lengths synchronized.
	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimationGroupReference SyncGroup;

	/** UEdGraphNode interface */
	ANIMGRAPH_API virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	ANIMGRAPH_API virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;

	virtual void SetAnimationAsset(UAnimationAsset* Asset) { check(false); /*Base function called*/ }
};

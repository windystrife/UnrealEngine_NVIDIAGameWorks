// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "Animation/AnimBlueprint.h"
#include "EditorParentPlayerListObj.generated.h"

class UAnimGraphNode_Base;

UCLASS()
class UNREALED_API UEditorParentPlayerListObj : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	// Build a possible list of overrides from a blueprint and pull in any existing overrides.
	void InitialiseFromBlueprint(UAnimBlueprint* Blueprint);

	// Adds an overridable node to the possible override list.
	FAnimParentNodeAssetOverride& AddOverridableNode(UAnimGraphNode_Base* Node);

	// Gets a pointer to a U-node from the stored GUID for that node.
	UAnimGraphNode_Base* GetVisualNodeFromGuid(FGuid InGuid) const;

	// Pushes an override into the blueprint we're editing.
	void ApplyOverrideToBlueprint(FAnimParentNodeAssetOverride& Override);

	// Get the current blueprint
	const UAnimBlueprint* GetBlueprint() const;

	// List of possible overrides to display.
	UPROPERTY(Category = AnimationGraphOverrides, EditAnywhere)
	TArray<FAnimParentNodeAssetOverride> Overrides;

private:
	UAnimBlueprint* AnimBlueprint;
	TMap<FGuid, UAnimGraphNode_Base*> GuidToVisualNodeMap;
};

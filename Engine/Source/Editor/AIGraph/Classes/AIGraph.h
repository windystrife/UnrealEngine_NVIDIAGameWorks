// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraph.h"
#include "AIGraph.generated.h"

class UEdGraphPin;

UCLASS()
class AIGRAPH_API UAIGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	int32 GraphVersion;

	virtual void OnCreated();
	virtual void OnLoaded();
	virtual void Initialize();

	virtual void UpdateAsset(int32 UpdateFlags = 0);
	virtual void UpdateVersion();
	virtual void MarkVersion();

	virtual void OnSubNodeDropped();
	virtual void OnNodesPasted(const FString& ImportStr);

	bool UpdateUnknownNodeClasses();
	void UpdateDeprecatedClasses();
	void RemoveOrphanedNodes();
	void UpdateClassData();

	bool IsLocked() const;
	void LockUpdates();
	void UnlockUpdates();

	//~ Begin UObject Interface.
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface.

protected:

	/** if set, graph modifications won't cause updates in internal tree structure
	 *  flag allows freezing update during heavy changes like pasting new nodes 
	 */
	uint32 bLockUpdates : 1;

	virtual void CollectAllNodeInstances(TSet<UObject*>& NodeInstances);
	virtual bool CanRemoveNestedObject(UObject* TestObject) const;
	virtual void OnNodeInstanceRemoved(UObject* NodeInstance);

	UEdGraphPin* FindGraphNodePin(UEdGraphNode* Node, EEdGraphPinDirection Dir);
};

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphNode.h"
#include "AssetData.h"
#include "EdGraph_ReferenceViewer.h"
#include "EdGraphNode_Reference.generated.h"

class UEdGraphPin;

UCLASS()
class UEdGraphNode_Reference : public UEdGraphNode
{
	GENERATED_UCLASS_BODY()

	virtual void SetupReferenceNode(const FIntPoint& NodeLoc, const TArray<FAssetIdentifier>& NewIdentifiers, const FAssetData& InAssetData);
	virtual void SetReferenceNodeCollapsed(const FIntPoint& NodeLoc, int32 InNumReferencesExceedingMax);
	virtual void AddReferencer(class UEdGraphNode_Reference* ReferencerNode);
	// Returns first asset identifier
	virtual FAssetIdentifier GetIdentifier() const;
	virtual void GetAllIdentifiers(TArray<FAssetIdentifier>& OutIdentifiers) const;
	// Returns only the packages in this node, skips searchable names
	virtual void GetAllPackageNames(TArray<FName>& OutPackageNames) const;
	class UEdGraph_ReferenceViewer* GetReferenceViewerGraph() const;

	// UEdGraphNode implementation
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual void AllocateDefaultPins() override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	// End UEdGraphNode implementation

	void CacheAssetData(const FAssetData& AssetData);

	bool UsesThumbnail() const;
	bool IsPackage() const;
	bool IsCollapsed() const;
	FAssetData GetAssetData() const;

	virtual UEdGraphPin* GetDependencyPin();
	virtual UEdGraphPin* GetReferencerPin();
protected:
	class UEdGraph_ReferenceViewer* GetReferenceGraph() const
	{
		return CastChecked<UEdGraph_ReferenceViewer>(GetOuter());
	}

private:
	TArray<FAssetIdentifier> Identifiers;
	FText NodeTitle;

	bool bUsesThumbnail;
	bool bIsPackage;
	bool bIsPrimaryAsset;
	bool bIsCollapsed;
	FAssetData CachedAssetData;

	UEdGraphPin* DependencyPin;
	UEdGraphPin* ReferencerPin;
};



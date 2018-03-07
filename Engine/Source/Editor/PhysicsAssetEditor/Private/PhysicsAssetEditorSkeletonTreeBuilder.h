// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SkeletonTreeBuilder.h"

class UPhysicsAsset;

class FPhysicsAssetEditorSkeletonTreeBuilder : public FSkeletonTreeBuilder
{
public:
	FPhysicsAssetEditorSkeletonTreeBuilder(UPhysicsAsset* InPhysicsAsset, const FSkeletonTreeBuilderArgs& InSkeletonTreeBuilderArgs = FSkeletonTreeBuilderArgs(true, false, true, false));

	/** ISkeletonTreeBuilder interface */
	virtual void Build(FSkeletonTreeBuilderOutput& Output) override;
	virtual ESkeletonTreeFilterResult FilterItem(const FSkeletonTreeFilterArgs& InArgs, const TSharedPtr<class ISkeletonTreeItem>& InItem) override;
	
	/** Flags used for filtering */
	bool bShowBodies;
	bool bShowConstraints;
	bool bShowPrimitives;

private:
	/** Add bodies & their shapes to the tree */
	void AddBodies(FSkeletonTreeBuilderOutput& Output);

private:
	/** The physics asset we use to populate the tree */
	UPhysicsAsset* PhysicsAsset;
};
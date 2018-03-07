// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowTabFactory.h"
#include "ISkeletonTree.h"
#include "SPhysicsAssetGraph.h"

class UPhysicsAsset;
class IEditableSkeleton;

struct FPhysicsAssetGraphSummoner : public FWorkflowTabFactory
{
public:
	FPhysicsAssetGraphSummoner(TSharedPtr<FAssetEditorToolkit> InHostingApp, UPhysicsAsset* InPhysicsAsset, const TSharedRef<IEditableSkeleton>& InEditableSkeleton, FOnPhysicsAssetGraphCreated InOnPhysicsAssetGraphCreated, FOnGraphObjectsSelected InOnGraphObjectsSelected);

	/** FWorkflowTabFactory interface */
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const override;

	/** Reference to our Physics Asset */
	TWeakObjectPtr<UPhysicsAsset> PhysicsAssetPtr;

	/** Reference to our editable skeleton */
	TWeakPtr<IEditableSkeleton> EditableSkeletonPtr;

	/** Graph created delegate */
	FOnPhysicsAssetGraphCreated OnPhysicsAssetGraphCreated;

	/** Object selected delegate */
	FOnGraphObjectsSelected OnGraphObjectsSelected;
};

// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowTabFactory.h"

class UPhysicsAsset;

struct FPhysicsAssetEditorProfilesSummoner : public FWorkflowTabFactory
{
public:
	FPhysicsAssetEditorProfilesSummoner(TSharedPtr<FAssetEditorToolkit> InHostingApp, UPhysicsAsset* InPhysicsAsset);

	/** FWorkflowTabFactory interface */
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const override;

	/** Reference to our Physics Asset */
	TWeakObjectPtr<UPhysicsAsset> PhysicsAssetPtr;
};

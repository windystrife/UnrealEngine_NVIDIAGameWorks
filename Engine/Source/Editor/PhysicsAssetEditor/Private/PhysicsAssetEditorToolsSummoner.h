// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowTabFactory.h"

struct FPhysicsAssetEditorToolsSummoner : public FWorkflowTabFactory
{
public:
	FPhysicsAssetEditorToolsSummoner(TSharedPtr<FAssetEditorToolkit> InHostingApp);

	/** FWorkflowTabFactory interface */
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const override;
};

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/ApplicationMode.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

class FSkeletonEditorMode : public FApplicationMode
{
public:
	FSkeletonEditorMode(TSharedRef<class FWorkflowCentricApplication> InHostingApp, TSharedRef<class ISkeletonTree> InSkeletonTree);

	/** FApplicationMode interface */
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;
	
protected:
	virtual void AddTabFactory(FCreateWorkflowTabFactory FactoryCreator) override;
	virtual void RemoveTabFactory(FName TabFactoryID) override;
protected:
	/** The hosting app */
	TWeakPtr<class FWorkflowCentricApplication> HostingAppPtr;

	/** The tab factories we support */
	FWorkflowAllowedTabSet TabFactories;
};

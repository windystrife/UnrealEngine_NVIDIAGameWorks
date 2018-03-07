// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Docking/TabManager.h"
#include "BehaviorTreeEditor.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "WorkflowOrientedApp/ApplicationMode.h"

/** Application mode for main behavior tree editing mode */
class FBehaviorTreeEditorApplicationMode : public FApplicationMode
{
public:
	FBehaviorTreeEditorApplicationMode(TSharedPtr<class FBehaviorTreeEditor> InBehaviorTreeEditor);

	virtual void RegisterTabFactories(TSharedPtr<class FTabManager> InTabManager) override;
	virtual void PreDeactivateMode() override;
	virtual void PostActivateMode() override;

protected:
	TWeakPtr<class FBehaviorTreeEditor> BehaviorTreeEditor;

	// Set of spawnable tabs in behavior tree editing mode
	FWorkflowAllowedTabSet BehaviorTreeEditorTabFactories;
};

/** Application mode for blackboard editing mode */
class FBlackboardEditorApplicationMode : public FApplicationMode
{
public:
	FBlackboardEditorApplicationMode(TSharedPtr<class FBehaviorTreeEditor> InBehaviorTreeEditor);

	virtual void RegisterTabFactories(TSharedPtr<class FTabManager> InTabManager) override;
	virtual void PostActivateMode() override;

protected:
	TWeakPtr<class FBehaviorTreeEditor> BehaviorTreeEditor;

	// Set of spawnable tabs in blackboard mode
	FWorkflowAllowedTabSet BlackboardTabFactories;
};

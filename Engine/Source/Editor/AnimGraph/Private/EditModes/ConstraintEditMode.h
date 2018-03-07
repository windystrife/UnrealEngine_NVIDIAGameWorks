// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNodeEditMode.h"
#include "AnimNode_Constraint.h"

class FConstraintEditMode : public FAnimNodeEditMode
{
public:
	FConstraintEditMode();

	/** IAnimNodeEditMode interface */
	virtual void EnterMode(class UAnimGraphNode_Base* InEditorNode, struct FAnimNode_Base* InRuntimeNode) override;
	virtual void ExitMode() override;
private:

private:
	struct FAnimNode_Constraint* RuntimeNode;
	class UAnimGraphNode_Constraint* GraphNode;

	// storing current widget mode 
	mutable FWidget::EWidgetMode CurWidgetMode;
};
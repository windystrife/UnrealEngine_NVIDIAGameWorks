// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h"

class UEdGraphNode;
struct Rect;

/** BT Editor public interface */
class IBehaviorTreeEditor : public FWorkflowCentricApplication
{

public:
	virtual uint32 GetSelectedNodesCount() const = 0;
	
	virtual void InitializeDebuggerState(class FBehaviorTreeDebugger* ParentDebugger) const = 0;
	virtual UEdGraphNode* FindInjectedNode(int32 Index) const = 0;
	virtual void DoubleClickNode(class UEdGraphNode* Node) = 0;
	virtual bool GetBoundsForSelectedNodes(class FSlateRect& Rect, float Padding) const = 0;
};



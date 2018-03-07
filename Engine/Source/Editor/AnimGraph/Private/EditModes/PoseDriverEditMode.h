// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNodeEditMode.h"

class FCanvas;
class FEditorViewportClient;
class FPrimitiveDrawInterface;

class FPoseDriverEditMode : public FAnimNodeEditMode
{
public:
	/** IAnimNodeEditMode interface */
	virtual void EnterMode(class UAnimGraphNode_Base* InEditorNode, struct FAnimNode_Base* InRuntimeNode) override;
	virtual void ExitMode() override;

	/** FEdMode interface */
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;

private:
	struct FAnimNode_PoseDriver* RuntimeNode;
	class UAnimGraphNode_PoseDriver* GraphNode;
};

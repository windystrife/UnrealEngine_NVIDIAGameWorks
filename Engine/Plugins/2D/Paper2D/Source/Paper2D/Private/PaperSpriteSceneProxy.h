// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpriteDrawCall.h"
#include "PaperRenderSceneProxy.h"

class FMeshElementCollector;
class UBodySetup;
class UPaperSpriteComponent;

//////////////////////////////////////////////////////////////////////////
// FPaperSpriteSceneProxy

class FPaperSpriteSceneProxy : public FPaperRenderSceneProxy
{
public:
	FPaperSpriteSceneProxy(UPaperSpriteComponent* InComponent);

	// FPrimitiveSceneProxy interface
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	// End of FPrimitiveSceneProxy interface

	void SetSprite_RenderThread(const struct FSpriteDrawCallRecord& NewDynamicData, int32 SplitIndex);

protected:

	// FPaperRenderSceneProxy interface
	virtual void GetDynamicMeshElementsForView(const FSceneView* View, int32 ViewIndex, FMeshElementCollector& Collector) const override;
	// End of FPaperRenderSceneProxy interface

protected:
	UMaterialInterface* AlternateMaterial;
	int32 MaterialSplitIndex;
	const UBodySetup* BodySetup;
	TArray<FSpriteDrawCallRecord> AlternateBatchedSprites;
};

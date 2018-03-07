// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PaperFlipbookSceneProxy.h"
#include "PaperFlipbookComponent.h"

//////////////////////////////////////////////////////////////////////////
// FPaperFlipbookSceneProxy

FPaperFlipbookSceneProxy::FPaperFlipbookSceneProxy(const UPaperFlipbookComponent* InComponent)
	: FPaperRenderSceneProxy(InComponent)
{
	//@TODO: PAPER2D: WireframeColor = RenderComp->GetWireframeColor();

	Material = InComponent->GetMaterial(0);
	MaterialRelevance = InComponent->GetMaterialRelevance(GetScene().GetFeatureLevel());
}

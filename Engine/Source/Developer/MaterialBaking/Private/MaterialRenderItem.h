// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CanvasTypes.h"
#include "MaterialRenderItemData.h"

class FSceneViewFamily;
class FMaterialRenderProxy;
class FSceneView;
class FRHICommandListImmediate;
struct FMaterialData;
struct FMeshData;
struct FDrawingPolicyRenderState;
struct FMaterialMeshVertex;

class FMeshMaterialRenderItem : public FCanvasBaseRenderItem
{
public:
	FMeshMaterialRenderItem(const FMaterialData* InMaterialSettings, const FMeshData* InMeshSettings, EMaterialProperty InMaterialProperty);

	/** Begin FCanvasBaseRenderItem overrides */
	virtual bool Render_RenderThread(FRHICommandListImmediate& RHICmdList, FDrawingPolicyRenderState& DrawRenderState, const FCanvas* Canvas) final;
	virtual bool Render_GameThread(const FCanvas* Canvas) final;
	/** End FCanvasBaseRenderItem overrides */

	/** Populate vertices and indices according to available mesh data and otherwise uses simple quad */
	void GenerateRenderData();
protected:
	/** Enqueues the current material to be rendered */
	void QueueMaterial(FRHICommandListImmediate& RHICmdList, FDrawingPolicyRenderState& DrawRenderState, const FSceneView* View);
	
	/** Helper functions to populate render data using either mesh data or a simple quad */
	void PopulateWithQuadData();
	void PopulateWithMeshData();
public:
	/** Mesh and material settings to use while baking out the material */
	const FMeshData* MeshSettings;
	const FMaterialData* MaterialSettings;
	/** Material property to bake out */
	EMaterialProperty MaterialProperty;
	/** Material render proxy (material/shader) to use while baking */
	FMaterialRenderProxy* MaterialRenderProxy;	
	/** Vertex and index data representing the mesh or a quad */
	TArray<FMaterialMeshVertex, TInlineAllocator<4>> Vertices;
	TArray<int32, TInlineAllocator<6>> Indices;
	/** Light cache interface object to simulate lightmap behaviour in case the material used prebaked ambient occlusion */
	FLightCacheInterface* LCI;
	/** View family to use while baking */
	FSceneViewFamily* ViewFamily;
};

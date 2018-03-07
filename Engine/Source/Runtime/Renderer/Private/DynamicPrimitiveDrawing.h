// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DynamicPrimitiveDrawing.h: Dynamic primitive drawing definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "SceneManagement.h"
#include "SceneRendering.h"

/**
* Draws a range of view's elements with the specified drawing policy factory type.
* @param View - The view to draw the meshes for.
* @param DrawingContext - The drawing policy type specific context for the drawing.
* @param DPGIndex World or Foreground DPG index for draw order
* @param FirstIndex - Element range
* @param LastIndex - Element range
*/
template<class DrawingPolicyFactoryType>
void DrawViewElementsInner(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	const FDrawingPolicyRenderState& DrawRenderState,
	const typename DrawingPolicyFactoryType::ContextType& DrawingContext,
	uint8 DPGIndex,
	bool bPreFog,
	int32 FirstIndex,
	int32 LastIndex
	);

/**
 * Draws a view's elements with the specified drawing policy factory type.
 * @param View - The view to draw the meshes for.
 * @param DrawingContext - The drawing policy type specific context for the drawing.
 * @param DPGIndex World or Foreground DPG index for draw order
 * @param bPreFog - true if the draw call is occurring before fog has been rendered.
 */
template<class DrawingPolicyFactoryType>
bool DrawViewElements(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	const FDrawingPolicyRenderState& DrawRenderState,
	const typename DrawingPolicyFactoryType::ContextType& DrawingContext,
	uint8 DPGIndex,
	bool bPreFog
	);

/**
* Draws a view's elements with the specified drawing policy factory type.
* @param View - The view to draw the meshes for.
* @param DrawingContext - The drawing policy type specific context for the drawing.
* @param DPGIndex World or Foreground DPG index for draw order
* @param bPreFog - true if the draw call is occurring before fog has been rendered.
* @param ParentCmdList - cmdlist to put the wait and execute task on
* @param Width - parallel width
*/
template<class DrawingPolicyFactoryType>
void DrawViewElementsParallel(
	const typename DrawingPolicyFactoryType::ContextType& DrawingContext,
	uint8 DPGIndex,
	bool bPreFog,
	FParallelCommandListSet& ParallelCommandListSet
	);

/** A primitive draw interface which adds the drawn elements to the view's batched elements. */
class FViewElementPDI : public FPrimitiveDrawInterface
{
public:

	FViewElementPDI(FViewInfo* InViewInfo,FHitProxyConsumer* InHitProxyConsumer);

	// FPrimitiveDrawInterface interface.
	virtual bool IsHitTesting() override;
	virtual void SetHitProxy(HHitProxy* HitProxy) override;
	virtual void RegisterDynamicResource(FDynamicPrimitiveResource* DynamicResource) override;
	virtual void AddReserveLines(uint8 DepthPriorityGroup, int32 NumLines, bool bDepthBiased = false, bool bThickLines = false) override;
	virtual void DrawSprite(
		const FVector& Position,
		float SizeX,
		float SizeY,
		const FTexture* Sprite,
		const FLinearColor& Color,
		uint8 DepthPriorityGroup,
		float U,
		float UL,
		float V,
		float VL,
		uint8 BlendMode = SE_BLEND_Masked
		) override;
	virtual void DrawLine(
		const FVector& Start,
		const FVector& End,
		const FLinearColor& Color,
		uint8 DepthPriorityGroup,
		float Thickness = 0.0f,
		float DepthBias = 0.0f,
		bool bScreenSpace = false
		) override;
	virtual void DrawPoint(
		const FVector& Position,
		const FLinearColor& Color,
		float PointSize,
		uint8 DepthPriorityGroup
		) override;
	virtual int32 DrawMesh(const FMeshBatch& Mesh) override;

private:
	FViewInfo* ViewInfo;
	TRefCountPtr<HHitProxy> CurrentHitProxy;
	FHitProxyConsumer* HitProxyConsumer;

	/** Depending of the DPG we return a different FBatchedElement instance. */
	FBatchedElements& GetElements(uint8 DepthPriorityGroup) const;
};

#include "DynamicPrimitiveDrawing.inl"


// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DynamicPrimitiveDrawing.inl: Dynamic primitive drawing implementation.
=============================================================================*/

#pragma once

#include "CoreTypes.h"
#include "CoreFwd.h"

class FBatchedElements;
class FDynamicPrimitiveResource;
class FHitProxyConsumer;
class FHitProxyId;
class FParallelCommandListSet;
class FPrimitiveDrawInterface;
class FRenderTask;
class FRHICommandList;
class FTexture;
class FViewElementPDI;
class FViewInfo;
class HHitProxy;
struct FMath;
struct FMeshBatch;
struct FMeshBatchElement;
struct TStatId;
template<typename TTask> class TGraphTask;

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
	)
{
	// Get the correct element list based on dpg index
	const TIndirectArray<FMeshBatch>& ViewMeshElementList = ( DPGIndex == SDPG_Foreground ? View.TopViewMeshElements : View.ViewMeshElements );
	// Draw the view's mesh elements.
	check(LastIndex < ViewMeshElementList.Num());
	for (int32 MeshIndex = FirstIndex; MeshIndex <= LastIndex; MeshIndex++)
	{
		const FMeshBatch& Mesh = ViewMeshElementList[MeshIndex];
		const auto FeatureLevel = View.GetFeatureLevel();
		check(Mesh.MaterialRenderProxy);
		check(Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel));
		const bool bIsTwoSided = Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel)->IsTwoSided();
		int32 bBackFace = bIsTwoSided ? 1 : 0;
		do
		{
			FDrawingPolicyRenderState DrawRenderStateLocal(DrawRenderState);
			DrawRenderStateLocal.ModifyViewOverrideFlags() ^= (bBackFace != 0) ? EDrawingPolicyOverrideFlags::ReverseCullMode : EDrawingPolicyOverrideFlags::None;

			DrawingPolicyFactoryType::DrawDynamicMesh(
				RHICmdList, 
				View,
				DrawingContext,
				Mesh,
				bPreFog,
				DrawRenderStateLocal,
				NULL,
				Mesh.BatchHitProxyId
				);
			--bBackFace;
		} while( bBackFace >= 0 );
	}
}

template<typename DrawingPolicyFactoryType>
class FDrawViewElementsAnyThreadTask : public FRenderTask
{
	FRHICommandList& RHICmdList;
	const FViewInfo& View;
	FDrawingPolicyRenderState DrawRenderState;
	const typename DrawingPolicyFactoryType::ContextType& DrawingContext;
	uint8 DPGIndex;
	bool bPreFog;


	const int32 FirstIndex;
	const int32 LastIndex;

public:

	FDrawViewElementsAnyThreadTask(
		FRHICommandList& InRHICmdList,
		const FViewInfo& InView,
		const FDrawingPolicyRenderState& InDrawRenderState,
		const typename DrawingPolicyFactoryType::ContextType& InDrawingContext,
		uint8 InDPGIndex,
		bool InbPreFog,
		int32 InFirstIndex,
		int32 InLastIndex
		)
		: RHICmdList(InRHICmdList)
		, View(InView)
		, DrawRenderState(InDrawRenderState)
		, DrawingContext(InDrawingContext)
		, DPGIndex(InDPGIndex)
		, bPreFog(InbPreFog)
		, FirstIndex(InFirstIndex)
		, LastIndex(InLastIndex)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FDrawViewElementsAnyThreadTask, STATGROUP_TaskGraphTasks);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		DrawViewElementsInner<DrawingPolicyFactoryType>(RHICmdList, View, DrawRenderState, DrawingContext, DPGIndex, bPreFog, FirstIndex, LastIndex);
		RHICmdList.HandleRTThreadTaskCompletion(MyCompletionGraphEvent);
	}
};

template<class DrawingPolicyFactoryType>
void DrawViewElementsParallel(
	const typename DrawingPolicyFactoryType::ContextType& DrawingContext,
	uint8 DPGIndex,
	bool bPreFog,
	FParallelCommandListSet& ParallelCommandListSet
	)
{
	// Get the correct element list based on dpg index
	const TIndirectArray<FMeshBatch>& ViewMeshElementList = (DPGIndex == SDPG_Foreground ? ParallelCommandListSet.View.TopViewMeshElements : ParallelCommandListSet.View.ViewMeshElements);

	{
		int32 NumPrims = ViewMeshElementList.Num();
		int32 EffectiveThreads = FMath::Min<int32>(FMath::DivideAndRoundUp(NumPrims, ParallelCommandListSet.MinDrawsPerCommandList), ParallelCommandListSet.Width);

		int32 Start = 0;
		if (EffectiveThreads)
		{

			int32 NumPer = NumPrims / EffectiveThreads;
			int32 Extra = NumPrims - NumPer * EffectiveThreads;


			for (int32 ThreadIndex = 0; ThreadIndex < EffectiveThreads; ThreadIndex++)
			{
				int32 Last = Start + (NumPer - 1) + (ThreadIndex < Extra);
				check(Last >= Start);

				{
					FRHICommandList* CmdList = ParallelCommandListSet.NewParallelCommandList();

					FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FDrawViewElementsAnyThreadTask<DrawingPolicyFactoryType> >::CreateTask(ParallelCommandListSet.GetPrereqs(), ENamedThreads::RenderThread)
						.ConstructAndDispatchWhenReady(*CmdList, ParallelCommandListSet.View, ParallelCommandListSet.DrawRenderState, DrawingContext, DPGIndex, bPreFog, Start, Last);

					ParallelCommandListSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent, Last - Start + 1);
				}
				Start = Last + 1;
			}
		}
	}
}

template<class DrawingPolicyFactoryType>
bool DrawViewElements(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	const FDrawingPolicyRenderState& DrawRenderState,
	const typename DrawingPolicyFactoryType::ContextType& DrawingContext,
	uint8 DPGIndex,
	bool bPreFog
	)
{
	// Get the correct element list based on dpg index
	const TIndirectArray<FMeshBatch>& ViewMeshElementList = (DPGIndex == SDPG_Foreground ? View.TopViewMeshElements : View.ViewMeshElements);
	if (ViewMeshElementList.Num() != 0)
	{
		DrawViewElementsInner<DrawingPolicyFactoryType>(RHICmdList, View, DrawRenderState, DrawingContext, DPGIndex, bPreFog, 0, ViewMeshElementList.Num() - 1);
		return true;
	}
	return false;
}

inline FViewElementPDI::FViewElementPDI(FViewInfo* InViewInfo,FHitProxyConsumer* InHitProxyConsumer):
	FPrimitiveDrawInterface(InViewInfo),
	ViewInfo(InViewInfo),
	HitProxyConsumer(InHitProxyConsumer)
{}

inline bool FViewElementPDI::IsHitTesting()
{
	return HitProxyConsumer != NULL;
}
inline void FViewElementPDI::SetHitProxy(HHitProxy* HitProxy)
{
	// Change the current hit proxy.
	CurrentHitProxy = HitProxy;

	if(HitProxyConsumer && HitProxy)
	{
		// Notify the hit proxy consumer of the new hit proxy.
		HitProxyConsumer->AddHitProxy(HitProxy);
	}
}

inline void FViewElementPDI::RegisterDynamicResource(FDynamicPrimitiveResource* DynamicResource)
{
	if (IsInGameThread())
	{
		// Render thread might be reading the array while we are adding in the game thread
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(AddViewInfoDynamicResource,
			FViewInfo*, InViewInfo, ViewInfo,
			FDynamicPrimitiveResource*, InDynamicResource, DynamicResource,
			{
				InViewInfo->DynamicResources.Add(InDynamicResource);
			});
	}
	else
	{
		ViewInfo->DynamicResources.Add(DynamicResource);
	}
}

inline FBatchedElements& FViewElementPDI::GetElements(uint8 DepthPriorityGroup) const
{
	return DepthPriorityGroup ? ViewInfo->TopBatchedViewElements : ViewInfo->BatchedViewElements;
}

inline void FViewElementPDI::DrawSprite(
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
	uint8 BlendMode
	)
{
	FBatchedElements &Elements = GetElements(DepthPriorityGroup);

	Elements.AddSprite(
		Position,
		SizeX,
		SizeY,
		Sprite,
		Color,
		CurrentHitProxy ? CurrentHitProxy->Id : FHitProxyId(),
		U,UL,V,VL,
		BlendMode
	);
}

inline void FViewElementPDI::AddReserveLines(uint8 DepthPriorityGroup, int32 NumLines, bool bDepthBiased, bool bThickLines)
{
	FBatchedElements& Elements = GetElements(DepthPriorityGroup);

	Elements.AddReserveLines(NumLines, bDepthBiased, bThickLines);
}

inline void FViewElementPDI::DrawLine(
	const FVector& Start,
	const FVector& End,
	const FLinearColor& Color,
	uint8 DepthPriorityGroup,
	float Thickness,
	float DepthBias,
	bool bScreenSpace
	)
{
	FBatchedElements &Elements = GetElements(DepthPriorityGroup);

	Elements.AddLine(
		Start,
		End,
		Color,
		CurrentHitProxy ? CurrentHitProxy->Id : FHitProxyId(),
		Thickness,
		DepthBias,
		bScreenSpace
	);
}

inline void FViewElementPDI::DrawPoint(
	const FVector& Position,
	const FLinearColor& Color,
	float PointSize,
	uint8 DepthPriorityGroup
	)
{
	float ScaledPointSize = PointSize;

	bool bIsPerspective = (ViewInfo->ViewMatrices.GetProjectionMatrix().M[3][3] < 1.0f) ? true : false;
	if( !bIsPerspective )
	{
		const float ZoomFactor = FMath::Min<float>(View->ViewMatrices.GetProjectionMatrix().M[0][0], View->ViewMatrices.GetProjectionMatrix().M[1][1]);
		ScaledPointSize = ScaledPointSize / ZoomFactor;
	}

	FBatchedElements &Elements = GetElements(DepthPriorityGroup);

	Elements.AddPoint(
		Position,
		ScaledPointSize,
		Color,
		CurrentHitProxy ? CurrentHitProxy->Id : FHitProxyId()
	);
}

inline bool MeshBatchHasPrimitives(const FMeshBatch& Mesh)
{
	bool bHasPrimitives = true;
	const int32 NumElements = Mesh.Elements.Num();
	for (int32 ElementIndex = 0; ElementIndex < NumElements; ++ElementIndex)
	{
		const FMeshBatchElement& Element = Mesh.Elements[ElementIndex];
		bHasPrimitives = bHasPrimitives && Element.NumPrimitives > 0 && Element.NumInstances > 0;
	}
	return bHasPrimitives;
}

inline int32 FViewElementPDI::DrawMesh(const FMeshBatch& Mesh)
{
	if (ensure(MeshBatchHasPrimitives(Mesh)))
	{
		// Keep track of view mesh elements whether that have translucency.
		ViewInfo->bHasTranslucentViewMeshElements |= true;//Mesh.IsTranslucent() ? 1 : 0;

		uint8 DPGIndex = Mesh.DepthPriorityGroup;
		// Get the correct element list based on dpg index
		// Translucent view mesh elements in the foreground dpg are not supported yet
		TIndirectArray<FMeshBatch>& ViewMeshElementList = ( ( DPGIndex == SDPG_Foreground  ) ? ViewInfo->TopViewMeshElements : ViewInfo->ViewMeshElements );

		FMeshBatch* NewMesh = new(ViewMeshElementList) FMeshBatch(Mesh);
		if( CurrentHitProxy != nullptr )
		{
			NewMesh->BatchHitProxyId = CurrentHitProxy->Id;
		}

		return 1;
	}
	return 0;
}


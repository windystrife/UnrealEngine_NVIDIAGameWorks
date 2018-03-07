// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DrawingPolicy.cpp: Base drawing policy implementation.
=============================================================================*/

#include "DrawingPolicy.h"
#include "SceneUtils.h"
#include "SceneRendering.h"

int32 GEmitMeshDrawEvent = 0;
static FAutoConsoleVariableRef CVarEmitMeshDrawEvent(
	TEXT("r.EmitMeshDrawEvents"),
	GEmitMeshDrawEvent,
	TEXT("Emits a GPU event around each drawing policy draw call.  /n")
	TEXT("Useful for seeing stats about each draw call, however it greatly distorts total time and time per draw call."),
	ECVF_RenderThreadSafe
	);

FMeshDrawingPolicy::FMeshDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource,
	const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
	EDebugViewShaderMode InDebugViewShaderMode
	):
	VertexFactory(InVertexFactory),
	MaterialRenderProxy(InMaterialRenderProxy),
	MaterialResource(&InMaterialResource),
	MeshPrimitiveType(InOverrideSettings.MeshPrimitiveType),
	bIsDitheredLODTransitionMaterial(InMaterialResource.IsDitheredLODTransition() || !!(InOverrideSettings.MeshOverrideFlags & EDrawingPolicyOverrideFlags::DitheredLODTransition)),
	//convert from signed bool to unsigned uint32
	DebugViewShaderMode((uint32)InDebugViewShaderMode)
{
	// using this saves a virtual function call
	bool bMaterialResourceIsTwoSided = InMaterialResource.IsTwoSided();
	
	const bool bIsWireframeMaterial = InMaterialResource.IsWireframe() || !!(InOverrideSettings.MeshOverrideFlags & EDrawingPolicyOverrideFlags::Wireframe);
	MeshFillMode = bIsWireframeMaterial ? FM_Wireframe : FM_Solid;

	const bool bInTwoSidedOverride = !!(InOverrideSettings.MeshOverrideFlags & EDrawingPolicyOverrideFlags::TwoSided);
	const bool bInReverseCullModeOverride = !!(InOverrideSettings.MeshOverrideFlags & EDrawingPolicyOverrideFlags::ReverseCullMode);
	const bool bIsTwoSided = (bMaterialResourceIsTwoSided || bInTwoSidedOverride);
	const bool bMeshRenderTwoSided = bIsTwoSided || bInTwoSidedOverride;
	MeshCullMode = (bMeshRenderTwoSided) ? CM_None : (bInReverseCullModeOverride ? CM_CCW : CM_CW);

	bUsePositionOnlyVS = false;
}

void FMeshDrawingPolicy::OnlyApplyDitheredLODTransitionState(FDrawingPolicyRenderState& DrawRenderState, const FViewInfo& ViewInfo, const FStaticMesh& Mesh, const bool InAllowStencilDither)
{
	DrawRenderState.SetDitheredLODTransitionAlpha(0.0f);

	if (Mesh.bDitheredLODTransition && !InAllowStencilDither)
	{
		if (ViewInfo.StaticMeshFadeOutDitheredLODMap[Mesh.Id])
		{
			DrawRenderState.SetDitheredLODTransitionAlpha(ViewInfo.GetTemporalLODTransition());
		}
		else if (ViewInfo.StaticMeshFadeInDitheredLODMap[Mesh.Id])
		{
			DrawRenderState.SetDitheredLODTransitionAlpha(ViewInfo.GetTemporalLODTransition() - 1.0f);
		}
	}
}

void FMeshDrawingPolicy::DrawMesh(FRHICommandList& RHICmdList, const FMeshBatch& Mesh, int32 BatchElementIndex, const bool bIsInstancedStereo) const
{
	INC_DWORD_STAT(STAT_MeshDrawCalls);
	SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, MeshEvent, GEmitMeshDrawEvent != 0, TEXT("Mesh Draw"));

	const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];

	if (Mesh.UseDynamicData)
	{
		check(Mesh.DynamicVertexData);

		if (BatchElement.DynamicIndexData)
		{
			DrawIndexedPrimitiveUP(
				RHICmdList,
				Mesh.Type,
				BatchElement.MinVertexIndex,
				BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
				BatchElement.NumPrimitives,
				BatchElement.DynamicIndexData,
				BatchElement.DynamicIndexStride,
				Mesh.DynamicVertexData,
				Mesh.DynamicVertexStride
				);
		}
		else
		{
			DrawPrimitiveUP(
				RHICmdList,
				Mesh.Type,
				BatchElement.NumPrimitives,
				Mesh.DynamicVertexData,
				Mesh.DynamicVertexStride
				);
		}
	}
	else
	{
		if(BatchElement.IndexBuffer)
		{
			check(BatchElement.IndexBuffer->IsInitialized());
			if (BatchElement.bIsInstanceRuns)
			{
				checkSlow(BatchElement.bIsInstanceRuns);
				if (!GRHISupportsFirstInstance)
				{
					if (bUsePositionOnlyVS)
					{
						for (uint32 Run = 0; Run < BatchElement.NumInstances; Run++)
						{
							VertexFactory->OffsetPositionInstanceStreams(RHICmdList, BatchElement.InstanceRuns[Run * 2]); 
							RHICmdList.DrawIndexedPrimitive(
								BatchElement.IndexBuffer->IndexBufferRHI,
								Mesh.Type,
								0,
								0,
								BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
								BatchElement.FirstIndex,
								BatchElement.NumPrimitives,
								1 + BatchElement.InstanceRuns[Run * 2 + 1] - BatchElement.InstanceRuns[Run * 2]
							);
						}
					}
					else
					{
						for (uint32 Run = 0; Run < BatchElement.NumInstances; Run++)
						{
							VertexFactory->OffsetInstanceStreams(RHICmdList, BatchElement.InstanceRuns[Run * 2]); 
							RHICmdList.DrawIndexedPrimitive(
								BatchElement.IndexBuffer->IndexBufferRHI,
								Mesh.Type,
								0,
								0,
								BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
								BatchElement.FirstIndex,
								BatchElement.NumPrimitives,
								1 + BatchElement.InstanceRuns[Run * 2 + 1] - BatchElement.InstanceRuns[Run * 2]
							);
						}
					}
				}
				else
				{
					for (uint32 Run = 0; Run < BatchElement.NumInstances; Run++)
					{
						RHICmdList.DrawIndexedPrimitive(
							BatchElement.IndexBuffer->IndexBufferRHI,
							Mesh.Type,
							0,
							BatchElement.InstanceRuns[Run * 2],
							BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
							BatchElement.FirstIndex,
							BatchElement.NumPrimitives,
							1 + BatchElement.InstanceRuns[Run * 2 + 1] - BatchElement.InstanceRuns[Run * 2]
						);
					}
				}
			}
			else
			{
				// Currently only supporting this path for instanced stereo.
				const uint32 InstanceCount = (bIsInstancedStereo && !BatchElement.bIsInstancedMesh) ? 2 : BatchElement.NumInstances;

				RHICmdList.DrawIndexedPrimitive(
					BatchElement.IndexBuffer->IndexBufferRHI,
					Mesh.Type,
					0,
					0,
					BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
					BatchElement.FirstIndex,
					BatchElement.NumPrimitives,
					InstanceCount
					);
			}
		}
		else
		{
			RHICmdList.DrawPrimitive(
					Mesh.Type,
					BatchElement.FirstIndex,
					BatchElement.NumPrimitives,
					BatchElement.NumInstances
					);
		}
	}
}

void FMeshDrawingPolicy::SetSharedState(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, const FSceneView* View, const FMeshDrawingPolicy::ContextDataType PolicyContext) const
{
	check(VertexFactory && VertexFactory->IsInitialized());
	VertexFactory->Set(RHICmdList);
}

/**
* Get the decl and stream strides for this mesh policy type and vertexfactory
* @param VertexDeclaration - output decl 
*/
const FVertexDeclarationRHIRef& FMeshDrawingPolicy::GetVertexDeclaration() const
{
	check(VertexFactory && VertexFactory->IsInitialized());
	const FVertexDeclarationRHIRef& VertexDeclaration = VertexFactory->GetDeclaration();
	check(VertexFactory->NeedsDeclaration()==false || IsValidRef(VertexDeclaration));

	return VertexDeclaration;
}

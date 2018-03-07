// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TriangleRendering.cpp: Simple triangle rendering implementation.
=============================================================================*/

#include "TriangleRendering.h"
#include "ShowFlags.h"
#include "RHI.h"
#include "RenderResource.h"
#include "RenderingThread.h"
#include "VertexFactory.h"
#include "PackedNormal.h"
#include "LocalVertexFactory.h"
#include "SceneView.h"
#include "CanvasTypes.h"
#include "MeshBatch.h"
#include "RendererInterface.h"
#include "SceneUtils.h"
#include "EngineModule.h"
#include "DrawingPolicy.h"

/** 
 * vertex data for a screen triangle
 */
struct FMaterialTriangleVertex
{
	FVector			Position;
	FPackedNormal	TangentX;
	FPackedNormal	TangentZ;
	uint32			Color;
	float			U;
	float			V;

	inline void Initialize(float InX, float InY, float InU, float InV)
	{
		Position.X = InX; 
		Position.Y = InY; 
		Position.Z = 0.0f;
		TangentX = FVector(1.0f, 0.0f, 0.0f); 
		//TangentY = FVector(0.0f, 1.0f, 0.0f); 
		TangentZ = FVector(0.0f, 0.0f, 1.0f);
		// TangentZ.w contains the sign of the tangent basis determinant. Assume +1
		TangentZ.Vector.W = 255;
		Color = FColor(255,255,255,255).DWColor();
		U = InU; 
		V = InV;
	}
};

/** 
* Vertex buffer
*/
class FMaterialTriangleVertexBuffer : public FVertexBuffer
{
public:
	/** 
	* Initialize the RHI for this rendering resource 
	*/
	virtual void InitRHI() override
	{
		// used with a tristrip, so only 3 vertices are needed
		uint32 Size = 3 * sizeof(FMaterialTriangleVertex);
		// create vertex buffer
		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(Size,BUF_Static,CreateInfo, Buffer);

		// first vertex element
		FMaterialTriangleVertex* DestVertex = (FMaterialTriangleVertex*)Buffer;
		// fill out the verts
		DestVertex[0].Initialize(1, -1, 1, 1);
		DestVertex[1].Initialize(1, 1, 1, 0);
		DestVertex[2].Initialize(-1, -1, 0, 1);
			
		// Unlock the buffer.
		RHIUnlockVertexBuffer(VertexBufferRHI);        
	}
};

TGlobalResource<FMaterialTriangleVertexBuffer> GTriangleRendererVertexBuffer;

/**
* Vertex factory for rendering tiles.
*/
class FTriangleVertexFactory : public FLocalVertexFactory
{
public:

/** Default constructor. */
FTriangleVertexFactory()
{
	FLocalVertexFactory::FDataType VertexData;
	// position
	VertexData.PositionComponent = FVertexStreamComponent(
		&GTriangleRendererVertexBuffer, STRUCT_OFFSET(FMaterialTriangleVertex, Position), sizeof(FMaterialTriangleVertex), VET_Float3);
		// tangents
		VertexData.TangentBasisComponents[0] = FVertexStreamComponent(
		&GTriangleRendererVertexBuffer, STRUCT_OFFSET(FMaterialTriangleVertex, TangentX), sizeof(FMaterialTriangleVertex), VET_PackedNormal);
		VertexData.TangentBasisComponents[1] = FVertexStreamComponent(
		&GTriangleRendererVertexBuffer, STRUCT_OFFSET(FMaterialTriangleVertex, TangentZ), sizeof(FMaterialTriangleVertex), VET_PackedNormal);
		// color
		VertexData.ColorComponent = FVertexStreamComponent(
		&GTriangleRendererVertexBuffer, STRUCT_OFFSET(FMaterialTriangleVertex, Color), sizeof(FMaterialTriangleVertex), VET_Color);
		// UVs
		VertexData.TextureCoordinates.Add(FVertexStreamComponent(
		&GTriangleRendererVertexBuffer, STRUCT_OFFSET(FMaterialTriangleVertex, U), sizeof(FMaterialTriangleVertex), VET_Float2));
		
		// update the data
		SetData(VertexData);
	}
};

TGlobalResource<FTriangleVertexFactory> GTriangleVertexFactory;

/**
 * Mesh used to render triangles.
 */
class FTriangleMesh : public FRenderResource
{
public:

	/** The mesh element. */
	FMeshBatch TriMeshElement;

	virtual void InitRHI() override
	{
		FMeshBatchElement& BatchElement = TriMeshElement.Elements[0];
		TriMeshElement.VertexFactory = &GTriangleVertexFactory;
		TriMeshElement.DynamicVertexStride = sizeof(FMaterialTriangleVertex);
		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = 1;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = 2;
		TriMeshElement.ReverseCulling = false;
		TriMeshElement.bDisableBackfaceCulling = true;
		TriMeshElement.UseDynamicData = true;
		TriMeshElement.Type = PT_TriangleList;
		TriMeshElement.DepthPriorityGroup = SDPG_Foreground;
		BatchElement.PrimitiveUniformBufferResource = &GIdentityPrimitiveUniformBuffer;
	}
	
	virtual void ReleaseRHI() override
	{
		TriMeshElement.Elements[0].PrimitiveUniformBuffer.SafeRelease();
	}
};
TGlobalResource<FTriangleMesh> GTriangleMesh;

void FTriangleRenderer::DrawTriangle(FRHICommandListImmediate& RHICmdList, FDrawingPolicyRenderState& DrawRenderState, const class FSceneView& View, const FMaterialRenderProxy* MaterialRenderProxy, bool bNeedsToSwitchVerticalAxis, const FCanvasUVTri& Tri, bool bIsHitTesting, const FHitProxyId HitProxyId, const FColor InVertexColor)
{
	FMaterialTriangleVertex DestVertex[3];

	// create verts
	if (bNeedsToSwitchVerticalAxis)
	{
		DestVertex[0].Initialize(Tri.V1_Pos.X, View.ViewRect.Height() - Tri.V1_Pos.Y, Tri.V1_UV.X, Tri.V1_UV.Y);
		DestVertex[1].Initialize(Tri.V0_Pos.X, View.ViewRect.Height() - Tri.V0_Pos.Y, Tri.V0_UV.X, Tri.V0_UV.Y);
		DestVertex[2].Initialize(Tri.V2_Pos.X, View.ViewRect.Height() - Tri.V2_Pos.Y, Tri.V2_UV.X, Tri.V2_UV.Y);
	}
	else
	{
		DestVertex[0].Initialize(Tri.V1_Pos.X, Tri.V1_Pos.Y, Tri.V1_UV.X, Tri.V1_UV.Y);
		DestVertex[1].Initialize(Tri.V0_Pos.X, Tri.V0_Pos.Y, Tri.V0_UV.X, Tri.V0_UV.Y);
		DestVertex[2].Initialize(Tri.V2_Pos.X, Tri.V2_Pos.Y, Tri.V2_UV.X, Tri.V2_UV.Y);
	}
	
	DestVertex[0].Color = InVertexColor.DWColor();
	DestVertex[1].Color = InVertexColor.DWColor();
	DestVertex[2].Color = InVertexColor.DWColor();
	
	// update the FMeshBatch
	FMeshBatch& TriMesh = GTriangleMesh.TriMeshElement;
	TriMesh.UseDynamicData = true;
	TriMesh.DynamicVertexData = DestVertex;
	TriMesh.MaterialRenderProxy = MaterialRenderProxy;
	
	GetRendererModule().DrawTileMesh(RHICmdList, DrawRenderState, View, TriMesh, bIsHitTesting, HitProxyId);
}

void FTriangleRenderer::DrawTriangleUsingVertexColor(FRHICommandListImmediate& RHICmdList, FDrawingPolicyRenderState& DrawRenderState, const class FSceneView& View, const FMaterialRenderProxy* MaterialRenderProxy, bool bNeedsToSwitchVerticalAxis, const FCanvasUVTri& Tri, bool bIsHitTesting, const FHitProxyId HitProxyId)
{
	FMaterialTriangleVertex DestVertex[3];

	// create verts. Notice the order is (1, 0, 2)
	if (bNeedsToSwitchVerticalAxis)
	{
		DestVertex[0].Initialize(Tri.V1_Pos.X, View.ViewRect.Height() - Tri.V1_Pos.Y, Tri.V1_UV.X, Tri.V1_UV.Y);
		DestVertex[1].Initialize(Tri.V0_Pos.X, View.ViewRect.Height() - Tri.V0_Pos.Y, Tri.V0_UV.X, Tri.V0_UV.Y);
		DestVertex[2].Initialize(Tri.V2_Pos.X, View.ViewRect.Height() - Tri.V2_Pos.Y, Tri.V2_UV.X, Tri.V2_UV.Y);
	}
	else
	{
		DestVertex[0].Initialize(Tri.V1_Pos.X, Tri.V1_Pos.Y, Tri.V1_UV.X, Tri.V1_UV.Y);
		DestVertex[1].Initialize(Tri.V0_Pos.X, Tri.V0_Pos.Y, Tri.V0_UV.X, Tri.V0_UV.Y);
		DestVertex[2].Initialize(Tri.V2_Pos.X, Tri.V2_Pos.Y, Tri.V2_UV.X, Tri.V2_UV.Y);
	}

	// Notice the order is (1, 0, 2)
	DestVertex[0].Color = Tri.V1_Color.ToFColor(true).DWColor();
	DestVertex[1].Color = Tri.V0_Color.ToFColor(true).DWColor();
	DestVertex[2].Color = Tri.V2_Color.ToFColor(true).DWColor();

	// update the FMeshBatch
	FMeshBatch& TriMesh = GTriangleMesh.TriMeshElement;
	TriMesh.UseDynamicData = true;
	TriMesh.DynamicVertexData = DestVertex;
	TriMesh.MaterialRenderProxy = MaterialRenderProxy;

	GetRendererModule().DrawTileMesh(RHICmdList, DrawRenderState, View, TriMesh, bIsHitTesting, HitProxyId);
}

bool FCanvasTriangleRendererItem::Render_RenderThread(FRHICommandListImmediate& RHICmdList, FDrawingPolicyRenderState& DrawRenderState, const FCanvas* Canvas)
{
	float CurrentRealTime = 0.f;
	float CurrentWorldTime = 0.f;
	float DeltaWorldTime = 0.f;

	if (!bFreezeTime)
	{
		CurrentRealTime = Canvas->GetCurrentRealTime();
		CurrentWorldTime = Canvas->GetCurrentWorldTime();
		DeltaWorldTime = Canvas->GetCurrentDeltaWorldTime();
	}

	checkSlow(Data);

	// current render target set for the canvas
	const FRenderTarget* CanvasRenderTarget = Canvas->GetRenderTarget();
	FSceneViewFamily* ViewFamily = new FSceneViewFamily(FSceneViewFamily::ConstructionValues(
		CanvasRenderTarget,
		nullptr,
		FEngineShowFlags(ESFIM_Game))
		.SetWorldTimes(CurrentWorldTime, DeltaWorldTime, CurrentRealTime)
		.SetGammaCorrection(CanvasRenderTarget->GetDisplayGamma()));

	FIntRect ViewRect(FIntPoint(0, 0), CanvasRenderTarget->GetSizeXY());

	// make a temporary viewk

	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = ViewFamily;
	ViewInitOptions.SetViewRectangle(ViewRect);
	ViewInitOptions.ViewOrigin = FVector::ZeroVector;
	ViewInitOptions.ViewRotationMatrix = FMatrix::Identity;
	ViewInitOptions.ProjectionMatrix = Data->Transform.GetMatrix();
	ViewInitOptions.BackgroundColor = FLinearColor::Black;
	ViewInitOptions.OverlayColor = FLinearColor::White;

	bool bNeedsToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(Canvas->GetShaderPlatform()) && XOR(IsMobileHDR(), Canvas->GetAllowSwitchVerticalAxis());

	FSceneView* View = new FSceneView(ViewInitOptions);

	for (int32 TriIdx = 0; TriIdx < Data->Triangles.Num(); TriIdx++)
	{
		const FRenderData::FTriangleInst& Tri = Data->Triangles[TriIdx];
		FTriangleRenderer::DrawTriangleUsingVertexColor(
			RHICmdList,
			DrawRenderState,
			*View,
			Data->MaterialRenderProxy,
			bNeedsToSwitchVerticalAxis,
			Tri.Tri,
			Canvas->IsHitTesting(), Tri.HitProxyId
			);
	}

	delete View->Family;
	delete View;
	if (Canvas->GetAllowedModes() & FCanvas::Allow_DeleteOnRender)
	{
		delete Data;
	}
	if (Canvas->GetAllowedModes() & FCanvas::Allow_DeleteOnRender)
	{
		Data = nullptr;
	}
	return true;
}

bool FCanvasTriangleRendererItem::Render_GameThread(const FCanvas* Canvas)
{
	float CurrentRealTime = 0.f;
	float CurrentWorldTime = 0.f;
	float DeltaWorldTime = 0.f;

	if (!bFreezeTime)
	{
		CurrentRealTime = Canvas->GetCurrentRealTime();
		CurrentWorldTime = Canvas->GetCurrentWorldTime();
		DeltaWorldTime = Canvas->GetCurrentDeltaWorldTime();
	}

	checkSlow(Data);
	// current render target set for the canvas
	const FRenderTarget* CanvasRenderTarget = Canvas->GetRenderTarget();
	FSceneViewFamily* ViewFamily = new FSceneViewFamily(FSceneViewFamily::ConstructionValues(
		CanvasRenderTarget,
		Canvas->GetScene(),
		FEngineShowFlags(ESFIM_Game))
		.SetWorldTimes(CurrentWorldTime, DeltaWorldTime, CurrentRealTime)
		.SetGammaCorrection(CanvasRenderTarget->GetDisplayGamma()));

	FIntRect ViewRect(FIntPoint(0, 0), CanvasRenderTarget->GetSizeXY());

	// make a temporary view
	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = ViewFamily;
	ViewInitOptions.SetViewRectangle(ViewRect);
	ViewInitOptions.ViewOrigin = FVector::ZeroVector;
	ViewInitOptions.ViewRotationMatrix = FMatrix::Identity;
	ViewInitOptions.ProjectionMatrix = Data->Transform.GetMatrix();
	ViewInitOptions.BackgroundColor = FLinearColor::Black;
	ViewInitOptions.OverlayColor = FLinearColor::White;

	FSceneView* View = new FSceneView(ViewInitOptions);

	bool bNeedsToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(Canvas->GetShaderPlatform()) && XOR(IsMobileHDR(),Canvas->GetAllowSwitchVerticalAxis());

	struct FDrawTriangleParameters
	{
		FSceneView* View;
		FRenderData* RenderData;
		uint32 bIsHitTesting : 1;
		uint32 bNeedsToSwitchVerticalAxis : 1;
		uint32 AllowedCanvasModes;
	};
	FDrawTriangleParameters DrawTriangleParameters =
	{
		View,
		Data,
		(uint32)Canvas->IsHitTesting(),
		(uint32)bNeedsToSwitchVerticalAxis,
		Canvas->GetAllowedModes()
	};
	FDrawTriangleParameters Parameters = DrawTriangleParameters;
	ENQUEUE_RENDER_COMMAND(DrawTriangleCommand)(
		[Parameters](FRHICommandListImmediate& RHICmdList)	
		{
			FDrawingPolicyRenderState DrawRenderState(*Parameters.View);

			// disable depth test & writes
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

			SCOPED_DRAW_EVENT(RHICmdList, CanvasDrawTriangle);
			for (int32 TriIdx = 0; TriIdx < Parameters.RenderData->Triangles.Num(); TriIdx++)
			{
				const FRenderData::FTriangleInst& Tri = Parameters.RenderData->Triangles[TriIdx];
				FTriangleRenderer::DrawTriangleUsingVertexColor(
					RHICmdList,
					DrawRenderState,
					*Parameters.View,
					Parameters.RenderData->MaterialRenderProxy,
					Parameters.bNeedsToSwitchVerticalAxis,
					Tri.Tri,
					Parameters.bIsHitTesting, Tri.HitProxyId);
			}

			delete Parameters.View->Family;
			delete Parameters.View;
			if (Parameters.AllowedCanvasModes & FCanvas::Allow_DeleteOnRender)
			{
				delete Parameters.RenderData;
			}
		});
	if (Canvas->GetAllowedModes() & FCanvas::Allow_DeleteOnRender)
	{
		Data = nullptr;
	}
	return true;
}

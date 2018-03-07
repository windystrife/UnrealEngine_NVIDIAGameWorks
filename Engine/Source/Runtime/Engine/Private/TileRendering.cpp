// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TileRendering.cpp: Tile rendering implementation.
=============================================================================*/

#include "TileRendering.h"
#include "RHI.h"
#include "ShowFlags.h"
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

#define NUM_MATERIAL_TILE_VERTS	6

/** 
* vertex data for a screen quad 
*/
struct FMaterialTileVertex
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
		TangentX = FVector(1, 0, 0); 
		//TangentY = FVector(0, 1, 0); 
		TangentZ = FVector(0, 0, 1);
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
class FMaterialTileVertexBuffer : public FVertexBuffer
{
public:
	/** 
	* Initialize the RHI for this rendering resource 
	*/
	virtual void InitRHI() override
	{
		// used with a trilist, so 6 vertices are needed
		uint32 Size = NUM_MATERIAL_TILE_VERTS * sizeof(FMaterialTileVertex);
		// create vertex buffer
		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(Size,BUF_Static,CreateInfo, Buffer);
				
		// first vertex element
		FMaterialTileVertex* DestVertex = (FMaterialTileVertex*)Buffer;

		// fill out the verts
		DestVertex[0].Initialize(1, -1, 1, 1);
		DestVertex[1].Initialize(1, 1, 1, 0);
		DestVertex[2].Initialize(-1, -1, 0, 1);
		DestVertex[3].Initialize(-1, -1, 0, 1);
		DestVertex[4].Initialize(1, 1, 1, 0);	
		DestVertex[5].Initialize(-1, 1, 0, 0);

		// Unlock the buffer.
		RHIUnlockVertexBuffer(VertexBufferRHI);        
	}
};
TGlobalResource<FMaterialTileVertexBuffer> GTileRendererVertexBuffer;

/**
 * Vertex factory for rendering tiles.
 */
class FTileVertexFactory : public FLocalVertexFactory
{
public:

	/** Default constructor. */
	FTileVertexFactory()
	{
		FLocalVertexFactory::FDataType VertexData;
		// position
		VertexData.PositionComponent = FVertexStreamComponent(
			&GTileRendererVertexBuffer,STRUCT_OFFSET(FMaterialTileVertex,Position),sizeof(FMaterialTileVertex),VET_Float3);
		// tangents
		VertexData.TangentBasisComponents[0] = FVertexStreamComponent(
			&GTileRendererVertexBuffer,STRUCT_OFFSET(FMaterialTileVertex,TangentX),sizeof(FMaterialTileVertex),VET_PackedNormal);
		VertexData.TangentBasisComponents[1] = FVertexStreamComponent(
			&GTileRendererVertexBuffer,STRUCT_OFFSET(FMaterialTileVertex,TangentZ),sizeof(FMaterialTileVertex),VET_PackedNormal);
		// color
		VertexData.ColorComponent = FVertexStreamComponent(
			&GTileRendererVertexBuffer,STRUCT_OFFSET(FMaterialTileVertex,Color),sizeof(FMaterialTileVertex),VET_Color);
		// UVs
		VertexData.TextureCoordinates.Add(FVertexStreamComponent(
			&GTileRendererVertexBuffer,STRUCT_OFFSET(FMaterialTileVertex,U),sizeof(FMaterialTileVertex),VET_Float2));

		// update the data
		SetData(VertexData);
	}
};
TGlobalResource<FTileVertexFactory> GTileVertexFactory;

/**
 * Mesh used to render tiles.
 */
class FTileMesh : public FRenderResource
{
public:

	/** The mesh element. */
	FMeshBatch MeshElement;

	virtual void InitRHI() override
	{
		FMeshBatchElement& BatchElement = MeshElement.Elements[0];
		MeshElement.VertexFactory = &GTileVertexFactory;
		MeshElement.DynamicVertexStride = sizeof(FMaterialTileVertex);
		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = 2;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = NUM_MATERIAL_TILE_VERTS - 1;
		MeshElement.ReverseCulling = false;
		MeshElement.UseDynamicData = true;
		MeshElement.Type = PT_TriangleList;
		MeshElement.DepthPriorityGroup = SDPG_Foreground;
		BatchElement.PrimitiveUniformBufferResource = &GIdentityPrimitiveUniformBuffer;
	}

	virtual void ReleaseRHI() override
	{
		MeshElement.Elements[0].PrimitiveUniformBuffer.SafeRelease();
	}
};
TGlobalResource<FTileMesh> GTileMesh;

static void CreateTileVertices(FMaterialTileVertex DestVertex[NUM_MATERIAL_TILE_VERTS], const class FSceneView& View, bool bNeedsToSwitchVerticalAxis, float X, float Y, float SizeX, float SizeY, float U, float V, float SizeU, float SizeV, const FColor InVertexColor)
{
	static_assert(NUM_MATERIAL_TILE_VERTS == 6, "Invalid tile tri-list size.");

	if (bNeedsToSwitchVerticalAxis)
	{
		DestVertex[0].Initialize(X + SizeX, View.ViewRect.Height() - (Y + SizeY), U + SizeU, V + SizeV);
		DestVertex[1].Initialize(X, View.ViewRect.Height() - (Y + SizeY), U, V + SizeV);
		DestVertex[2].Initialize(X + SizeX, View.ViewRect.Height() - Y, U + SizeU, V);
		DestVertex[3].Initialize(X + SizeX, View.ViewRect.Height() - Y, U + SizeU, V);
		DestVertex[4].Initialize(X, View.ViewRect.Height() - (Y + SizeY), U, V + SizeV);
		DestVertex[5].Initialize(X, View.ViewRect.Height() - Y, U, V);
	}
	else
	{
		DestVertex[0].Initialize(X + SizeX, Y, U + SizeU, V);
		DestVertex[1].Initialize(X, Y, U, V);
		DestVertex[2].Initialize(X + SizeX, Y + SizeY, U + SizeU, V + SizeV);
		DestVertex[3].Initialize(X + SizeX, Y + SizeY, U + SizeU, V + SizeV);
		DestVertex[4].Initialize(X, Y, U, V);	
		DestVertex[5].Initialize(X, Y + SizeY, U, V + SizeV);
	}

	DestVertex[0].Color = InVertexColor.DWColor();
	DestVertex[1].Color = InVertexColor.DWColor();
	DestVertex[2].Color = InVertexColor.DWColor();
	DestVertex[3].Color = InVertexColor.DWColor();
	DestVertex[4].Color = InVertexColor.DWColor();
	DestVertex[5].Color = InVertexColor.DWColor();
}

void FTileRenderer::DrawTile(FRHICommandListImmediate& RHICmdList, FDrawingPolicyRenderState& DrawRenderState, const class FSceneView& View, const FMaterialRenderProxy* MaterialRenderProxy, bool bNeedsToSwitchVerticalAxis, float X, float Y, float SizeX, float SizeY, float U, float V, float SizeU, float SizeV, bool bIsHitTesting, const FHitProxyId HitProxyId, const FColor InVertexColor)
{
	// create verts
	FMaterialTileVertex DestVertex[NUM_MATERIAL_TILE_VERTS];
	CreateTileVertices(DestVertex, View, bNeedsToSwitchVerticalAxis, X, Y, SizeX, SizeY, U, V, SizeU, SizeV, InVertexColor);

	// update the FMeshBatch
	FMeshBatch& Mesh = GTileMesh.MeshElement;
	Mesh.UseDynamicData = true;
	Mesh.DynamicVertexData = DestVertex;
	Mesh.MaterialRenderProxy = MaterialRenderProxy;

	GetRendererModule().DrawTileMesh(RHICmdList, DrawRenderState, View, Mesh, bIsHitTesting, HitProxyId);
}

void FTileRenderer::DrawRotatedTile(FRHICommandListImmediate& RHICmdList, FDrawingPolicyRenderState& DrawRenderState, const class FSceneView& View, const FMaterialRenderProxy* MaterialRenderProxy, bool bNeedsToSwitchVerticalAxis, const FQuat& Rotation, float X, float Y, float SizeX, float SizeY, float U, float V, float SizeU, float SizeV, bool bIsHitTesting, const FHitProxyId HitProxyId, const FColor InVertexColor)
{
	// create verts
	FMaterialTileVertex DestVertex[NUM_MATERIAL_TILE_VERTS];
	CreateTileVertices(DestVertex, View, bNeedsToSwitchVerticalAxis, X, Y, SizeX, SizeY, U, V, SizeU, SizeV, InVertexColor);
	
	// rotate tile using view center as origin
	FIntPoint ViewRectSize = View.ViewRect.Size();
	FVector ViewRectOrigin = FVector(ViewRectSize.X*0.5f, ViewRectSize.Y*0.5f, 0.f);
	for (int32 i = 0; i < NUM_MATERIAL_TILE_VERTS; ++i)
	{
		DestVertex[i].Position = Rotation.RotateVector(DestVertex[i].Position - ViewRectOrigin) + ViewRectOrigin;
		DestVertex[i].TangentX = Rotation.RotateVector(DestVertex[i].TangentX);
		DestVertex[i].TangentZ = Rotation.RotateVector(DestVertex[i].TangentZ);
	}

	// update the FMeshBatch
	FMeshBatch& Mesh = GTileMesh.MeshElement;
	Mesh.UseDynamicData = true;
	Mesh.DynamicVertexData = DestVertex;
	Mesh.MaterialRenderProxy = MaterialRenderProxy;

	GetRendererModule().DrawTileMesh(RHICmdList, DrawRenderState, View, Mesh, bIsHitTesting, HitProxyId);
}

bool FCanvasTileRendererItem::Render_RenderThread(FRHICommandListImmediate& RHICmdList, FDrawingPolicyRenderState& DrawRenderState, const FCanvas* Canvas)
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
	
	bool bNeedsToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(Canvas->GetShaderPlatform()) && IsMobileHDR();

	for (int32 TileIdx = 0; TileIdx < Data->Tiles.Num(); TileIdx++)
	{
		const FRenderData::FTileInst& Tile = Data->Tiles[TileIdx];
		FTileRenderer::DrawTile(
			RHICmdList,
			DrawRenderState,
			*View,
			Data->MaterialRenderProxy,
			bNeedsToSwitchVerticalAxis,
			Tile.X, Tile.Y, Tile.SizeX, Tile.SizeY,
			Tile.U, Tile.V, Tile.SizeU, Tile.SizeV,
			Canvas->IsHitTesting(), Tile.HitProxyId,
			Tile.InColor
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
		Data = NULL;
	}
	return true;
}

bool FCanvasTileRendererItem::Render_GameThread(const FCanvas* Canvas)
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

	bool bNeedsToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(Canvas->GetShaderPlatform()) && IsMobileHDR();

	struct FDrawTileParameters
	{
		FSceneView* View;
		FRenderData* RenderData;
		uint32 bIsHitTesting : 1;
		uint32 bNeedsToSwitchVerticalAxis : 1;
		uint32 AllowedCanvasModes;
	};
	FDrawTileParameters DrawTileParameters =
	{
		View,
		Data,
		Canvas->IsHitTesting() ? uint32(1) : uint32(0),
		bNeedsToSwitchVerticalAxis ? uint32(1) : uint32(0),
		Canvas->GetAllowedModes()
	};
	ENQUEUE_RENDER_COMMAND(DrawTileCommand)(
		[DrawTileParameters](FRHICommandListImmediate& RHICmdList)
		{
			SCOPED_DRAW_EVENTF(RHICmdList, CanvasDrawTile, *DrawTileParameters.RenderData->MaterialRenderProxy->GetMaterial(GMaxRHIFeatureLevel)->GetFriendlyName());
			
			
			FDrawingPolicyRenderState DrawRenderState(*DrawTileParameters.View);

			// disable depth test & writes
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

			for (int32 TileIdx = 0; TileIdx < DrawTileParameters.RenderData->Tiles.Num(); TileIdx++)
			{
				const FRenderData::FTileInst& Tile = DrawTileParameters.RenderData->Tiles[TileIdx];
				FTileRenderer::DrawTile(
					RHICmdList,
					DrawRenderState,
					*DrawTileParameters.View,
					DrawTileParameters.RenderData->MaterialRenderProxy,
					DrawTileParameters.bNeedsToSwitchVerticalAxis,
					Tile.X, Tile.Y, Tile.SizeX, Tile.SizeY,
					Tile.U, Tile.V, Tile.SizeU, Tile.SizeV,
					DrawTileParameters.bIsHitTesting, Tile.HitProxyId,
					Tile.InColor);
			}

			delete DrawTileParameters.View->Family;
			delete DrawTileParameters.View;
			if (DrawTileParameters.AllowedCanvasModes & FCanvas::Allow_DeleteOnRender)
			{
				delete DrawTileParameters.RenderData;
			}
		});
	if (Canvas->GetAllowedModes() & FCanvas::Allow_DeleteOnRender)
	{
		Data = NULL;
	}
	return true;
}

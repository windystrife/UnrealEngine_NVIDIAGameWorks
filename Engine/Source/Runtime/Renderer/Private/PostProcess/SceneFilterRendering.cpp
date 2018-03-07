// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneFilterRendering.cpp: Filter rendering implementation.
=============================================================================*/

#include "PostProcess/SceneFilterRendering.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"

/**
* Static vertex and index buffer used for 2D screen rectangles.
*/
class FScreenRectangleVertexBuffer : public FVertexBuffer
{
public:
	/** Initialize the RHI for this rendering resource */
	void InitRHI() override
	{
		TResourceArray<FFilterVertex, VERTEXBUFFER_ALIGNMENT> Vertices;
		Vertices.SetNumUninitialized(6);

		Vertices[0].Position = FVector4(1,  1,	0,	1);
		Vertices[0].UV = FVector2D(1,	1);

		Vertices[1].Position = FVector4(0,  1,	0,	1);
		Vertices[1].UV = FVector2D(0,	1);

		Vertices[2].Position = FVector4(1,	0,	0,	1);
		Vertices[2].UV = FVector2D(1,	0);

		Vertices[3].Position = FVector4(0,	0,	0,	1);
		Vertices[3].UV = FVector2D(0,	0);

		//The final two vertices are used for the triangle optimization (a single triangle spans the entire viewport )
		Vertices[4].Position = FVector4(-1,  1,	0,	1);
		Vertices[4].UV = FVector2D(-1,	1);

		Vertices[5].Position = FVector4(1,  -1,	0,	1);
		Vertices[5].UV = FVector2D(1, -1);

		// Create vertex buffer. Fill buffer with initial data upon creation
		FRHIResourceCreateInfo CreateInfo(&Vertices);
		VertexBufferRHI = RHICreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static, CreateInfo);
	}
};

class FScreenRectangleIndexBuffer : public FIndexBuffer
{
public:
	/** Initialize the RHI for this rendering resource */
	void InitRHI() override
	{
		// Indices 0 - 5 are used for rendering a quad. Indices 6 - 8 are used for triangle optimization.
		const uint16 Indices[] = { 0, 1, 2, 2, 1, 3, 0, 4, 5 };

		TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> IndexBuffer;
		uint32 NumIndices = ARRAY_COUNT(Indices);
		IndexBuffer.AddUninitialized(NumIndices);
		FMemory::Memcpy(IndexBuffer.GetData(), Indices, NumIndices * sizeof(uint16));

		// Create index buffer. Fill buffer with initial data upon creation
		FRHIResourceCreateInfo CreateInfo(&IndexBuffer);
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), IndexBuffer.GetResourceDataSize(), BUF_Static, CreateInfo);
	}
};

/** Global resource  */
static TGlobalResource<FScreenRectangleVertexBuffer> GScreenRectangleVertexBuffer;
static TGlobalResource<FScreenRectangleIndexBuffer> GScreenRectangleIndexBuffer;

void FTesselatedScreenRectangleIndexBuffer::InitRHI()
{
	TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> IndexBuffer;

	uint32 NumIndices = NumPrimitives() * 3;
	IndexBuffer.AddUninitialized(NumIndices);

	uint16* Out = (uint16*)IndexBuffer.GetData();

	for(uint32 y = 0; y < Height; ++y)
	{
		for(uint32 x = 0; x < Width; ++x)
		{
			// left top to bottom right in reading order
			uint16 Index00 = x  + y * (Width + 1);
			uint16 Index10 = Index00 + 1;
			uint16 Index01 = Index00 + (Width + 1);
			uint16 Index11 = Index01 + 1;

			// todo: diagonal can be flipped on parts of the screen

			// triangle A
			*Out++ = Index00; *Out++ = Index01; *Out++ = Index10;

			// triangle B
			*Out++ = Index11; *Out++ = Index10; *Out++ = Index01;
		}
	}

	// Create index buffer. Fill buffer with initial data upon creation
	FRHIResourceCreateInfo CreateInfo(&IndexBuffer);
	IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), IndexBuffer.GetResourceDataSize(), BUF_Static, CreateInfo);
}

uint32 FTesselatedScreenRectangleIndexBuffer::NumVertices() const
{
	// 4 vertices per quad but shared
	return (Width + 1) * (Height + 1);
}

uint32 FTesselatedScreenRectangleIndexBuffer::NumPrimitives() const
{
	// triangle has 3 corners, 2 triangle per quad
	return 2 * Width * Height;
}

/** We don't need a vertex buffer as we can compute the vertex attributes in the VS */
static TGlobalResource<FTesselatedScreenRectangleIndexBuffer> GTesselatedScreenRectangleIndexBuffer;


/** Vertex declaration for the 2D screen rectangle. */
TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;
/** Vertex declaration for vertex shaders that don't require any inputs (eg. generated via vertex ID). */
TGlobalResource<FEmptyVertexDeclaration> GEmptyVertexDeclaration;

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FDrawRectangleParameters,TEXT("DrawRectangleParameters"));

typedef TUniformBufferRef<FDrawRectangleParameters> FDrawRectangleBufferRef;


static TAutoConsoleVariable<int32> CVarDrawRectangleOptimization(
	TEXT("r.DrawRectangleOptimization"),
	1,
	TEXT("Controls an optimization for DrawRectangle(). When enabled a triangle can be used to draw a quad in certain situations (viewport sized quad).\n")
	TEXT("Using a triangle allows for slightly faster post processing in lower resolutions but can not always be used.\n")
	TEXT(" 0: Optimization is disabled, DrawDenormalizedQuad always render with quad\n")
	TEXT(" 1: Optimization is enabled, a triangle can be rendered where specified (default)"),
	ECVF_RenderThreadSafe);

static void DoDrawRectangleFlagOverride(EDrawRectangleFlags& Flags)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Determine triangle draw mode
	int Value = CVarDrawRectangleOptimization.GetValueOnRenderThread();

	if(!Value)
	{
		// don't use triangle optimization
		Flags = EDRF_Default;
	}
#endif
}

template <typename TRHICommandList>
static inline void InternalDrawRectangle(
	TRHICommandList& RHICmdList,
	float X,
	float Y,
	float SizeX,
	float SizeY,
	float U,
	float V,
	float SizeU,
	float SizeV,
	FIntPoint TargetSize,
	FIntPoint TextureSize,
	FShader* VertexShader,
	EDrawRectangleFlags Flags,
	uint32 InstanceCount
	)
{
	float ClipSpaceQuadZ = 0.0f;

	DoDrawRectangleFlagOverride(Flags);

	// triangle if extending to left and top of the given rectangle, if it's not left top of the viewport it can cause artifacts
	if(X > 0.0f || Y > 0.0f)
	{
		// don't use triangle optimization
		Flags = EDRF_Default;
	}

	// Set up vertex uniform parameters for scaling and biasing the rectangle.
	// Note: Use DrawRectangle in the vertex shader to calculate the correct vertex position and uv.

	FDrawRectangleParameters Parameters;
	Parameters.PosScaleBias = FVector4(SizeX, SizeY, X, Y);
	Parameters.UVScaleBias = FVector4(SizeU, SizeV, U, V);

	Parameters.InvTargetSizeAndTextureSize = FVector4(
		1.0f / TargetSize.X, 1.0f / TargetSize.Y,
		1.0f / TextureSize.X, 1.0f / TextureSize.Y);

	SetUniformBufferParameterImmediate(RHICmdList, VertexShader->GetVertexShader(), VertexShader->GetUniformBufferParameter<FDrawRectangleParameters>(), Parameters);

	if(Flags == EDRF_UseTesselatedIndexBuffer)
	{
		// no vertex buffer needed as we compute it in VS
		RHICmdList.SetStreamSource(0, NULL, 0);

		RHICmdList.DrawIndexedPrimitive(
			GTesselatedScreenRectangleIndexBuffer.IndexBufferRHI,
			PT_TriangleList,
			/*BaseVertexIndex=*/ 0,
			/*MinIndex=*/ 0,
			/*NumVertices=*/ GTesselatedScreenRectangleIndexBuffer.NumVertices(),
			/*StartIndex=*/ 0,
			/*NumPrimitives=*/ GTesselatedScreenRectangleIndexBuffer.NumPrimitives(),
			/*NumInstances=*/ InstanceCount
			);
	}
	else
	{
		RHICmdList.SetStreamSource(0, GScreenRectangleVertexBuffer.VertexBufferRHI, 0);

		if (Flags == EDRF_UseTriangleOptimization)
		{
			// A single triangle spans the entire viewport this results in a quad that fill the viewport. This can increase rasterization efficiency
			// as we do not have a diagonal edge (through the center) for the rasterizer/span-dispatch. Although the actual benefit of this technique is dependent upon hardware.

			// We offset into the index buffer when using the triangle optimization to access the correct vertices.
			RHICmdList.DrawIndexedPrimitive(
				GScreenRectangleIndexBuffer.IndexBufferRHI,
				PT_TriangleList,
				/*BaseVertexIndex=*/ 0,
				/*MinIndex=*/ 0,
				/*NumVertices=*/ 3,
				/*StartIndex=*/ 6,
				/*NumPrimitives=*/ 1,
				/*NumInstances=*/ InstanceCount
				);
		}
		else
		{
			RHICmdList.SetStreamSource(0, GScreenRectangleVertexBuffer.VertexBufferRHI, 0);

			RHICmdList.DrawIndexedPrimitive(
				GScreenRectangleIndexBuffer.IndexBufferRHI,
				PT_TriangleList,
				/*BaseVertexIndex=*/ 0,
				/*MinIndex=*/ 0,
				/*NumVertices=*/ 4,
				/*StartIndex=*/ 0,
				/*NumPrimitives=*/ 2,
				/*NumInstances=*/ InstanceCount
				);
		}
	}
}

void DrawRectangle(
	FRHICommandList& RHICmdList,
	float X,
	float Y,
	float SizeX,
	float SizeY,
	float U,
	float V,
	float SizeU,
	float SizeV,
	FIntPoint TargetSize,
	FIntPoint TextureSize,
	FShader* VertexShader,
	EDrawRectangleFlags Flags,
	uint32 InstanceCount
)
{
	InternalDrawRectangle(RHICmdList, X, Y, SizeX, SizeY, U, V, SizeU, SizeV, TargetSize, TextureSize, VertexShader, Flags, InstanceCount);
}

void DrawTransformedRectangle(
	FRHICommandListImmediate& RHICmdList,
	float X,
	float Y,
	float SizeX,
	float SizeY,
	const FMatrix& PosTransform,
	float U,
	float V,
	float SizeU,
	float SizeV,
	const FMatrix& TexTransform,
	FIntPoint TargetSize,
	FIntPoint TextureSize
	)
{
	float ClipSpaceQuadZ = 0.0f;

	// we don't do the triangle optimization as this case is rare for the DrawTransformedRectangle case

	FFilterVertex Vertices[4];

	Vertices[0].Position = PosTransform.TransformFVector4(FVector4(X,			Y,			ClipSpaceQuadZ,	1));
	Vertices[1].Position = PosTransform.TransformFVector4(FVector4(X + SizeX,	Y,			ClipSpaceQuadZ,	1));
	Vertices[2].Position = PosTransform.TransformFVector4(FVector4(X,			Y + SizeY,	ClipSpaceQuadZ,	1));
	Vertices[3].Position = PosTransform.TransformFVector4(FVector4(X + SizeX,	Y + SizeY,	ClipSpaceQuadZ,	1));

	Vertices[0].UV = FVector2D(TexTransform.TransformFVector4(FVector(U,			V,         0)));
	Vertices[1].UV = FVector2D(TexTransform.TransformFVector4(FVector(U + SizeU,	V,         0)));
	Vertices[2].UV = FVector2D(TexTransform.TransformFVector4(FVector(U,			V + SizeV, 0)));
	Vertices[3].UV = FVector2D(TexTransform.TransformFVector4(FVector(U + SizeU,	V + SizeV, 0)));

	for (int32 VertexIndex = 0; VertexIndex < 4; VertexIndex++)
	{
		Vertices[VertexIndex].Position.X = -1.0f + 2.0f * (Vertices[VertexIndex].Position.X) / (float)TargetSize.X;
		Vertices[VertexIndex].Position.Y = (+1.0f - 2.0f * (Vertices[VertexIndex].Position.Y) / (float)TargetSize.Y) * GProjectionSignY;

		Vertices[VertexIndex].UV.X = Vertices[VertexIndex].UV.X / (float)TextureSize.X;
		Vertices[VertexIndex].UV.Y = Vertices[VertexIndex].UV.Y / (float)TextureSize.Y;
	}

	static const uint16 Indices[] =	{ 0, 1, 3, 0, 3, 2 };

	DrawIndexedPrimitiveUP(RHICmdList, PT_TriangleList, 0, 4, 2, Indices, sizeof(Indices[0]), Vertices, sizeof(Vertices[0]));
}

void DrawHmdMesh(
	FRHICommandList& RHICmdList,
	float X,
	float Y,
	float SizeX,
	float SizeY,
	float U,
	float V,
	float SizeU,
	float SizeV,
	FIntPoint TargetSize,
	FIntPoint TextureSize,
	EStereoscopicPass StereoView,
	FShader* VertexShader
	)
{
	FDrawRectangleParameters Parameters;
	Parameters.PosScaleBias = FVector4(SizeX, SizeY, X, Y);
	Parameters.UVScaleBias = FVector4(SizeU, SizeV, U, V);

	Parameters.InvTargetSizeAndTextureSize = FVector4(
		1.0f / TargetSize.X, 1.0f / TargetSize.Y,
		1.0f / TextureSize.X, 1.0f / TextureSize.Y);

	SetUniformBufferParameterImmediate(RHICmdList, VertexShader->GetVertexShader(), VertexShader->GetUniformBufferParameter<FDrawRectangleParameters>(), Parameters);

	if (GEngine->XRSystem->GetHMDDevice())
	{
		GEngine->XRSystem->GetHMDDevice()->DrawVisibleAreaMesh_RenderThread(RHICmdList, StereoView);
	}
}

void DrawPostProcessPass(
	FRHICommandList& RHICmdList,
	float X,
	float Y,
	float SizeX,
	float SizeY,
	float U,
	float V,
	float SizeU,
	float SizeV,
	FIntPoint TargetSize,
	FIntPoint TextureSize,
	FShader* VertexShader,
	EStereoscopicPass StereoView,
	bool bHasCustomMesh,
	EDrawRectangleFlags Flags)
{
	if (bHasCustomMesh && StereoView != eSSP_FULL)
	{
		DrawHmdMesh(RHICmdList, X, Y, SizeX, SizeY, U, V, SizeU, SizeV, TargetSize, TextureSize, StereoView, VertexShader);
	}
	else
	{
		DrawRectangle(RHICmdList, X, Y, SizeX, SizeY, U, V, SizeU, SizeV, TargetSize, TextureSize, VertexShader, Flags);
	}
}

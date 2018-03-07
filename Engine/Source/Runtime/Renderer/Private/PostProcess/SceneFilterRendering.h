// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneFilterRendering.h: Filter rendering definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "RendererInterface.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "PostProcess/SceneRenderTargets.h"
#include "ShaderParameterUtils.h"

/** Uniform buffer for computing the vertex positional and UV adjustments in the vertex shader. */
BEGIN_UNIFORM_BUFFER_STRUCT( FDrawRectangleParameters,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, PosScaleBias )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, UVScaleBias )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, InvTargetSizeAndTextureSize )
END_UNIFORM_BUFFER_STRUCT( FDrawRectangleParameters )

/**
 * Draws a quad with the given vertex positions and UVs in denormalized pixel/texel coordinates.
 * The platform-dependent mapping from pixels to texels is done automatically.
 * Note that the positions are affected by the current viewport.
 * NOTE: DrawRectangle should be used in the vertex shader to calculate the correct position and uv for vertices.
 *
 * X, Y							Position in screen pixels of the top left corner of the quad
 * SizeX, SizeY					Size in screen pixels of the quad
 * U, V							Position in texels of the top left corner of the quad's UV's
 * SizeU, SizeV					Size in texels of the quad's UV's
 * TargetSizeX, TargetSizeY		Size in screen pixels of the target surface
 * TextureSize                  Size in texels of the source texture
 * VertexShader					The vertex shader used for rendering
 * Flags						see EDrawRectangleFlags
 * InstanceCount				Number of instances of rectangle
 */
extern void DrawRectangle(
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
	class FShader* VertexShader,
	EDrawRectangleFlags Flags = EDRF_Default,
	uint32 InstanceCount = 1
	);

extern void DrawTransformedRectangle(
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
	);

extern void DrawHmdMesh(
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
	);

extern void DrawPostProcessPass(
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
	class FShader* VertexShader,
	EStereoscopicPass StereoView,
	bool bHasCustomMesh,
	EDrawRectangleFlags Flags = EDRF_Default
	);

extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;
extern TGlobalResource<FEmptyVertexDeclaration> GEmptyVertexDeclaration;




/*-----------------------------------------------------------------------------
FMGammaShaderParameters
-----------------------------------------------------------------------------*/

/** Encapsulates the gamma correction parameters. */
class FGammaShaderParameters
{
public:

	/** Default constructor. */
	FGammaShaderParameters() {}

	/** Initialization constructor. */
	FGammaShaderParameters(const FShaderParameterMap& ParameterMap)
	{
		RenderTargetExtent.Bind(ParameterMap,TEXT("RenderTargetExtent"));
		GammaColorScaleAndInverse.Bind(ParameterMap,TEXT("GammaColorScaleAndInverse"));
		GammaOverlayColor.Bind(ParameterMap,TEXT("GammaOverlayColor"));
	}

	/** Set the material shader parameter values. */
	void Set(FRHICommandList& RHICmdList, FShader* PixelShader, float DisplayGamma, FLinearColor const& ColorScale, FLinearColor const& ColorOverlay)
	{
		// GammaColorScaleAndInverse

		float InvDisplayGamma = 1.f / FMath::Max<float>(DisplayGamma,KINDA_SMALL_NUMBER);
		float OneMinusOverlayBlend = 1.f - ColorOverlay.A;

		FVector4 ColorScaleAndInverse;

		ColorScaleAndInverse.X = ColorScale.R * OneMinusOverlayBlend;
		ColorScaleAndInverse.Y = ColorScale.G * OneMinusOverlayBlend;
		ColorScaleAndInverse.Z = ColorScale.B * OneMinusOverlayBlend;
		ColorScaleAndInverse.W = InvDisplayGamma;

		SetShaderValue(
			RHICmdList,
			PixelShader->GetPixelShader(),
			GammaColorScaleAndInverse,
			ColorScaleAndInverse
			);

		// GammaOverlayColor

		FVector4 OverlayColor;

		OverlayColor.X = ColorOverlay.R * ColorOverlay.A;
		OverlayColor.Y = ColorOverlay.G * ColorOverlay.A;
		OverlayColor.Z = ColorOverlay.B * ColorOverlay.A;
		OverlayColor.W = 0.f; // Unused

		SetShaderValue(
			RHICmdList,
			PixelShader->GetPixelShader(),
			GammaOverlayColor,
			OverlayColor
			);

		FIntPoint BufferSize = FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY();
		float BufferSizeX = (float)BufferSize.X;
		float BufferSizeY = (float)BufferSize.Y;
		float InvBufferSizeX = 1.0f / BufferSizeX;
		float InvBufferSizeY = 1.0f / BufferSizeY;

		const FVector4 vRenderTargetExtent(BufferSizeX, BufferSizeY,  InvBufferSizeX, InvBufferSizeY);

		SetShaderValue(
			RHICmdList,
			PixelShader->GetPixelShader(),
			RenderTargetExtent,
			vRenderTargetExtent);
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FGammaShaderParameters& P)
	{
		Ar << P.GammaColorScaleAndInverse;
		Ar << P.GammaOverlayColor;
		Ar << P.RenderTargetExtent;

		return Ar;
	}


private:
	FShaderParameter				GammaColorScaleAndInverse;
	FShaderParameter				GammaOverlayColor;
	FShaderParameter				RenderTargetExtent;
};


class FTesselatedScreenRectangleIndexBuffer : public FIndexBuffer
{
public:

	// if one of those constants change, UpscaleVS needs to be recompiled

	// number of quads in x
	static const uint32 Width = 32;		// used for CylindricalProjection (smaller FOV could do less tessellation)
	// number of quads in y
	static const uint32 Height = 20;	// to minimize distortion we also tessellate in Y but a perspective distortion could do that with fewer triangles.

	/** Initialize the RHI for this rendering resource */
	void InitRHI() override;

	uint32 NumVertices() const;

	uint32 NumPrimitives() const;
};


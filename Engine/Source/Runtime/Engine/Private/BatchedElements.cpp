// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "BatchedElements.h"
#include "Misc/App.h"
#include "Shader.h"
#include "SimpleElementShaders.h"
#include "DrawingPolicy.h"
#include "PipelineStateCache.h"

DEFINE_LOG_CATEGORY_STATIC(LogBatchedElements, Log, All);

/** The simple element vertex declaration. */
TGlobalResource<FSimpleElementVertexDeclaration> GSimpleElementVertexDeclaration;

EBlendModeFilter::Type GetBlendModeFilter(ESimpleElementBlendMode BlendMode)
{
	if (BlendMode == SE_BLEND_Opaque || BlendMode == SE_BLEND_Masked || BlendMode == SE_BLEND_MaskedDistanceField || BlendMode == SE_BLEND_MaskedDistanceFieldShadowed)
	{
		return EBlendModeFilter::OpaqueAndMasked;
	}
	else
	{
		return EBlendModeFilter::Translucent;
	}
}

void FBatchedElements::AddLine(const FVector& Start, const FVector& End, const FLinearColor& Color, FHitProxyId HitProxyId, float Thickness, float DepthBias, bool bScreenSpace)
{
	// Ensure the line isn't masked out.  Some legacy code relies on Color.A being ignored.
	FLinearColor OpaqueColor(Color);
	OpaqueColor.A = 1;

	if (Thickness == 0.0f)
	{
		if (DepthBias == 0.0f)
		{
			new(LineVertices) FSimpleElementVertex(Start, FVector2D::ZeroVector, OpaqueColor, HitProxyId);
			new(LineVertices) FSimpleElementVertex(End, FVector2D::ZeroVector, OpaqueColor, HitProxyId);
		}
		else
		{
			// Draw degenerate triangles in wireframe mode to support depth bias (d3d11 and opengl3 don't support depth bias on line primitives, but do on wireframes)
			FBatchedWireTris* WireTri = new(WireTris) FBatchedWireTris();
			WireTri->DepthBias = DepthBias;
			new(WireTriVerts) FSimpleElementVertex(Start, FVector2D::ZeroVector, OpaqueColor, HitProxyId);
			new(WireTriVerts) FSimpleElementVertex(End, FVector2D::ZeroVector, OpaqueColor, HitProxyId);
			new(WireTriVerts) FSimpleElementVertex(End, FVector2D::ZeroVector, OpaqueColor, HitProxyId);
		}
	}
	else
	{
		FBatchedThickLines* ThickLine = new(ThickLines) FBatchedThickLines;
		ThickLine->Start = Start;
		ThickLine->End = End;
		ThickLine->Thickness = Thickness;
		ThickLine->Color = OpaqueColor;
		ThickLine->HitProxyId = HitProxyId;
		ThickLine->DepthBias = DepthBias;
		ThickLine->bScreenSpace = bScreenSpace;
	}
}

void FBatchedElements::AddTranslucentLine(const FVector& Start, const FVector& End, const FLinearColor& Color, FHitProxyId HitProxyId, float Thickness, float DepthBias, bool bScreenSpace)
{
	if (Thickness == 0.0f)
	{
		if (DepthBias == 0.0f)
		{
			new(LineVertices) FSimpleElementVertex(Start, FVector2D::ZeroVector, Color, HitProxyId);
			new(LineVertices) FSimpleElementVertex(End, FVector2D::ZeroVector, Color, HitProxyId);
		}
		else
		{
			// Draw degenerate triangles in wireframe mode to support depth bias (d3d11 and opengl3 don't support depth bias on line primitives, but do on wireframes)
			FBatchedWireTris* WireTri = new(WireTris) FBatchedWireTris();
			WireTri->DepthBias = DepthBias;
			new(WireTriVerts) FSimpleElementVertex(Start, FVector2D::ZeroVector, Color, HitProxyId);
			new(WireTriVerts) FSimpleElementVertex(End, FVector2D::ZeroVector, Color, HitProxyId);
			new(WireTriVerts) FSimpleElementVertex(End, FVector2D::ZeroVector, Color, HitProxyId);
		}
	}
	else
	{
		FBatchedThickLines* ThickLine = new(ThickLines) FBatchedThickLines;
		ThickLine->Start = Start;
		ThickLine->End = End;
		ThickLine->Thickness = Thickness;
		ThickLine->Color = Color;
		ThickLine->HitProxyId = HitProxyId;
		ThickLine->DepthBias = DepthBias;
		ThickLine->bScreenSpace = bScreenSpace;

	}
}

void FBatchedElements::AddPoint(const FVector& Position,float Size,const FLinearColor& Color,FHitProxyId HitProxyId)
{
	// Ensure the point isn't masked out.  Some legacy code relies on Color.A being ignored.
	FLinearColor OpaqueColor(Color);
	OpaqueColor.A = 1;

	FBatchedPoint* Point = new(Points) FBatchedPoint;
	Point->Position = Position;
	Point->Size = Size;
	Point->Color = OpaqueColor.ToFColor(true);
	Point->HitProxyId = HitProxyId;
}

int32 FBatchedElements::AddVertex(const FVector4& InPosition,const FVector2D& InTextureCoordinate,const FLinearColor& InColor,FHitProxyId HitProxyId)
{
	int32 VertexIndex = MeshVertices.Num();
	new(MeshVertices) FSimpleElementVertex(InPosition,InTextureCoordinate,InColor,HitProxyId);
	return VertexIndex;
}

/** Adds a triangle to the batch. */
void FBatchedElements::AddTriangle(int32 V0,int32 V1,int32 V2,const FTexture* Texture,EBlendMode BlendMode)
{
	ESimpleElementBlendMode SimpleElementBlendMode = SE_BLEND_Opaque;
	switch (BlendMode)
	{
	case BLEND_Opaque:
		SimpleElementBlendMode = SE_BLEND_Opaque; 
		break;
	case BLEND_Masked:
	case BLEND_Translucent:
		SimpleElementBlendMode = SE_BLEND_Translucent; 
		break;
	case BLEND_Additive:
		SimpleElementBlendMode = SE_BLEND_Additive; 
		break;
	case BLEND_Modulate:
		SimpleElementBlendMode = SE_BLEND_Modulate; 
		break;
	case BLEND_AlphaComposite:
		SimpleElementBlendMode = SE_BLEND_AlphaComposite;
		break;
	};	
	AddTriangle(V0,V1,V2,Texture,SimpleElementBlendMode);
}

void FBatchedElements::AddTriangle(int32 V0, int32 V1, int32 V2, const FTexture* Texture, ESimpleElementBlendMode BlendMode, const FDepthFieldGlowInfo& GlowInfo)
{
	AddTriangleExtensive( V0, V1, V2, NULL, Texture, BlendMode, GlowInfo );
}

	
void FBatchedElements::AddTriangle(int32 V0,int32 V1,int32 V2,FBatchedElementParameters* BatchedElementParameters,ESimpleElementBlendMode BlendMode)
{
	AddTriangleExtensive( V0, V1, V2, BatchedElementParameters, GWhiteTexture, BlendMode );
}


void FBatchedElements::AddTriangleExtensive(int32 V0,int32 V1,int32 V2,FBatchedElementParameters* BatchedElementParameters,const FTexture* Texture,ESimpleElementBlendMode BlendMode, const FDepthFieldGlowInfo& GlowInfo)
{
	check(Texture);

	// Find an existing mesh element for the given texture and blend mode
	FBatchedMeshElement* MeshElement = NULL;
	for(int32 MeshIndex = 0;MeshIndex < MeshElements.Num();MeshIndex++)
	{
		const FBatchedMeshElement& CurMeshElement = MeshElements[MeshIndex];
		if( CurMeshElement.Texture == Texture && 
			CurMeshElement.BatchedElementParameters.GetReference() == BatchedElementParameters &&
			CurMeshElement.BlendMode == BlendMode &&
			// make sure we are not overflowing on indices
			(CurMeshElement.Indices.Num()+3) < MaxMeshIndicesAllowed &&
			CurMeshElement.GlowInfo == GlowInfo )
		{
			// make sure we are not overflowing on vertices
			int32 DeltaV0 = (V0 - (int32)CurMeshElement.MinVertex);
			int32 DeltaV1 = (V1 - (int32)CurMeshElement.MinVertex);
			int32 DeltaV2 = (V2 - (int32)CurMeshElement.MinVertex);
			if( DeltaV0 >= 0 && DeltaV0 < MaxMeshVerticesAllowed &&
				DeltaV1 >= 0 && DeltaV1 < MaxMeshVerticesAllowed &&
				DeltaV2 >= 0 && DeltaV2 < MaxMeshVerticesAllowed )
			{
				MeshElement = &MeshElements[MeshIndex];
				break;
			}			
		}
	}
	if(!MeshElement)
	{
		// make sure that vertex indices are close enough to fit within MaxVerticesAllowed
		if( FMath::Abs(V0 - V1) >= MaxMeshVerticesAllowed ||
			FMath::Abs(V0 - V2) >= MaxMeshVerticesAllowed )
		{
			UE_LOG(LogBatchedElements, Warning, TEXT("Omitting FBatchedElements::AddTriangle due to sparce vertices V0=%i,V1=%i,V2=%i"),V0,V1,V2);
		}
		else
		{
			// Create a new mesh element for the texture if this is the first triangle encountered using it.
			MeshElement = new(MeshElements) FBatchedMeshElement;
			MeshElement->Texture = Texture;
			MeshElement->BatchedElementParameters = BatchedElementParameters;
			MeshElement->BlendMode = BlendMode;
			MeshElement->GlowInfo = GlowInfo;
			MeshElement->MaxVertex = V0;
			// keep track of the min vertex index used
			MeshElement->MinVertex = FMath::Min(FMath::Min(V0,V1),V2);
		}
	}

	if( MeshElement )
	{
		// Add the triangle's indices to the mesh element's index array.
		MeshElement->Indices.Add(V0 - MeshElement->MinVertex);
		MeshElement->Indices.Add(V1 - MeshElement->MinVertex);
		MeshElement->Indices.Add(V2 - MeshElement->MinVertex);

		// keep track of max vertex used in this mesh batch
		MeshElement->MaxVertex = FMath::Max(FMath::Max(FMath::Max(V0,(int32)MeshElement->MaxVertex),V1),V2);
	}
}

/** 
* Reserves space in mesh vertex array
* 
* @param NumMeshVerts - number of verts to reserve space for
* @param Texture - used to find the mesh element entry
* @param BlendMode - used to find the mesh element entry
*/
void FBatchedElements::AddReserveVertices(int32 NumMeshVerts)
{
	MeshVertices.Reserve( MeshVertices.Num() + NumMeshVerts );
}

void FBatchedElements::ReserveVertices(int32 NumMeshVerts)
{
	MeshVertices.Reserve( NumMeshVerts );
}

/** 
 * Reserves space in line vertex array
 * 
 * @param NumLines - number of lines to reserve space for
 * @param bDepthBiased - whether reserving depth-biased lines or non-biased lines
 * @param bThickLines - whether reserving regular lines or thick lines
 */
void FBatchedElements::AddReserveLines(int32 NumLines, bool bDepthBiased, bool bThickLines)
{
	if (!bThickLines)
	{
		if (!bDepthBiased)
		{
			LineVertices.Reserve(LineVertices.Num() + NumLines * 2);
		}
		else
		{
			WireTris.Reserve(WireTris.Num() + NumLines);
			WireTriVerts.Reserve(WireTriVerts.Num() + NumLines * 3);
		}
	}
	else
	{
		ThickLines.Reserve(ThickLines.Num() + NumLines * 2);
	}
}

/** 
* Reserves space in triangle arrays 
* 
* @param NumMeshTriangles - number of triangles to reserve space for
* @param Texture - used to find the mesh element entry
* @param BlendMode - used to find the mesh element entry
*/
void FBatchedElements::AddReserveTriangles(int32 NumMeshTriangles,const FTexture* Texture,ESimpleElementBlendMode BlendMode)
{
	for(int32 MeshIndex = 0;MeshIndex < MeshElements.Num();MeshIndex++)
	{
		FBatchedMeshElement& CurMeshElement = MeshElements[MeshIndex];
		if( CurMeshElement.Texture == Texture && 
			CurMeshElement.BatchedElementParameters.GetReference() == NULL &&
			CurMeshElement.BlendMode == BlendMode &&
			(CurMeshElement.Indices.Num()+3) < MaxMeshIndicesAllowed )
		{
			CurMeshElement.Indices.Reserve( CurMeshElement.Indices.Num() + NumMeshTriangles );
			break;
		}
	}	
}

void FBatchedElements::ReserveTriangles(int32 NumMeshTriangles, const FTexture* Texture, ESimpleElementBlendMode BlendMode)
{
	for (int32 MeshIndex = 0; MeshIndex < MeshElements.Num(); MeshIndex++)
	{
		FBatchedMeshElement& CurMeshElement = MeshElements[MeshIndex];
		if (CurMeshElement.Texture == Texture &&
		   CurMeshElement.BatchedElementParameters.GetReference() == NULL &&
		   CurMeshElement.BlendMode == BlendMode &&
		   (CurMeshElement.Indices.Num()+3) < MaxMeshIndicesAllowed)
		{
			CurMeshElement.Indices.Reserve( NumMeshTriangles );
			break;
		}
	}
}

void FBatchedElements::AddSprite(
	const FVector& Position,
	float SizeX,
	float SizeY,
	const FTexture* Texture,
	const FLinearColor& Color,
	FHitProxyId HitProxyId,
	float U,
	float UL,
	float V,
	float VL,
	uint8 BlendMode
	)
{
	check(Texture);

	FBatchedSprite* Sprite = new(Sprites) FBatchedSprite;
	Sprite->Position = Position;
	Sprite->SizeX = SizeX;
	Sprite->SizeY = SizeY;
	Sprite->Texture = Texture;
	Sprite->Color = Color;
	Sprite->HitProxyId = HitProxyId;
	Sprite->U = U;
	Sprite->UL = UL == 0.f ? Texture->GetSizeX() : UL;
	Sprite->V = V;
	Sprite->VL = VL == 0.f ? Texture->GetSizeY() : VL;
	Sprite->BlendMode = BlendMode;
}

/** Translates a ESimpleElementBlendMode into a RHI state change for rendering a mesh with the blend mode normally. */
static void SetBlendState(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, ESimpleElementBlendMode BlendMode, bool bEncodedHDR)
{
	if (bEncodedHDR)
	{
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		return;
	}

	// Override blending operations to accumulate alpha
	static const auto CVarCompositeMode = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.UI.CompositeMode"));

	const bool bCompositeUI = GRHISupportsHDROutput
		&& CVarCompositeMode && CVarCompositeMode->GetValueOnRenderThread() != 0 
		&& IsHDREnabled();

	if (bCompositeUI)
	{
		// Compositing to offscreen buffer, so alpha needs to be accumulated in a sensible manner
		switch (BlendMode)
		{
		case SE_BLEND_Translucent:
		case SE_BLEND_TranslucentDistanceField:
		case SE_BLEND_TranslucentDistanceFieldShadowed:
		case SE_BLEND_TranslucentAlphaOnly:
			BlendMode = SE_BLEND_AlphaBlend;
			break;

		default:
			// Blend mode is reasonable as-is
			break;
		};
	}

	switch(BlendMode)
	{
	case SE_BLEND_Opaque:
	case SE_BLEND_Masked:
	case SE_BLEND_MaskedDistanceField:
	case SE_BLEND_MaskedDistanceFieldShadowed:
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		break;
	case SE_BLEND_Translucent:
	case SE_BLEND_TranslucentDistanceField:
	case SE_BLEND_TranslucentDistanceFieldShadowed:
	case SE_BLEND_TranslucentAlphaOnly:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI();
		break;
	case SE_BLEND_Additive:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI();
		break;
	case SE_BLEND_Modulate:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI();
		break;
	case SE_BLEND_AlphaComposite:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
		break;
	case SE_BLEND_TranslucentAlphaOnlyWriteAlpha:
	case SE_BLEND_AlphaBlend:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_InverseDestAlpha, BF_One>::GetRHI();
		break;
	case SE_BLEND_RGBA_MASK_END:
	case SE_BLEND_RGBA_MASK_START:
		break;
	}
}

/** Translates a ESimpleElementBlendMode into a RHI state change for rendering a mesh with the blend mode for hit testing. */
static void SetHitTestingBlendState(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, ESimpleElementBlendMode BlendMode)
{
	switch(BlendMode)
	{
	case SE_BLEND_Opaque:
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		break;
	case SE_BLEND_Masked:
	case SE_BLEND_MaskedDistanceField:
	case SE_BLEND_MaskedDistanceFieldShadowed:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
		break;
	case SE_BLEND_AlphaComposite:
	case SE_BLEND_AlphaBlend:
	case SE_BLEND_Translucent:
	case SE_BLEND_TranslucentDistanceField:
	case SE_BLEND_TranslucentDistanceFieldShadowed:
	case SE_BLEND_TranslucentAlphaOnly:
	case SE_BLEND_TranslucentAlphaOnlyWriteAlpha:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
		break;
	case SE_BLEND_Additive:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
		break;
	case SE_BLEND_Modulate:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
		break;
	case SE_BLEND_RGBA_MASK_END:
	case SE_BLEND_RGBA_MASK_START:
		break;
	}
}

/** Global alpha ref test value for rendering masked batched elements */
float GBatchedElementAlphaRefVal = 128.f;
/** Global smoothing width for rendering batched elements with distance field blend modes */
float GBatchedElementSmoothWidth = 4;

FAutoConsoleVariableRef CVarWellCanvasDistanceFieldSmoothWidth(TEXT("Canvas.DistanceFieldSmoothness"), GBatchedElementSmoothWidth, TEXT("Global sharpness of distance field fonts/shapes rendered by canvas."), ECVF_Default);

template<class TSimpleElementPixelShader>
static TSimpleElementPixelShader* GetPixelShader(bool bEncoded, ESimpleElementBlendMode BlendMode, ERHIFeatureLevel::Type FeatureLevel)
{
	if (bEncoded)
	{
		// When encoding blending occurs in the shader. return the appropriate blend shader.
		switch (BlendMode)
		{
			case SE_BLEND_Opaque:
				return *TShaderMapRef<FEncodedSimpleElement<TSimpleElementPixelShader, SE_BLEND_Opaque> >(GetGlobalShaderMap(FeatureLevel));
			case SE_BLEND_Masked:
				return *TShaderMapRef<FEncodedSimpleElement<TSimpleElementPixelShader, SE_BLEND_Masked> >(GetGlobalShaderMap(FeatureLevel));
			case SE_BLEND_Translucent:
				return *TShaderMapRef<FEncodedSimpleElement<TSimpleElementPixelShader, SE_BLEND_Translucent> >(GetGlobalShaderMap(FeatureLevel));
			case SE_BLEND_Additive:
				return *TShaderMapRef<FEncodedSimpleElement<TSimpleElementPixelShader, SE_BLEND_Additive> >(GetGlobalShaderMap(FeatureLevel));
			case SE_BLEND_Modulate:
				return *TShaderMapRef<FEncodedSimpleElement<TSimpleElementPixelShader, SE_BLEND_Modulate> >(GetGlobalShaderMap(FeatureLevel));
			case SE_BLEND_MaskedDistanceField:
				return *TShaderMapRef<FEncodedSimpleElement<TSimpleElementPixelShader, SE_BLEND_MaskedDistanceField> >(GetGlobalShaderMap(FeatureLevel));
			case SE_BLEND_MaskedDistanceFieldShadowed:
				return *TShaderMapRef<FEncodedSimpleElement<TSimpleElementPixelShader, SE_BLEND_MaskedDistanceFieldShadowed> >(GetGlobalShaderMap(FeatureLevel));
			case SE_BLEND_AlphaComposite:
				return *TShaderMapRef<FEncodedSimpleElement<TSimpleElementPixelShader, SE_BLEND_AlphaComposite> >(GetGlobalShaderMap(FeatureLevel));
			case SE_BLEND_AlphaBlend:
				return *TShaderMapRef<FEncodedSimpleElement<TSimpleElementPixelShader, SE_BLEND_AlphaBlend> >(GetGlobalShaderMap(FeatureLevel));
			case SE_BLEND_TranslucentAlphaOnly:
				return *TShaderMapRef<FEncodedSimpleElement<TSimpleElementPixelShader, SE_BLEND_TranslucentAlphaOnly> >(GetGlobalShaderMap(FeatureLevel));
			case SE_BLEND_TranslucentAlphaOnlyWriteAlpha:
				return *TShaderMapRef<FEncodedSimpleElement<TSimpleElementPixelShader, SE_BLEND_TranslucentAlphaOnlyWriteAlpha> >(GetGlobalShaderMap(FeatureLevel));
			default:
				checkNoEntry();
		}
	}
	return *TShaderMapRef<TSimpleElementPixelShader>(GetGlobalShaderMap(FeatureLevel));
}

static bool Is32BppHDREncoded(const FSceneView* View, ERHIFeatureLevel::Type FeatureLevel)
{
	// If the view has no view family then it wont be using encoding.
	// Do not use the view's feature level, if it does not have a scene it will be invalid.
	if (View == nullptr || FeatureLevel >= ERHIFeatureLevel::ES3_1 || View->Family == nullptr)
	{
		return false;
	}

	static auto* MobileHDRCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
	bool bMobileHDR = MobileHDRCvar->GetValueOnRenderThread() == 1;

	static auto* MobileHDR32bppModeCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR32bppMode"));
	bool bMobileHDR32Bpp = bMobileHDR && (GSupportsRenderTargetFormat_PF_FloatRGBA == false || MobileHDR32bppModeCvar->GetValueOnRenderThread() != 0);

	if (bMobileHDR32Bpp)
	{
		switch (MobileHDR32bppModeCvar->GetValueOnRenderThread())
		{
		case 1:
			return false;
		case 2:
			return true;
		default:
			return (GSupportsHDR32bppEncodeModeIntrinsic && GSupportsShaderFramebufferFetch);
		}
	}
	return false;
}

/**
 * Sets the appropriate vertex and pixel shader.
 */
void FBatchedElements::PrepareShaders(
	FRHICommandList& RHICmdList,
	FGraphicsPipelineStateInitializer& GraphicsPSOInit,
	ERHIFeatureLevel::Type FeatureLevel,
	ESimpleElementBlendMode BlendMode,
	const FMatrix& Transform,
	bool bSwitchVerticalAxis,
	FBatchedElementParameters* BatchedElementParameters,
	const FTexture* Texture,
	bool bHitTesting,
	float Gamma,
	const FDepthFieldGlowInfo* GlowInfo,
	const FSceneView* View,
	FTexture2DRHIRef DepthTexture
	) const
{
	// used to mask individual channels and desaturate
	FMatrix ColorWeights( FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 1, 0), FPlane(0, 0, 0, 0) );

	// bEncodedHDR requires that blend states are disabled.
	bool bEncodedHDR = /*bEnableHDREncoding &&*/ Is32BppHDREncoded(View, FeatureLevel);

	float GammaToUse = Gamma;

	ESimpleElementBlendMode MaskedBlendMode = SE_BLEND_Opaque;

	if(BlendMode >= SE_BLEND_RGBA_MASK_START && BlendMode <= SE_BLEND_RGBA_MASK_END)
	{
		/*
		* Red, Green, Blue and Alpha color weights all initialized to 0
		*/
		FPlane R( 0.0f, 0.0f, 0.0f, 0.0f );
		FPlane G( 0.0f, 0.0f, 0.0f, 0.0f );
		FPlane B( 0.0f, 0.0f, 0.0f, 0.0f );
		FPlane A( 0.0f,	0.0f, 0.0f, 0.0f );

		/*
		* Extract the color components from the in BlendMode to determine which channels should be active
		*/
		uint32 BlendMask = ( ( uint32 )BlendMode ) - SE_BLEND_RGBA_MASK_START;

		bool bRedChannel = ( BlendMask & ( 1 << 0 ) ) != 0;
		bool bGreenChannel = ( BlendMask & ( 1 << 1 ) ) != 0;
		bool bBlueChannel = ( BlendMask & ( 1 << 2 ) ) != 0;
		bool bAlphaChannel = ( BlendMask & ( 1 << 3 ) ) != 0;
		bool bDesaturate = ( BlendMask & ( 1 << 4 ) ) != 0;
		bool bAlphaOnly = bAlphaChannel && !bRedChannel && !bGreenChannel && !bBlueChannel;
		uint32 NumChannelsOn = ( bRedChannel ? 1 : 0 ) + ( bGreenChannel ? 1 : 0 ) + ( bBlueChannel ? 1 : 0 );
		GammaToUse = bAlphaOnly? 1.0f: Gamma;
		
		// If we are only to draw the alpha channel, make the Blend state opaque, to allow easy identification of the alpha values
		if( bAlphaOnly )
		{
			MaskedBlendMode = SE_BLEND_Opaque;
			SetBlendState(RHICmdList, GraphicsPSOInit, MaskedBlendMode, bEncodedHDR);
			
			R.W = G.W = B.W = 1.0f;
		}
		else
		{
			MaskedBlendMode = !bAlphaChannel ? SE_BLEND_Opaque : SE_BLEND_Translucent;  // If alpha channel is disabled, do not allow alpha blending
			SetBlendState(RHICmdList, GraphicsPSOInit, MaskedBlendMode, bEncodedHDR);

			// Determine the red, green, blue and alpha components of their respective weights to enable that colours prominence
			R.X = bRedChannel ? 1.0f : 0.0f;
			G.Y = bGreenChannel ? 1.0f : 0.0f;
			B.Z = bBlueChannel ? 1.0f : 0.0f;
			A.W = bAlphaChannel ? 1.0f : 0.0f;

			/*
			* Determine if desaturation is enabled, if so, we determine the output colour based on the number of active channels that are displayed
			* e.g. if Only the red and green channels are being viewed, then the desaturation of the image will be divided by 2
			*/
			if( bDesaturate && NumChannelsOn )
			{
				float ValR, ValG, ValB;
				ValR = R.X / NumChannelsOn;
				ValG = G.Y / NumChannelsOn;
				ValB = B.Z / NumChannelsOn;
				R = FPlane( ValR, ValG, ValB, 0 );
				G = FPlane( ValR, ValG, ValB, 0 );
				B = FPlane( ValR, ValG, ValB, 0 );
			}
		}

		ColorWeights = FMatrix( R, G, B, A );
	}
	
	if( BatchedElementParameters != NULL )
	{
		// Use the vertex/pixel shader that we were given
		BatchedElementParameters->BindShaders(RHICmdList, GraphicsPSOInit, FeatureLevel, Transform, GammaToUse, ColorWeights, Texture);
	}
	else
	{
		TShaderMapRef<FSimpleElementVS> VertexShader(GetGlobalShaderMap(FeatureLevel));
			
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GSimpleElementVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);

		if (bHitTesting)
		{
			SetHitTestingBlendState(RHICmdList, GraphicsPSOInit, BlendMode);

			TShaderMapRef<FSimpleElementHitProxyPS> HitTestingPixelShader(GetGlobalShaderMap(FeatureLevel));
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*HitTestingPixelShader);

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			HitTestingPixelShader->SetParameters(RHICmdList, Texture);
		}
		else
		{
			if (BlendMode == SE_BLEND_Masked)
			{
				// use clip() in the shader instead of alpha testing as cards that don't support floating point blending
				// also don't support alpha testing to floating point render targets
				GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

				if (Texture->bSRGB)
				{
					auto* MaskedPixelShader = GetPixelShader<FSimpleElementMaskedGammaPS_SRGB>(bEncodedHDR, BlendMode, FeatureLevel);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(MaskedPixelShader);

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

					MaskedPixelShader->SetEditorCompositingParameters(RHICmdList, View, DepthTexture);
					MaskedPixelShader->SetParameters(RHICmdList, Texture, Gamma, GBatchedElementAlphaRefVal / 255.0f, BlendMode);
				}
				else
				{
					auto* MaskedPixelShader = GetPixelShader<FSimpleElementMaskedGammaPS_Linear>(bEncodedHDR, BlendMode, FeatureLevel);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(MaskedPixelShader);

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

					MaskedPixelShader->SetEditorCompositingParameters(RHICmdList, View, DepthTexture);
					MaskedPixelShader->SetParameters(RHICmdList, Texture, Gamma, GBatchedElementAlphaRefVal / 255.0f, BlendMode);
				}
			}
			// render distance field elements
			else if (
				BlendMode == SE_BLEND_MaskedDistanceField || 
				BlendMode == SE_BLEND_MaskedDistanceFieldShadowed ||
				BlendMode == SE_BLEND_TranslucentDistanceField	||
				BlendMode == SE_BLEND_TranslucentDistanceFieldShadowed)
			{
				float AlphaRefVal = GBatchedElementAlphaRefVal;
				if (BlendMode == SE_BLEND_TranslucentDistanceField ||
					BlendMode == SE_BLEND_TranslucentDistanceFieldShadowed)
				{
					// enable alpha blending and disable clip ref value for translucent rendering
					if (!bEncodedHDR)
					{
						GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
					}
					AlphaRefVal = 0.0f;
				}
				else
				{
					// clip is done in shader so just render opaque
					GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
				}
				
				TShaderMapRef<FSimpleElementDistanceFieldGammaPS> DistanceFieldPixelShader(GetGlobalShaderMap(FeatureLevel));
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*DistanceFieldPixelShader);

				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

				// @todo - expose these as options for batch rendering
				static FVector2D ShadowDirection(-1.0f/Texture->GetSizeX(),-1.0f/Texture->GetSizeY());
				static FLinearColor ShadowColor(FLinearColor::Black);
				const float ShadowSmoothWidth = (GBatchedElementSmoothWidth * 2) / Texture->GetSizeX();
				
				const bool EnableShadow = (
					BlendMode == SE_BLEND_MaskedDistanceFieldShadowed || 
					BlendMode == SE_BLEND_TranslucentDistanceFieldShadowed
					);
				
				DistanceFieldPixelShader->SetParameters(
					RHICmdList, 
					Texture,
					Gamma,
					AlphaRefVal / 255.0f,
					GBatchedElementSmoothWidth,
					EnableShadow,
					ShadowDirection,
					ShadowColor,
					ShadowSmoothWidth,
					(GlowInfo != NULL) ? *GlowInfo : FDepthFieldGlowInfo(),
					BlendMode
					);
			}
			else if(BlendMode == SE_BLEND_TranslucentAlphaOnly || BlendMode == SE_BLEND_TranslucentAlphaOnlyWriteAlpha)
			{
				SetBlendState(RHICmdList, GraphicsPSOInit, BlendMode, bEncodedHDR);

				if (FMath::Abs(Gamma - 1.0f) < KINDA_SMALL_NUMBER)
				{
					auto* AlphaOnlyPixelShader = GetPixelShader<FSimpleElementAlphaOnlyPS>(bEncodedHDR, BlendMode, FeatureLevel);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(AlphaOnlyPixelShader);

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

					AlphaOnlyPixelShader->SetParameters(RHICmdList, Texture);
					AlphaOnlyPixelShader->SetEditorCompositingParameters(RHICmdList, View, DepthTexture);
				}
				else
				{
					auto* GammaAlphaOnlyPixelShader = GetPixelShader<FSimpleElementGammaAlphaOnlyPS>(bEncodedHDR, BlendMode, FeatureLevel);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(GammaAlphaOnlyPixelShader);

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

					GammaAlphaOnlyPixelShader->SetParameters(RHICmdList, Texture, Gamma, BlendMode);
					GammaAlphaOnlyPixelShader->SetEditorCompositingParameters(RHICmdList, View, DepthTexture);
				}
			}
			else if(BlendMode >= SE_BLEND_RGBA_MASK_START && BlendMode <= SE_BLEND_RGBA_MASK_END)
			{
				TShaderMapRef<FSimpleElementColorChannelMaskPS> ColorChannelMaskPixelShader(GetGlobalShaderMap(FeatureLevel));
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*ColorChannelMaskPixelShader);

				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
			
				ColorChannelMaskPixelShader->SetParameters(RHICmdList, Texture, ColorWeights, GammaToUse );
			}
			else
			{
				SetBlendState(RHICmdList, GraphicsPSOInit, BlendMode, bEncodedHDR);
	
				if (FMath::Abs(Gamma - 1.0f) < KINDA_SMALL_NUMBER)
				{
					TShaderMapRef<FSimpleElementPS> PixelShader(GetGlobalShaderMap(FeatureLevel));
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

					PixelShader->SetParameters(RHICmdList, Texture);
					PixelShader->SetEditorCompositingParameters(RHICmdList, View, DepthTexture);
				}
				else
				{
					TShaderMapRef<FSimpleElementGammaPS_SRGB> PixelShader_SRGB(GetGlobalShaderMap(FeatureLevel));
					TShaderMapRef<FSimpleElementGammaPS_Linear> PixelShader_Linear(GetGlobalShaderMap(FeatureLevel));
					
					FSimpleElementGammaBasePS* BasePixelShader = Texture->bSRGB ? static_cast<FSimpleElementGammaBasePS*>(*PixelShader_SRGB) : *PixelShader_Linear;

					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(BasePixelShader);
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

					BasePixelShader->SetParameters(RHICmdList, Texture, Gamma, BlendMode);
					BasePixelShader->SetEditorCompositingParameters(RHICmdList, View, DepthTexture);
				}
			}
		}

		// Set the simple element vertex shader parameters
		VertexShader->SetParameters(RHICmdList, Transform, bSwitchVerticalAxis);
	}
}

void FBatchedElements::DrawPointElements(FRHICommandList& RHICmdList, const FMatrix& Transform, const uint32 ViewportSizeX, const uint32 ViewportSizeY, const FVector& CameraX, const FVector& CameraY) const
{
	// Draw the point elements.
	if( Points.Num() > 0 )
	{
		// preallocate some memory to directly fill out
		void* VerticesPtr;

		const int32 NumPoints = Points.Num();
		const int32 NumTris = NumPoints * 2;
		const int32 NumVertices = NumTris * 3;
		RHICmdList.BeginDrawPrimitiveUP(PT_TriangleList, NumTris, NumVertices, sizeof(FSimpleElementVertex), VerticesPtr);
		FSimpleElementVertex* PointVertices = (FSimpleElementVertex*)VerticesPtr;

		int32 VertIdx = 0;
		for(int32 PointIndex = 0;PointIndex < NumPoints;PointIndex++)
		{
			// TODO: Support quad primitives here
			const FBatchedPoint& Point = Points[PointIndex];
			FVector4 TransformedPosition = Transform.TransformFVector4(Point.Position);

			// Generate vertices for the point such that the post-transform point size is constant.
			const uint32 ViewportMajorAxis = ViewportSizeX;//FMath::Max(ViewportSizeX, ViewportSizeY);
			const FVector WorldPointX = CameraX * Point.Size / ViewportMajorAxis * TransformedPosition.W;
			const FVector WorldPointY = CameraY * -Point.Size / ViewportMajorAxis * TransformedPosition.W;
					
			PointVertices[VertIdx + 0] = FSimpleElementVertex(FVector4(Point.Position + WorldPointX - WorldPointY,1),FVector2D(1,0),Point.Color,Point.HitProxyId);
			PointVertices[VertIdx + 1] = FSimpleElementVertex(FVector4(Point.Position + WorldPointX + WorldPointY,1),FVector2D(1,1),Point.Color,Point.HitProxyId);
			PointVertices[VertIdx + 2] = FSimpleElementVertex(FVector4(Point.Position - WorldPointX - WorldPointY,1),FVector2D(0,0),Point.Color,Point.HitProxyId);
			PointVertices[VertIdx + 3] = FSimpleElementVertex(FVector4(Point.Position + WorldPointX + WorldPointY,1),FVector2D(1,1),Point.Color,Point.HitProxyId);
			PointVertices[VertIdx + 4] = FSimpleElementVertex(FVector4(Point.Position - WorldPointX - WorldPointY,1),FVector2D(0,0),Point.Color,Point.HitProxyId);
			PointVertices[VertIdx + 5] = FSimpleElementVertex(FVector4(Point.Position - WorldPointX + WorldPointY,1),FVector2D(0,1),Point.Color,Point.HitProxyId);

			VertIdx += 6;
		}

		// Draw the sprite.
		RHICmdList.EndDrawPrimitiveUP();
	}
}

FSceneView FBatchedElements::CreateProxySceneView(const FMatrix& ProjectionMatrix, const FIntRect& ViewRect)
{
	FSceneViewInitOptions ProxyViewInitOptions;
	ProxyViewInitOptions.SetViewRectangle(ViewRect);
	ProxyViewInitOptions.ViewOrigin = FVector::ZeroVector;
	ProxyViewInitOptions.ViewRotationMatrix = FMatrix::Identity;
	ProxyViewInitOptions.ProjectionMatrix = ProjectionMatrix;

	return FSceneView(ProxyViewInitOptions);
}

bool FBatchedElements::Draw(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, ERHIFeatureLevel::Type FeatureLevel, bool bNeedToSwitchVerticalAxis, const FMatrix& Transform, uint32 ViewportSizeX, uint32 ViewportSizeY, bool bHitTesting, float Gamma, const FSceneView* View, FTexture2DRHIRef DepthTexture, EBlendModeFilter::Type Filter) const
{
	if ( View )
	{
		// Going to ignore these parameters in favor of just using the values directly from the scene view, so ensure that they're identical.
		check(Transform == View->ViewMatrices.GetViewProjectionMatrix());
		check(ViewportSizeX == View->ViewRect.Width());
		check(ViewportSizeY == View->ViewRect.Height());

		return Draw(RHICmdList, DrawRenderState, FeatureLevel, bNeedToSwitchVerticalAxis, *View, bHitTesting, Gamma, DepthTexture, Filter);
	}
	else
	{
		FIntRect ViewRect = FIntRect(0, 0, ViewportSizeX, ViewportSizeY);

		return Draw(RHICmdList, DrawRenderState, FeatureLevel, bNeedToSwitchVerticalAxis, CreateProxySceneView(Transform, ViewRect), bHitTesting, Gamma, DepthTexture, Filter);
	}
}

bool FBatchedElements::Draw(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, ERHIFeatureLevel::Type FeatureLevel, bool bNeedToSwitchVerticalAxis, const FSceneView& View, bool bHitTesting, float Gamma /* = 1.0f */, FTexture2DRHIRef DepthTexture /* = FTexture2DRHIRef() */, EBlendModeFilter::Type Filter /* = EBlendModeFilter::All */) const
{
	const FMatrix& Transform = View.ViewMatrices.GetViewProjectionMatrix();
	const uint32 ViewportSizeX = View.ViewRect.Width();
	const uint32 ViewportSizeY = View.ViewRect.Height();

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	DrawRenderState.ApplyToPSO(GraphicsPSOInit);
	uint32 StencilRef = DrawRenderState.GetStencilRef();

	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();

	if (UNLIKELY(!FApp::CanEverRender()))
	{
		return false;
	}

	if( HasPrimsToDraw() )
	{
		FMatrix InvTransform = Transform.Inverse();
		FVector CameraX = InvTransform.TransformVector(FVector(1,0,0)).GetSafeNormal();
		FVector CameraY = InvTransform.TransformVector(FVector(0,1,0)).GetSafeNormal();
		FVector CameraZ = InvTransform.TransformVector(FVector(0,0,1)).GetSafeNormal();

		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();

		if( (LineVertices.Num() > 0 || Points.Num() > 0 || ThickLines.Num() > 0 || WireTris.Num() > 0)
			&& (Filter & EBlendModeFilter::OpaqueAndMasked))
		{
			// Lines/points don't support batched element parameters (yet!)
			FBatchedElementParameters* BatchedElementParameters = NULL;

			// Draw the line elements.
			if( LineVertices.Num() > 0 )
			{
				GraphicsPSOInit.PrimitiveType = PT_LineList;

				// Set the appropriate pixel shader parameters & shader state for the non-textured elements.
				PrepareShaders(RHICmdList, GraphicsPSOInit, FeatureLevel, SE_BLEND_Opaque, Transform, bNeedToSwitchVerticalAxis, BatchedElementParameters, GWhiteTexture, bHitTesting, Gamma, NULL, &View, DepthTexture);
				RHICmdList.SetStencilRef(StencilRef);

				int32 MaxVerticesAllowed = ((GDrawUPVertexCheckCount / sizeof(FSimpleElementVertex)) / 2) * 2;
				/*
				hack to avoid a crash when trying to render large numbers of line segments.
				*/
				MaxVerticesAllowed = FMath::Min(MaxVerticesAllowed, 64 * 1024);

				int32 MinVertex=0;
				int32 TotalVerts = (LineVertices.Num() / 2) * 2;
				while( MinVertex < TotalVerts )
				{
					int32 NumLinePrims = FMath::Min( MaxVerticesAllowed, TotalVerts - MinVertex ) / 2;
					DrawPrimitiveUP(RHICmdList, PT_LineList, NumLinePrims, &LineVertices[MinVertex], sizeof(FSimpleElementVertex));
					MinVertex += NumLinePrims * 2;
				}
			}

			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			// Set the appropriate pixel shader parameters & shader state for the non-textured elements.
			PrepareShaders(RHICmdList, GraphicsPSOInit, FeatureLevel, SE_BLEND_Opaque, Transform, bNeedToSwitchVerticalAxis, BatchedElementParameters, GWhiteTexture, bHitTesting, Gamma, NULL, &View, DepthTexture);
			RHICmdList.SetStencilRef(StencilRef);

			// Draw points
			DrawPointElements(RHICmdList, Transform, ViewportSizeX, ViewportSizeY, CameraX, CameraY);

			if ( ThickLines.Num() > 0 )
			{
				float OrthoZoomFactor = 1.0f;

				const bool bIsPerspective = View.ViewMatrices.GetProjectionMatrix().M[3][3] < 1.0f ? true : false;
				if (!bIsPerspective)
				{
					OrthoZoomFactor = 1.0f / View.ViewMatrices.GetProjectionMatrix().M[0][0];
				}

				int32 LineIndex = 0;
				const int32 MaxLinesPerBatch = 2048;
				while (LineIndex < ThickLines.Num())
				{
					int32 FirstLineThisBatch = LineIndex;
					float DepthBiasThisBatch = ThickLines[LineIndex].DepthBias;
					while (++LineIndex < ThickLines.Num())
					{
						if ((ThickLines[LineIndex].DepthBias != DepthBiasThisBatch)
							|| ((LineIndex - FirstLineThisBatch) >= MaxLinesPerBatch))
						{
							break;
						}
					}
					int32 NumLinesThisBatch = LineIndex - FirstLineThisBatch;

					const bool bEnableMSAA = true;
					const bool bEnableLineAA = false;
					FRasterizerStateInitializerRHI Initializer = { FM_Solid, CM_None, 0, DepthBiasThisBatch, bEnableMSAA, bEnableLineAA };
					auto RasterState = RHICreateRasterizerState(Initializer);
					GraphicsPSOInit.RasterizerState = RasterState.GetReference();
					PrepareShaders(RHICmdList, GraphicsPSOInit, FeatureLevel, SE_BLEND_Translucent, Transform, bNeedToSwitchVerticalAxis, BatchedElementParameters, GWhiteTexture, bHitTesting, Gamma, NULL, &View, DepthTexture);
					RHICmdList.SetStencilRef(StencilRef);

					void* ThickVertexData = NULL;
					RHICmdList.BeginDrawPrimitiveUP(PT_TriangleList, 8 * NumLinesThisBatch, 8 * 3 * NumLinesThisBatch, sizeof(FSimpleElementVertex), ThickVertexData);
					FSimpleElementVertex* ThickVertices = (FSimpleElementVertex*)ThickVertexData;
					check(ThickVertices);

					for (int i = 0; i < NumLinesThisBatch; ++i)
					{
						const FBatchedThickLines& Line = ThickLines[FirstLineThisBatch + i];
						const float Thickness = FMath::Abs( Line.Thickness );

						const float StartW			= Transform.TransformFVector4(Line.Start).W;
						const float EndW			= Transform.TransformFVector4(Line.End).W;

						// Negative thickness means that thickness is calculated in screen space, positive thickness should be used for world space thickness.
						const float ScalingStart	= Line.bScreenSpace ? StartW / ViewportSizeX : 1.0f;
						const float ScalingEnd		= Line.bScreenSpace ? EndW   / ViewportSizeX : 1.0f;

						OrthoZoomFactor = Line.bScreenSpace ? OrthoZoomFactor : 1.0f;

						const float ScreenSpaceScaling = Line.bScreenSpace ? 2.0f : 1.0f;

						const float StartThickness	= Thickness * ScreenSpaceScaling * OrthoZoomFactor * ScalingStart;
						const float EndThickness	= Thickness * ScreenSpaceScaling * OrthoZoomFactor * ScalingEnd;

						const FVector WorldPointXS	= CameraX * StartThickness * 0.5f;
						const FVector WorldPointYS	= CameraY * StartThickness * 0.5f;

						const FVector WorldPointXE	= CameraX * EndThickness * 0.5f;
						const FVector WorldPointYE	= CameraY * EndThickness * 0.5f;

						// Generate vertices for the point such that the post-transform point size is constant.
						const FVector WorldPointX = CameraX * Thickness * StartW / ViewportSizeX;
						const FVector WorldPointY = CameraY * Thickness * StartW / ViewportSizeX;

						// Begin point
						ThickVertices[0] = FSimpleElementVertex(FVector4(Line.Start + WorldPointXS - WorldPointYS,1),FVector2D(1,0),Line.Color,Line.HitProxyId); // 0S
						ThickVertices[1] = FSimpleElementVertex(FVector4(Line.Start + WorldPointXS + WorldPointYS,1),FVector2D(1,1),Line.Color,Line.HitProxyId); // 1S
						ThickVertices[2] = FSimpleElementVertex(FVector4(Line.Start - WorldPointXS - WorldPointYS,1),FVector2D(0,0),Line.Color,Line.HitProxyId); // 2S
					
						ThickVertices[3] = FSimpleElementVertex(FVector4(Line.Start + WorldPointXS + WorldPointYS,1),FVector2D(1,1),Line.Color,Line.HitProxyId); // 1S
						ThickVertices[4] = FSimpleElementVertex(FVector4(Line.Start - WorldPointXS - WorldPointYS,1),FVector2D(0,0),Line.Color,Line.HitProxyId); // 2S
						ThickVertices[5] = FSimpleElementVertex(FVector4(Line.Start - WorldPointXS + WorldPointYS,1),FVector2D(0,1),Line.Color,Line.HitProxyId); // 3S

						// Ending point
						ThickVertices[0+ 6] = FSimpleElementVertex(FVector4(Line.End + WorldPointXE - WorldPointYE,1),FVector2D(1,0),Line.Color,Line.HitProxyId); // 0E
						ThickVertices[1+ 6] = FSimpleElementVertex(FVector4(Line.End + WorldPointXE + WorldPointYE,1),FVector2D(1,1),Line.Color,Line.HitProxyId); // 1E
						ThickVertices[2+ 6] = FSimpleElementVertex(FVector4(Line.End - WorldPointXE - WorldPointYE,1),FVector2D(0,0),Line.Color,Line.HitProxyId); // 2E
																																							  
						ThickVertices[3+ 6] = FSimpleElementVertex(FVector4(Line.End + WorldPointXE + WorldPointYE,1),FVector2D(1,1),Line.Color,Line.HitProxyId); // 1E
						ThickVertices[4+ 6] = FSimpleElementVertex(FVector4(Line.End - WorldPointXE - WorldPointYE,1),FVector2D(0,0),Line.Color,Line.HitProxyId); // 2E
						ThickVertices[5+ 6] = FSimpleElementVertex(FVector4(Line.End - WorldPointXE + WorldPointYE,1),FVector2D(0,1),Line.Color,Line.HitProxyId); // 3E

						// First part of line
						ThickVertices[0+12] = FSimpleElementVertex(FVector4(Line.Start - WorldPointXS - WorldPointYS,1),FVector2D(0,0),Line.Color,Line.HitProxyId); // 2S
						ThickVertices[1+12] = FSimpleElementVertex(FVector4(Line.Start + WorldPointXS + WorldPointYS,1),FVector2D(1,1),Line.Color,Line.HitProxyId); // 1S
						ThickVertices[2+12] = FSimpleElementVertex(FVector4(Line.End   - WorldPointXE - WorldPointYE,1),FVector2D(0,0),Line.Color,Line.HitProxyId); // 2E

						ThickVertices[3+12] = FSimpleElementVertex(FVector4(Line.Start + WorldPointXS + WorldPointYS,1),FVector2D(1,1),Line.Color,Line.HitProxyId); // 1S
						ThickVertices[4+12] = FSimpleElementVertex(FVector4(Line.End   + WorldPointXE + WorldPointYE,1),FVector2D(1,1),Line.Color,Line.HitProxyId); // 1E
						ThickVertices[5+12] = FSimpleElementVertex(FVector4(Line.End   - WorldPointXE - WorldPointYE,1),FVector2D(0,0),Line.Color,Line.HitProxyId); // 2E

						// Second part of line
						ThickVertices[0+18] = FSimpleElementVertex(FVector4(Line.Start - WorldPointXS + WorldPointYS,1),FVector2D(0,1),Line.Color,Line.HitProxyId); // 3S
						ThickVertices[1+18] = FSimpleElementVertex(FVector4(Line.Start + WorldPointXS - WorldPointYS,1),FVector2D(1,0),Line.Color,Line.HitProxyId); // 0S
						ThickVertices[2+18] = FSimpleElementVertex(FVector4(Line.End   - WorldPointXE + WorldPointYE,1),FVector2D(0,1),Line.Color,Line.HitProxyId); // 3E

						ThickVertices[3+18] = FSimpleElementVertex(FVector4(Line.Start + WorldPointXS - WorldPointYS,1),FVector2D(1,0),Line.Color,Line.HitProxyId); // 0S
						ThickVertices[4+18] = FSimpleElementVertex(FVector4(Line.End   + WorldPointXE - WorldPointYE,1),FVector2D(1,0),Line.Color,Line.HitProxyId); // 0E
						ThickVertices[5+18] = FSimpleElementVertex(FVector4(Line.End   - WorldPointXE + WorldPointYE,1),FVector2D(0,1),Line.Color,Line.HitProxyId); // 3E

						ThickVertices += 24;
					}
					RHICmdList.EndDrawPrimitiveUP();
				}

				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			}
			// Draw the wireframe triangles.
			if (WireTris.Num() > 0)
			{
				check(WireTriVerts.Num() == WireTris.Num() * 3);

				const bool bEnableMSAA = true;
				const bool bEnableLineAA = false;
				FRasterizerStateInitializerRHI Initializer = { FM_Wireframe, CM_None, 0, 0, bEnableMSAA, bEnableLineAA };

				int32 MaxVerticesAllowed = ((GDrawUPVertexCheckCount / sizeof(FSimpleElementVertex)) / 3) * 3;
				/*
				hack to avoid a crash when trying to render large numbers of line segments.
				*/
				MaxVerticesAllowed = FMath::Min(MaxVerticesAllowed, 64 * 1024);

				const int32 MaxTrisAllowed = MaxVerticesAllowed / 3;

				int32 MinTri=0;
				int32 TotalTris = WireTris.Num();
				while( MinTri < TotalTris )
				{
					int32 MaxTri = FMath::Min(MinTri + MaxTrisAllowed, TotalTris);
					float DepthBias = WireTris[MinTri].DepthBias;
					for (int32 i = MinTri + 1; i < MaxTri; ++i)
					{
						if (DepthBias != WireTris[i].DepthBias)
						{
							MaxTri = i;
							break;
						}
					}

					Initializer.DepthBias = DepthBias;
					auto RasterState = RHICreateRasterizerState(Initializer);
					GraphicsPSOInit.RasterizerState = RasterState.GetReference();
					PrepareShaders(RHICmdList, GraphicsPSOInit, FeatureLevel, SE_BLEND_Opaque, Transform, bNeedToSwitchVerticalAxis, BatchedElementParameters, GWhiteTexture, bHitTesting, Gamma, NULL, &View, DepthTexture);
					RHICmdList.SetStencilRef(StencilRef);

					int32 NumTris = MaxTri - MinTri;
					DrawPrimitiveUP(RHICmdList, PT_TriangleList, NumTris, &WireTriVerts[MinTri * 3], sizeof(FSimpleElementVertex));
					MinTri = MaxTri;
				}

				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			}
		}

		// Draw the sprites.
		if( Sprites.Num() > 0 )
		{
			// Sprites don't support batched element parameters (yet!)
			FBatchedElementParameters* BatchedElementParameters = NULL;

			int32 SpriteCount = Sprites.Num();
			//Sort sprites by texture
			/** Compare two texture pointers, return false if equal */
			struct FCompareTexture
			{
				FORCEINLINE bool operator()( const FBatchedElements::FBatchedSprite& SpriteA, const FBatchedElements::FBatchedSprite& SpriteB ) const
				{
					return (SpriteA.Texture == SpriteB.Texture && SpriteA.BlendMode == SpriteB.BlendMode) ? false : true; 
				}
			};

			Sprites.Sort(FCompareTexture());
			
			//First time init
			const FTexture* CurrentTexture = Sprites[0].Texture;
			ESimpleElementBlendMode CurrentBlendMode = (ESimpleElementBlendMode)Sprites[0].BlendMode;

			TArray<FSimpleElementVertex> SpriteList;
			for(int32 SpriteIndex = 0;SpriteIndex < SpriteCount;SpriteIndex++)
			{
				const FBatchedSprite& Sprite = Sprites[SpriteIndex];
				const EBlendModeFilter::Type SpriteFilter = GetBlendModeFilter((ESimpleElementBlendMode)Sprite.BlendMode);

				// Only render blend modes in the filter
				if (Filter & SpriteFilter)
				{
					if (CurrentTexture != Sprite.Texture || CurrentBlendMode != Sprite.BlendMode)
					{
						//New batch, draw previous and clear
						const int32 VertexCount = SpriteList.Num();
						const int32 PrimCount = VertexCount / 3;
						PrepareShaders(RHICmdList, GraphicsPSOInit, FeatureLevel, CurrentBlendMode, Transform, bNeedToSwitchVerticalAxis, BatchedElementParameters, CurrentTexture, bHitTesting, Gamma, NULL, &View, DepthTexture);
						RHICmdList.SetStencilRef(StencilRef);

						DrawPrimitiveUP(RHICmdList, PT_TriangleList, PrimCount, SpriteList.GetData(), sizeof(FSimpleElementVertex));

						SpriteList.Empty(6);
						CurrentTexture = Sprite.Texture;
						CurrentBlendMode = (ESimpleElementBlendMode)Sprite.BlendMode;
					}

					int32 SpriteListIndex = SpriteList.AddUninitialized(6);
					FSimpleElementVertex* Vertex = SpriteList.GetData();

					// Compute the sprite vertices.
					const FVector WorldSpriteX = CameraX * Sprite.SizeX;
					const FVector WorldSpriteY = CameraY * -Sprite.SizeY * GProjectionSignY;

					const float UStart = Sprite.U/Sprite.Texture->GetSizeX();
					const float UEnd = (Sprite.U + Sprite.UL)/Sprite.Texture->GetSizeX();
					const float VStart = Sprite.V/Sprite.Texture->GetSizeY();
					const float VEnd = (Sprite.V + Sprite.VL)/Sprite.Texture->GetSizeY();

					Vertex[SpriteListIndex + 0] = FSimpleElementVertex(FVector4(Sprite.Position + WorldSpriteX - WorldSpriteY,1),FVector2D(UEnd,  VStart),Sprite.Color,Sprite.HitProxyId);
					Vertex[SpriteListIndex + 1] = FSimpleElementVertex(FVector4(Sprite.Position + WorldSpriteX + WorldSpriteY,1),FVector2D(UEnd,  VEnd  ),Sprite.Color,Sprite.HitProxyId);
					Vertex[SpriteListIndex + 2] = FSimpleElementVertex(FVector4(Sprite.Position - WorldSpriteX - WorldSpriteY,1),FVector2D(UStart,VStart),Sprite.Color,Sprite.HitProxyId);

					Vertex[SpriteListIndex + 3] = FSimpleElementVertex(FVector4(Sprite.Position + WorldSpriteX + WorldSpriteY,1),FVector2D(UEnd,  VEnd  ),Sprite.Color,Sprite.HitProxyId);
					Vertex[SpriteListIndex + 4] = FSimpleElementVertex(FVector4(Sprite.Position - WorldSpriteX - WorldSpriteY,1),FVector2D(UStart,VStart),Sprite.Color,Sprite.HitProxyId);
					Vertex[SpriteListIndex + 5] = FSimpleElementVertex(FVector4(Sprite.Position - WorldSpriteX + WorldSpriteY,1),FVector2D(UStart,VEnd  ),Sprite.Color,Sprite.HitProxyId);
				}
			}

			if (SpriteList.Num() > 0)
			{
				const EBlendModeFilter::Type SpriteFilter = GetBlendModeFilter(CurrentBlendMode);

				// Only render blend modes in the filter
				if (Filter & SpriteFilter)
				{
					//Draw last batch
					const int32 VertexCount = SpriteList.Num();
					const int32 PrimCount = VertexCount / 3;
					PrepareShaders(RHICmdList, GraphicsPSOInit, FeatureLevel, CurrentBlendMode, Transform, bNeedToSwitchVerticalAxis, BatchedElementParameters, CurrentTexture, bHitTesting, Gamma, NULL, &View, DepthTexture);
					RHICmdList.SetStencilRef(StencilRef);

					DrawPrimitiveUP(RHICmdList, PT_TriangleList, PrimCount, SpriteList.GetData(), sizeof(FSimpleElementVertex));
				}
			}
		}

		if( MeshElements.Num() > 0)
		{
			// Draw the mesh elements.
			for(int32 MeshIndex = 0;MeshIndex < MeshElements.Num();MeshIndex++)
			{
				const FBatchedMeshElement& MeshElement = MeshElements[MeshIndex];
				const EBlendModeFilter::Type MeshFilter = GetBlendModeFilter(MeshElement.BlendMode);

				// Only render blend modes in the filter
				if (Filter & MeshFilter)
				{
					// Set the appropriate pixel shader for the mesh.
					PrepareShaders(RHICmdList, GraphicsPSOInit, FeatureLevel, MeshElement.BlendMode, Transform, bNeedToSwitchVerticalAxis, MeshElement.BatchedElementParameters, MeshElement.Texture, bHitTesting, Gamma, &MeshElement.GlowInfo, &View);
					RHICmdList.SetStencilRef(StencilRef);

					// Draw the mesh.
					DrawIndexedPrimitiveUP(
						RHICmdList,
						PT_TriangleList,
						0,
						MeshElement.MaxVertex - MeshElement.MinVertex + 1,
						MeshElement.Indices.Num() / 3,
						MeshElement.Indices.GetData(),
						sizeof(uint16),
						&MeshVertices[MeshElement.MinVertex],
						sizeof(FSimpleElementVertex)
						);
				}
			}
		}

		return true;
	}
	else
	{
		return false;
	}
}

void FBatchedElements::Clear()
{
	LineVertices.Empty();
	Points.Empty();
	Sprites.Empty();
	MeshElements.Empty();
	ThickLines.Empty();
}

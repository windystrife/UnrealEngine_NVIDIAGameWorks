// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BatchedElements.h: Batched element rendering.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Templates/RefCounting.h"
#include "RHI.h"
#include "RenderResource.h"
#include "HitProxies.h"
#include "SceneView.h"
#include "StaticBoundShaderState.h"

struct FBatchedPoint;
struct FDrawingPolicyRenderState;

namespace EBlendModeFilter
{
	enum Type
	{
		None = 0,
		OpaqueAndMasked = 1,
		Translucent = 2,
		All = (OpaqueAndMasked | Translucent)
	};
};

/** The type used to store batched line vertices. */
struct FSimpleElementVertex
{
	FVector4 Position;
	FVector2D TextureCoordinate;
	FLinearColor Color;
	FColor HitProxyIdColor;

	FSimpleElementVertex() {}

	FSimpleElementVertex(const FVector4& InPosition,const FVector2D& InTextureCoordinate,const FLinearColor& InColor,FHitProxyId InHitProxyId):
		Position(InPosition),
		TextureCoordinate(InTextureCoordinate),
		Color(InColor),
		HitProxyIdColor(InHitProxyId.GetColor())
	{}
};

/**
* The simple element vertex declaration resource type.
*/
class FSimpleElementVertexDeclaration : public FRenderResource
{
public:

	FVertexDeclarationRHIRef VertexDeclarationRHI;

	// Destructor.
	virtual ~FSimpleElementVertexDeclaration() {}

	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		uint16 Stride = sizeof(FSimpleElementVertex);
		Elements.Add(FVertexElement(0,STRUCT_OFFSET(FSimpleElementVertex,Position),VET_Float4,0,Stride));
		Elements.Add(FVertexElement(0,STRUCT_OFFSET(FSimpleElementVertex,TextureCoordinate),VET_Float2,1,Stride));
		Elements.Add(FVertexElement(0,STRUCT_OFFSET(FSimpleElementVertex,Color),VET_Float4,2,Stride));
		Elements.Add(FVertexElement(0,STRUCT_OFFSET(FSimpleElementVertex,HitProxyIdColor),VET_Color,3,Stride));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** The simple element vertex declaration. */
extern ENGINE_API TGlobalResource<FSimpleElementVertexDeclaration> GSimpleElementVertexDeclaration;



/** Custom parameters for batched element shaders.  Derive from this class to implement your shader bindings. */
class FBatchedElementParameters
	: public FRefCountedObject
{

public:

	/** Binds vertex and pixel shaders for this element */
	virtual void BindShaders(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, ERHIFeatureLevel::Type InFeatureLevel, const FMatrix& InTransform, const float InGamma, const FMatrix& ColorWeights, const FTexture* Texture) = 0;

};



/** Batched elements for later rendering. */
class ENGINE_API FBatchedElements
{
public:

	/**
	* Constructor 
	*/
	FBatchedElements()
		:	MaxMeshIndicesAllowed(GDrawUPIndexCheckCount / sizeof(int32))
			// the index buffer is 2 bytes, so make sure we only address 0xFFFF vertices in the index buffer
		,	MaxMeshVerticesAllowed(FMath::Min<uint32>(0xFFFF, GDrawUPVertexCheckCount / sizeof(FSimpleElementVertex)))
		,	bEnableHDREncoding(true)
	{
	}

	/** Adds a line to the batch. Note only SE_BLEND_Opaque will be used for batched line rendering. */
	void AddLine(const FVector& Start,const FVector& End,const FLinearColor& Color,FHitProxyId HitProxyId, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false);

	/** Adds a translucent line to the batch. */
	void AddTranslucentLine(const FVector& Start, const FVector& End, const FLinearColor& Color, FHitProxyId HitProxyId, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false);

	/** Adds a point to the batch. Note only SE_BLEND_Opaque will be used for batched point rendering. */
	void AddPoint(const FVector& Position,float Size,const FLinearColor& Color,FHitProxyId HitProxyId);

	/** Adds a mesh vertex to the batch. */
	int32 AddVertex(const FVector4& InPosition,const FVector2D& InTextureCoordinate,const FLinearColor& InColor,FHitProxyId HitProxyId);

	/** Adds a triangle to the batch. */
	void AddTriangle(int32 V0,int32 V1,int32 V2,const FTexture* Texture,EBlendMode BlendMode);

	/** Adds a triangle to the batch. */
	void AddTriangle(int32 V0, int32 V1, int32 V2, const FTexture* Texture, ESimpleElementBlendMode BlendMode, const FDepthFieldGlowInfo& GlowInfo = FDepthFieldGlowInfo());

	/** Adds a triangle to the batch. */
	void AddTriangle(int32 V0,int32 V1,int32 V2,FBatchedElementParameters* BatchedElementParameters,ESimpleElementBlendMode BlendMode);

	/** 
	* Reserves space in index array for a mesh element for current number plus expected number.
	* 
	* @param NumMeshTriangles - number of triangles to reserve space for
	* @param Texture - used to find the mesh element entry
	* @param BlendMode - used to find the mesh element entry
	*/
	void AddReserveTriangles(int32 NumMeshTriangles,const FTexture* Texture,ESimpleElementBlendMode BlendMode);

	/** 
	* Reserves space in index array for a mesh element
	* 
	* @param NumMeshTriangles - number of triangles to reserve space for
	* @param Texture - used to find the mesh element entry
	* @param BlendMode - used to find the mesh element entry
	*/
	void ReserveTriangles(int32 NumMeshTriangles,const FTexture* Texture,ESimpleElementBlendMode BlendMode);
	
	/** 
	* Reserves space in mesh vertex array for current number plus expected number.
	* 
	* @param NumMeshVerts - number of verts to reserve space for
	* @param Texture - used to find the mesh element entry
	* @param BlendMode - used to find the mesh element entry
	*/
	void AddReserveVertices(int32 NumMeshVerts);

	/** 
	* Reserves space in mesh vertex array for at least this many total verts.
	* 
	* @param NumMeshVerts - number of verts to reserve space for
	* @param Texture - used to find the mesh element entry
	* @param BlendMode - used to find the mesh element entry
	*/
	void ReserveVertices(int32 NumMeshVerts);

	/** 
	 * Reserves space in line vertex array
	 * 
	 * @param NumLines - number of lines to reserve space for
	 * @param bDepthBiased - whether reserving depth-biased lines or non-biased lines
	 * @param bThickLines - whether reserving regular lines or thick lines
	 */
	void AddReserveLines(int32 NumLines, bool bDepthBiased = false, bool bThickLines = false);

	/** Adds a sprite to the batch. */
	void AddSprite(
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
		uint8 BlendMode = SE_BLEND_Masked
		);

	/** 
	 *Draws the batch
	 *
	 * @param Transform	The transform matrix for each viewport (View->ViewProjectionMatrix)
	 * @param ViewportSizeX	The width of the viewport
	 * @param ViewportSizeY The height of the viewport
	 * @param bHitTesting	Whether or not we are hit testing
	 * @param Gamma			Optional gamma override
	 * @param View			Optional FSceneView for shaders that need access to view constants
	 * @param DepthTexture	DepthTexture for manual depth testing with editor compositing in the pixel shader
	 */
	DEPRECATED(4.14, "Deprecated. Use the FBatchedElements::Draw method that takes a non-optional FSceneView parameter instead")
	bool Draw(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, ERHIFeatureLevel::Type FeatureLevel, bool bNeedToSwitchVerticalAxis, const FMatrix& Transform, uint32 ViewportSizeX, uint32 ViewportSizeY, bool bHitTesting, float Gamma = 1.0f, const FSceneView* View = nullptr, FTexture2DRHIRef DepthTexture = FTexture2DRHIRef(), EBlendModeFilter::Type Filter = EBlendModeFilter::All) const;
	
	/**
	 * Draws the batch
	 *
	 * @param View			FSceneView for shaders that need access to view constants. Non-optional to also reference its ViewProjectionMatrix and size of the ViewRect
	 * @param bHitTesting	Whether or not we are hit testing
	 * @param Gamma			Optional gamma override
	 * @param DepthTexture	DepthTexture for manual depth testing with editor compositing in the pixel shader
	 */
	bool Draw(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, ERHIFeatureLevel::Type FeatureLevel, bool bNeedToSwitchVerticalAxis, const FSceneView& View, bool bHitTesting, float Gamma = 1.0f, FTexture2DRHIRef DepthTexture = FTexture2DRHIRef(), EBlendModeFilter::Type Filter = EBlendModeFilter::All) const;

	/**
	 * Creates a proxy FSceneView for operations that are not tied directly to a scene but still require batched elements to be drawn.
	 */
	static FSceneView CreateProxySceneView(const FMatrix& ProjectionMatrix, const FIntRect& ViewRect);

	FORCEINLINE bool HasPrimsToDraw() const
	{
		return( LineVertices.Num() || Points.Num() || Sprites.Num() || MeshElements.Num() || ThickLines.Num() || WireTris.Num() > 0 );
	}

	/** Adds a triangle to the batch. Extensive version where all parameters can be passed in. */
	void AddTriangleExtensive(int32 V0,int32 V1,int32 V2,FBatchedElementParameters* BatchedElementParameters,const FTexture* Texture,ESimpleElementBlendMode BlendMode, const FDepthFieldGlowInfo& GlowInfo = FDepthFieldGlowInfo());

	/** Clears any batched elements **/
	void Clear();

	/** 
	 * Helper function to return the amount of memory allocated by this class 
	 *
	 * @return number of bytes allocated by this container
	 */
	FORCEINLINE uint32 GetAllocatedSize( void ) const
	{
		return sizeof(*this) + Points.GetAllocatedSize() + WireTris.GetAllocatedSize() + WireTriVerts.GetAllocatedSize() + ThickLines.GetAllocatedSize()
			+ Sprites.GetAllocatedSize() + MeshElements.GetAllocatedSize() + MeshVertices.GetAllocatedSize();
	}

	void EnableMobileHDREncoding(bool bInEnableHDREncoding)
	{
		bEnableHDREncoding = bInEnableHDREncoding;
	}

private:

	/**
	 * Draws points
	 *
	 * @param	Transform	Transformation matrix to use
	 * @param	ViewportSizeX	Horizontal viewport size in pixels
	 * @param	ViewportSizeY	Vertical viewport size in pixels
	 * @param	CameraX		Local space normalized view direction X vector
	 * @param	CameraY		Local space normalized view direction Y vector
	 */
	void DrawPointElements(FRHICommandList& RHICmdList, const FMatrix& Transform, const uint32 ViewportSizeX, const uint32 ViewportSizeY, const FVector& CameraX, const FVector& CameraY) const;

	TArray<FSimpleElementVertex> LineVertices;

	struct FBatchedPoint
	{
		FVector Position;
		float Size;
		FColor Color;
		FHitProxyId HitProxyId;
	};
	TArray<FBatchedPoint> Points;

	struct FBatchedWireTris
	{
		float DepthBias;
	};
	TArray<FBatchedWireTris> WireTris;
	TArray<FSimpleElementVertex> WireTriVerts;

	struct FBatchedThickLines
	{
		FVector Start;
		FVector End;
		float Thickness;
		FLinearColor Color;
		FHitProxyId HitProxyId;
		float DepthBias;
		uint32 bScreenSpace;
	};
	TArray<FBatchedThickLines> ThickLines;

	struct FBatchedSprite
	{
		FVector Position;
		float SizeX;
		float SizeY;
		const FTexture* Texture;
		FLinearColor Color;
		FHitProxyId HitProxyId;
		float U;
		float UL;
		float V;
		float VL;
		uint8 BlendMode;
	};
	/** This array is sorted during draw-calls */
	mutable TArray<FBatchedSprite> Sprites;

	struct FBatchedMeshElement
	{
		/** starting index in vertex buffer for this batch */
		uint32 MinVertex;
		/** largest vertex index used by this batch */
		uint32 MaxVertex;
		/** index buffer for triangles */
		TArray<uint16,TInlineAllocator<6> > Indices;
		/** all triangles in this batch draw with the same texture */
		const FTexture* Texture;
		/** Parameters for this batched element */
		TRefCountPtr<FBatchedElementParameters> BatchedElementParameters;
		/** all triangles in this batch draw with the same blend mode */
		ESimpleElementBlendMode BlendMode;
		/** all triangles in this batch draw with the same depth field glow (depth field blend modes only) */
		FDepthFieldGlowInfo GlowInfo;
	};

	/** Max number of mesh index entries that will fit in a DrawPriUP call */
	int32 MaxMeshIndicesAllowed;
	/** Max number of mesh vertices that will fit in a DrawPriUP call */
	int32 MaxMeshVerticesAllowed;

	TArray<FBatchedMeshElement,TInlineAllocator<2> > MeshElements;
	TArray<FSimpleElementVertex,TInlineAllocator<4> > MeshVertices;

	/** bound shader state for the fast path */
	class FSimpleElementBSSContainer
	{
		static const uint32 NumBSS = (uint32)SE_BLEND_RGBA_MASK_START;
		FGlobalBoundShaderState UnencodedBSS;
		FGlobalBoundShaderState EncodedBSS[NumBSS];
	public:
		FGlobalBoundShaderState& GetBSS(bool bEncoded, ESimpleElementBlendMode BlendMode)
		{
			if (bEncoded)
			{
				check((uint32)BlendMode < NumBSS);
				return EncodedBSS[BlendMode];
			}
			else
			{
				return UnencodedBSS;
			}
		}
	};

	/**
	 * Sets the appropriate vertex and pixel shader.
	 */
	void PrepareShaders(
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
		const FDepthFieldGlowInfo* GlowInfo = NULL,
		const FSceneView* View = NULL,
		FTexture2DRHIRef DepthTexture = FTexture2DRHIRef()
		) const;

	/** if false then prevent the use of HDR encoded shaders. */
	bool bEnableHDREncoding;
};


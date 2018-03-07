// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rendering/RenderingCommon.h"

class FSlateBatchData;
class FSlateDrawElement;
class FSlateDrawLayer;
class FSlateElementBatch;
class FSlateRenderingPolicy;
class FSlateShaderResource;
class FSlateWindowElementList;
struct FShaderParams;

/**
 * A class which batches Slate elements for rendering
 */
class SLATECORE_API FSlateElementBatcher
{
public:

	FSlateElementBatcher( TSharedRef<FSlateRenderingPolicy> InRenderingPolicy );
	~FSlateElementBatcher();

	/** 
	 * Batches elements to be rendered 
	 * 
	 * @param DrawElements	The elements to batch
	 */
	void AddElements( FSlateWindowElementList& ElementList );

	/**
	 * Returns true if the elements in this batcher require v-sync.
	 */
	bool RequiresVsync() const { return bRequiresVsync; }

	/** Whether or not any post process passes were batched */
	bool HasFXPassses() const { return NumPostProcessPasses > 0;}

	/** 
	 * Resets all stored data accumulated during the batching process
	 */
	void ResetBatches();

private:
	void AddElements(const TArray<FSlateDrawElement>& DrawElements, const FVector2D& ViewportSize);
	
	FColor PackVertexColor(const FLinearColor& InLinearColor);

	/** 
	 * Creates vertices necessary to draw a Quad element 
	 */
	template<ESlateVertexRounding Rounding>
	void AddQuadElement( const FSlateDrawElement& DrawElement, FColor Color = FColor::White);

	/** 
	 * Creates vertices necessary to draw a 3x3 element
	 */
	template<ESlateVertexRounding Rounding>
	void AddBoxElement( const FSlateDrawElement& DrawElement );

	/** 
	 * Creates vertices necessary to draw a string (one quad per character)
	 */
	template<ESlateVertexRounding Rounding>
	void AddTextElement( const FSlateDrawElement& DrawElement );

	/** 
	 * Creates vertices necessary to draw a shaped glyph sequence (one quad per glyph)
	 */
	template<ESlateVertexRounding Rounding>
	void AddShapedTextElement( const FSlateDrawElement& DrawElement );

	/** 
	 * Creates vertices necessary to draw a gradient box (horizontal or vertical)
	 */
	template<ESlateVertexRounding Rounding>
	void AddGradientElement( const FSlateDrawElement& DrawElement );

	/** 
	 * Creates vertices necessary to draw a spline (Bezier curve)
	 */
	template<ESlateVertexRounding Rounding>
	void AddSplineElement( const FSlateDrawElement& DrawElement );

	/** 
	 * Creates vertices necessary to draw a series of attached line segments
	 */
	template<ESlateVertexRounding Rounding>
	void AddLineElement( const FSlateDrawElement& DrawElement );
	
	/** 
	 * Creates vertices necessary to draw a viewport (just a textured quad)
	 */
	template<ESlateVertexRounding Rounding>
	void AddViewportElement( const FSlateDrawElement& DrawElement );

	/** 
	 * Creates vertices necessary to draw a border element
	 */
	template<ESlateVertexRounding Rounding>
	void AddBorderElement( const FSlateDrawElement& DrawElement );

	/**
	 * Batches a custom slate drawing element
	 *
	 * @param Position		The top left screen space position of the element
	 * @param Size			The size of the element
	 * @param Scale			The amount to scale the element by
	 * @param InPayload		The data payload for this element
	 * @param DrawEffects	DrawEffects to apply
	 * @param Layer			The layer to draw this element in
	 */
	void AddCustomElement( const FSlateDrawElement& DrawElement );

	void AddCustomVerts( const FSlateDrawElement& DrawElement );

	void AddCachedBuffer( const FSlateDrawElement& DrawElement );

	void AddLayer(const FSlateDrawElement& DrawElement);

	void AddPostProcessPass(const FSlateDrawElement& DrawElement, const FVector2D& WindowSize);

	/** 
	 * Finds an batch for an element based on the passed in parameters
	 * Elements with common parameters and layers will batched together.
	 *
	 * @param Layer			The layer where this element should be drawn (signifies draw order)
	 * @param ShaderParams	The shader params for this element
	 * @param InTexture		The texture to use in the batch
	 * @param PrimitiveType	The primitive type( triangles, lines ) to use when drawing the batch
	 * @param ShaderType	The shader to use when rendering this batch
	 * @param DrawFlags		Any optional draw flags for this batch
	 * @param ScissorRect   Optional scissor rectangle for this batch
	 * @param SceneIndex    Index in the slate renderer's scenes array associated with this element.
	 */
	FSlateElementBatch& FindBatchForElement( uint32 Layer, 
											 const FShaderParams& ShaderParams, 
											 const FSlateShaderResource* InTexture, 
											 ESlateDrawPrimitive::Type PrimitiveType, 
											 ESlateShader::Type ShaderType, 
											 ESlateDrawEffect DrawEffects, 
											 ESlateBatchDrawFlag DrawFlags,
											 int32 ClippingIndex,
											 int32 SceneIndex = -1);
private:
	/** Batch data currently being filled in */
	FSlateBatchData* BatchData;

	/** The draw layer currently being accumulated */
	FSlateDrawLayer* DrawLayer;

	/** Rendering policy we were created from */
	FSlateRenderingPolicy* RenderingPolicy;

	/** Track the number of drawn batches from the previous frame to report to stats. */
	int32 NumDrawnBatchesStat;

	/** Track the number of drawn boxes from the previous frame to report to stats. */
	int32 NumDrawnBoxesStat;

	/** Track the number of drawn texts from the previous frame to report to stats. */
	int32 NumDrawnTextsStat;

	/** How many post process passes are needed */
	int32 NumPostProcessPasses;

	/** Offset to use when supporting 1:1 texture to pixel snapping */
	const float PixelCenterOffset;

	/** Are the vertex colors expected to be in sRGB space? */
	const bool bSRGBVertexColor;

	// true if any element in the batch requires vsync.
	bool bRequiresVsync;
};

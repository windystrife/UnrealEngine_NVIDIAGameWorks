// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The different types of relevance a primitive scene proxy can declare towards a particular scene view.
 * the class is only storing bits, and has an |= operator
 */
struct FPrimitiveViewRelevance
{
	// Warning: This class is memzeroed externally as 0 is assumed a
	// valid value for all members meaning 'not relevant'. If this
	// changes existing class usage should be re-evaluated

	// from FMaterialRelevance (could be made the base class):

	/** The LightingProfile supported by this primitive, as a bitmask. */
	uint16 ShadingModelMaskRelevance;
	/** The primitive has one or more opaque or masked elements. */
	uint32 bOpaqueRelevance : 1;
	/** The primitive has one or more masked elements. */
	uint32 bMaskedRelevance : 1;
	/** The primitive has one or more distortion elements. */
	uint32 bDistortionRelevance : 1;
	/** The primitive has one or more elements that have SeparateTranslucency. */
	uint32 bSeparateTranslucencyRelevance : 1;
	/** The primitive has one or more elements that have normal translucency. */
	uint32 bNormalTranslucencyRelevance : 1;
	/** For translucent primitives reading the scene color. */
	uint32 bUsesSceneColorCopy : 1;
	/** For primitive that can't render in offscreen buffers (blend modulate). */
	uint32 bDisableOffscreenRendering : 1;
	/** */
	uint32 bUsesGlobalDistanceField : 1;

	// others:

	/** The primitive's static elements are rendered for the view. */
	uint32 bStaticRelevance : 1; 
	/** The primitive's dynamic elements are rendered for the view. */
	uint32 bDynamicRelevance : 1;
	/** The primitive is drawn. */
	uint32 bDrawRelevance : 1;
	/** The primitive is casting a shadow. */
	uint32 bShadowRelevance : 1;
	/** The primitive should render to the custom depth pass. */
	uint32 bRenderCustomDepth : 1;
	/** The primitive should render to the base pass / normal depth / velocity rendering. */
	uint32 bRenderInMainPass : 1;
	/** The primitive has materials using the volume domain. */
	uint32 bHasVolumeMaterialDomain : 1;
	/** The primitive is drawn only in the editor and composited onto the scene after post processing */
	uint32 bEditorPrimitiveRelevance : 1;
	/** The primitive's static elements are selected and rendered again in the selection outline pass*/
	uint32 bEditorStaticSelectionRelevance : 1;
	/** The primitive is drawn only in the editor and composited onto the scene after post processing using no depth testing */
	uint32 bEditorNoDepthTestPrimitiveRelevance : 1;
	/** The primitive should have GatherSimpleLights called on the proxy when gathering simple lights. */
	uint32 bHasSimpleLights : 1;
	/** The primitive has one or more elements that have World Position Offset. */
	uint32 bUsesWorldPositionOffset : 1;
	/** Whether the primitive uses non-default lighting channels. */
	uint32 bUsesLightingChannels : 1;
	/** */
	uint32 bDecal : 1;
	/** Whether the primitive has materals that use translucent surface lighting. */
	uint32 bTranslucentSurfaceLighting : 1;
	/** Whether the primitive has materials that read the scene depth. */
	uint32 bUsesSceneDepth : 1;

	// @third party code - BEGIN HairWorks
	/** The primitive is a HairWorks instance. */
	uint32 bHairWorks : 1;
	// @third party code - END HairWorks

	/** 
	 * Whether this primitive view relevance has been initialized this frame.  
	 * Primitives that have not had ComputeRelevanceForView called on them (because they were culled) will not be initialized,
	 * But we may still need to render them from other views like shadow passes, so this tracks whether we can reuse the cached relevance or not.
	 */
	uint32 bInitializedThisFrame : 1;

	bool HasTranslucency() const 
	{
		return bSeparateTranslucencyRelevance || bNormalTranslucencyRelevance;
	}

	/** Default constructor */
	FPrimitiveViewRelevance()
	{
		// the class is only storing bits, the following avoids code redundancy
		uint8 * RESTRICT p = (uint8*)this;
		for(uint32 i = 0; i < sizeof(*this); ++i)
		{
			*p++ = 0;
		}

		// only exceptions (bugs we need to fix?):

		bOpaqueRelevance = true;
		// without it BSP doesn't render
		bRenderInMainPass = true;
	}

	/** Bitwise OR operator.  Sets any relevance bits which are present in either. */
	FPrimitiveViewRelevance& operator|=(const FPrimitiveViewRelevance& B)
	{
		// the class is only storing bits, the following avoids code redundancy
		const uint8 * RESTRICT s = (const uint8*)&B;
		uint8 * RESTRICT d = (uint8*)this;
		for(uint32 i = 0; i < sizeof(*this); ++i)
		{
			*d = *d | *s; 
			++s;++d;
		}
		return *this;
	}
};

///////////////////////////////////////////////////////////////////////
//  RenderState.inl
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) CONFIDENTIAL AND PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization, Inc. and
//  may not be copied, disclosed, or exploited except in accordance with 
//  the terms of that agreement.
//
//      Copyright (c) 2003-2014 IDV, Inc.
//      All rights reserved in all media.
//
//      IDV, Inc.
//      http://www.idvinc.com


///////////////////////////////////////////////////////////////////////  
//  SRenderState::SRenderState

ST_INLINE SRenderState::SRenderState( ) :
	// lighting model
	m_eLightingModel(LIGHTING_MODEL_PER_VERTEX),
	// ambient
	m_vAmbientColor(1.0f, 1.0f, 1.0f),
	m_eAmbientContrast(EFFECT_OFF),
	m_fAmbientContrastFactor(0.0f),
	m_bAmbientOcclusion(false),
	// diffuse
	m_vDiffuseColor(1.0f, 1.0f, 1.0f),
	m_fDiffuseScalar(1.0f),
	m_bDiffuseAlphaMaskIsOpaque(false),
	// detail
	m_eDetailLayer(EFFECT_OFF),
	// specular
	m_eSpecular(EFFECT_OFF),
	m_fShininess(30.0f),
	m_vSpecularColor(1.0f, 1.0f, 1.0f),
	// transmission
	m_eTransmission(EFFECT_OFF),
	m_vTransmissionColor(1.0f, 1.0f, 0.0f),
	m_fTransmissionShadowBrightness(0.2f),
	m_fTransmissionViewDependency(0.5f),
	// branch seam smoothing
	m_eBranchSeamSmoothing(EFFECT_OFF),
	m_fBranchSeamWeight(1.0f),
	// LOD parameters
	m_eLodMethod(LOD_METHOD_POP),
	m_bFadeToBillboard(true),
	m_bVertBillboard(false),
	m_bHorzBillboard(false),
	// render states
	m_eShaderGenerationMode(SHADER_GEN_MODE_STANDARD),
	m_bUsedAsGrass(false),
	m_eFaceCulling(CULLTYPE_NONE),
	m_bBlending(false),
	// image-based ambient lighting
	m_eAmbientImageLighting(EFFECT_OFF),
	m_eHueVariation(EFFECT_OFF),
	// fog
	m_eFogCurve(FOG_CURVE_NONE),
	m_eFogColorStyle(FOG_COLOR_TYPE_CONSTANT),
	// shadows
	m_bCastsShadows(false),
	m_bReceivesShadows(false),
	m_bShadowSmoothing(false),
	// alpha effects
	m_fAlphaScalar(1.4f),
	// wind
	m_eWindLod(WIND_LOD_NONE),
	// non-lighting shader
	m_eRenderPass(RENDER_PASS_MAIN),
	// geometry
	m_bBranchesPresent(false),
	m_bFrondsPresent(false),
	m_bLeavesPresent(false),
	m_bFacingLeavesPresent(false),
	m_bRigidMeshesPresent(false),
	// misc
	m_pDescription(NULL),
	m_pUserData(NULL)
{
	for (st_int32 i = 0; i < TL_NUM_TEX_LAYERS; ++i)
		m_apTextures[i] = NULL;
}


///////////////////////////////////////////////////////////////////////  
//  Helper function: StringsEqual

ST_INLINE st_bool StringsEqual(const char* pA, const char* pB)
{
	st_bool bEqual = false;

	if (pA && pB)
		bEqual = (strcmp(pA, pB) == 0);
	else if (!pA && !pB)
		bEqual = true;

	return bEqual;
}


///////////////////////////////////////////////////////////////////////  
//  Helper function: StringLessThan

ST_INLINE st_bool StringLessThan(const char* pA, const char* pB)
{
	st_bool bLess = false;

	if (pA && pB)
		bLess = (strcmp(pA, pB) < 0);

	return bLess;
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::operator==

ST_INLINE st_bool SRenderState::operator==(const SRenderState& sRight) const
{
			// textures
	return (StringsEqual(m_apTextures[TL_DIFFUSE],			   sRight.m_apTextures[TL_DIFFUSE]) &&
			StringsEqual(m_apTextures[TL_NORMAL],			   sRight.m_apTextures[TL_NORMAL]) &&
			StringsEqual(m_apTextures[TL_DETAIL_DIFFUSE],	   sRight.m_apTextures[TL_DETAIL_DIFFUSE]) &&
			StringsEqual(m_apTextures[TL_DETAIL_NORMAL],	   sRight.m_apTextures[TL_DETAIL_NORMAL]) &&
			StringsEqual(m_apTextures[TL_SPECULAR_MASK],	   sRight.m_apTextures[TL_SPECULAR_MASK]) &&
			StringsEqual(m_apTextures[TL_TRANSMISSION_MASK],   sRight.m_apTextures[TL_TRANSMISSION_MASK]) &&
			// lighting model
			m_eLightingModel								== sRight.m_eLightingModel &&
			// ambient
			m_vAmbientColor									== sRight.m_vAmbientColor &&
			m_eAmbientContrast								== sRight.m_eAmbientContrast &&
			m_fAmbientContrastFactor						== sRight.m_fAmbientContrastFactor &&
			m_bAmbientOcclusion								== sRight.m_bAmbientOcclusion &&
			// diffuse
			m_vDiffuseColor									== sRight.m_vDiffuseColor &&
			m_fDiffuseScalar								== sRight.m_fDiffuseScalar &&
			m_bDiffuseAlphaMaskIsOpaque						== sRight.m_bDiffuseAlphaMaskIsOpaque &&
			// detail
			m_eDetailLayer									== sRight.m_eDetailLayer &&
			// specular
			m_eSpecular									    == sRight.m_eSpecular &&
			m_fShininess									== sRight.m_fShininess &&
			m_vSpecularColor								== sRight.m_vSpecularColor &&
			// transmission
			m_eTransmission									== sRight.m_eTransmission &&
			m_vTransmissionColor							== sRight.m_vTransmissionColor &&
			m_fTransmissionShadowBrightness					== sRight.m_fTransmissionShadowBrightness &&
			m_fTransmissionViewDependency					== sRight.m_fTransmissionViewDependency &&
			// branch seam smoothing
			m_eBranchSeamSmoothing							== sRight.m_eBranchSeamSmoothing &&
			m_fBranchSeamWeight								== sRight.m_fBranchSeamWeight &&
			// LOD parameters
			m_eLodMethod									== sRight.m_eLodMethod &&
			m_bFadeToBillboard								== sRight.m_bFadeToBillboard &&
			m_bVertBillboard								== sRight.m_bVertBillboard &&
			//
			m_eHueVariation									== sRight.m_eHueVariation &&
			// render states
			m_eShaderGenerationMode							== sRight.m_eShaderGenerationMode &&
			m_bUsedAsGrass								    == sRight.m_bUsedAsGrass &&
			m_eFaceCulling									== sRight.m_eFaceCulling &&
			m_bBlending										== sRight.m_bBlending &&
			// image-based ambient lighting
			m_eAmbientImageLighting							== sRight.m_eAmbientImageLighting &&
			// fog
			m_eFogCurve										== sRight.m_eFogCurve &&
			m_eFogColorStyle								== sRight.m_eFogColorStyle &&
			// shadows
			m_bCastsShadows									== sRight.m_bCastsShadows &&
			m_bReceivesShadows								== sRight.m_bReceivesShadows &&
			m_bShadowSmoothing								== sRight.m_bShadowSmoothing &&
			// alpha effects
			m_fAlphaScalar									== sRight.m_fAlphaScalar &&
			// wind
			m_eWindLod										== sRight.m_eWindLod &&
			// non-lighting
			m_eRenderPass									== sRight.m_eRenderPass &&
			// geometry types
			m_bBranchesPresent								== sRight.m_bBranchesPresent &&
			m_bFrondsPresent								== sRight.m_bFrondsPresent &&
			m_bLeavesPresent								== sRight.m_bLeavesPresent &&
			m_bFacingLeavesPresent							== sRight.m_bFacingLeavesPresent &&
			m_bRigidMeshesPresent							== sRight.m_bRigidMeshesPresent &&
			// misc
			StringsEqual(m_pDescription,					   sRight.m_pDescription) &&
			StringsEqual(m_pUserData,						   sRight.m_pUserData));
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::operator!=

ST_INLINE st_bool SRenderState::operator!=(const SRenderState& sRight) const
{
	return !(*this == sRight);
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::IsPerPixelModelActive
//
//	This is primarily used by the Compiler application.

ST_INLINE st_bool SRenderState::IsPerPixelModelActive(void) const
{
	return (m_eLightingModel == LIGHTING_MODEL_PER_PIXEL ||
		    m_eLightingModel == LIGHTING_MODEL_PER_VERTEX_X_PER_PIXEL ||
			m_eLightingModel == LIGHTING_MODEL_DEFERRED);
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::IsLightingModelInTransition
//
//	This is primarily used by the Compiler to determine if a fade value needs
//	to be passed from the vertex shader to the pixel shader.

ST_INLINE st_bool SRenderState::IsLightingModelInTransition(void) const
{
	return (m_eLightingModel == LIGHTING_MODEL_PER_VERTEX_X_PER_PIXEL);
}


///////////////////////////////////////////////////////////////////////  
//  Function: Get5xModeHashName

inline CFixedString Get5xModeHashName(const SRenderState& sRenderState)
{
	CFixedString strName;

	if (sRenderState.m_bVertBillboard)
	{
		strName = "Billboard";
	}
	else if (sRenderState.m_bUsedAsGrass)
	{
		strName = "Grass";
	}
	else
	{
		if (sRenderState.m_bBranchesPresent)
			strName = "Branches";
		else if (sRenderState.m_bFrondsPresent)
			strName = "FrondsAndCaps";
		else if (sRenderState.m_bLeavesPresent)
			strName = "Leaves";
		else if (sRenderState.m_bFacingLeavesPresent)
			strName = "FacingLeaves";
		else
			strName = "RigidMeshes";
	}

	if (sRenderState.m_eRenderPass == RENDER_PASS_DEPTH_PREPASS)
		strName += "_depthprepass";
	else if (sRenderState.m_eRenderPass == RENDER_PASS_SHADOW_CAST)
		strName += "_shadowcast";

	return strName;
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::VertexShaderHashName
//
//	The hash name is based on the member variables in SRenderState that
//	affect the shader code.

inline CFixedString SRenderState::VertexShaderHashName(const CWind& cWind, EShadowConfig eShadowConfig) const
{
	CFixedString strHashName;

	if (ShaderGenHasFixedDecls( ))
	{
		strHashName = Get5xModeHashName(*this);
	}
	else
	{
		if (!m_bVertBillboard)
		{
			// encode vertex decl
			strHashName += VertexDeclHash( );

			// add pixel decl
			strHashName += PixelDeclHash( );

			// diffuse texture has solid alpha channel
			if (m_bDiffuseAlphaMaskIsOpaque)
				strHashName += "dmo";

			// encode things that contribute to vertex shader behavior that aren't reflected in the vertex decl
			if (m_eRenderPass == RENDER_PASS_MAIN)
			{
				// fade/transition flags
				st_int32 nFadeFlags = 0;
				if (m_eAmbientContrast == EFFECT_OFF_X_ON)
					nFadeFlags += 1;
				if (m_eDetailLayer == EFFECT_OFF_X_ON)
					nFadeFlags += 2;
				if (m_eSpecular == EFFECT_OFF_X_ON)
					nFadeFlags += 4;
				if (m_eTransmission == EFFECT_OFF_X_ON)
					nFadeFlags += 8;
				if (nFadeFlags > 0)
					strHashName += CFixedString::Format("f%x", nFadeFlags);

				// special face culling / transmission state
				if (m_eFaceCulling == CULLTYPE_BACK && m_eTransmission != EFFECT_OFF)
					strHashName += "b";

				// grass
				if (m_bUsedAsGrass)
					strHashName += "g";

				// shadows
				if (eShadowConfig > SHADOW_CONFIG_OFF && m_bReceivesShadows)
					strHashName += CFixedString::Format("s%d", eShadowConfig);
			}
			else if (m_eRenderPass == RENDER_PASS_DEPTH_PREPASS)
				strHashName += "_depthprepass";
			else if (m_eRenderPass == RENDER_PASS_SHADOW_CAST)
				strHashName += "_shadowcast";

			// wind options
			if (m_eWindLod != WIND_LOD_NONE)
			{
				st_uint32 uiWindOptions = 0;
				st_uint32 uiBit = 1;
				assert(CWind::NUM_WIND_OPTIONS <= 32);

				// global wind
				if (IsGlobalWindEnabled( ))
				{
					for (st_int32 i = CWind::GLOBAL_WIND; i <= CWind::GLOBAL_PRESERVE_SHAPE; ++i)
					{
						if (cWind.IsOptionEnabled(CWind::EOptions(i)))
							uiWindOptions += uiBit;
						uiBit *= 2;
					}
				}

				// branch wind
				if (IsBranchWindEnabled( ))
				{
					for (st_int32 i = CWind::BRANCH_SIMPLE_1; i <= CWind::BRANCH_OSC_COMPLEX_2; ++i)
					{
						if (cWind.IsOptionEnabled(CWind::EOptions(i)))
							uiWindOptions += uiBit;
						uiBit *= 2;
					}
				}

				// leaf wind 1
				if (IsFullWindEnabled( ) && (m_bLeavesPresent || m_bFacingLeavesPresent))
				{
					for (st_int32 i = CWind::LEAF_RIPPLE_VERTEX_NORMAL_1; i <= CWind::LEAF_OCCLUSION_1; ++i)
					{
						if (cWind.IsOptionEnabled(CWind::EOptions(i)))
							uiWindOptions += uiBit;
						uiBit *= 2;
					}

					for (st_int32 i = CWind::LEAF_RIPPLE_VERTEX_NORMAL_2; i <= CWind::LEAF_OCCLUSION_2; ++i)
					{
						if (cWind.IsOptionEnabled(CWind::EOptions(i)))
							uiWindOptions += uiBit;
						uiBit *= 2;
					}
				}

				// frond ripple
				if (IsFullWindEnabled( ) && m_bFrondsPresent)
				{
					for (st_int32 i = CWind::FROND_RIPPLE_ONE_SIDED; i <= CWind::FROND_RIPPLE_ADJUST_LIGHTING; ++i)
					{
						if (cWind.IsOptionEnabled(CWind::EOptions(i)))
							uiWindOptions += uiBit;
						uiBit *= 2;
					}
				}

				strHashName += CFixedString::Format("_w%d%x", m_eWindLod, uiWindOptions);
			}
		}
		// billboard name
		else
		{
			// base billboard name
			strHashName = "Billboard";

			// horz suffix
			if (m_bHorzBillboard)
				strHashName += "_horz";

			// wind suffix
			if (m_eWindLod != WIND_LOD_NONE)
				strHashName += "_wind";

			// depth/shadow suffix
			if (m_eRenderPass == RENDER_PASS_DEPTH_PREPASS)
				strHashName += "_depthprepass";
			else if (m_eRenderPass == RENDER_PASS_SHADOW_CAST)
				strHashName += "_shadowcast";
		}
	}

	return strHashName;
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::PixelShaderHashName
//
//	The hash name is based on the member variables in SRenderState that
//	affect the shader code.

inline CFixedString SRenderState::PixelShaderHashName(EShadowConfig eShadowConfig) const
{
	CFixedString strHashName;

	if (ShaderGenHasFixedDecls( ))
	{
		strHashName = Get5xModeHashName(*this);
	}
	else
	{
		if (m_bVertBillboard)
		{
			strHashName = (m_eRenderPass == RENDER_PASS_MAIN) ? "Billboard" : "Billboard_do";
		}
		else
		{
			strHashName = PixelDeclHash( );

			// diffuse texture has solid alpha channel
			if (m_bDiffuseAlphaMaskIsOpaque)
				strHashName += "dmo";

			// encode things that contribute to pixel shader behavior that aren't reflected in the pixel decl
			if (m_eRenderPass == RENDER_PASS_MAIN)
			{
				// lighting effects flags
				st_int32 nEffectFlags = 0;
				if (m_eDetailLayer != EFFECT_OFF)
					nEffectFlags += st_int32(m_eDetailLayer);
				if (m_eSpecular != EFFECT_OFF)
					nEffectFlags += st_int32(m_eSpecular) * 4;
				if (m_eTransmission != EFFECT_OFF)
					nEffectFlags += st_int32(m_eTransmission) * 8;
				if (m_eAmbientContrast != EFFECT_OFF)
					nEffectFlags += st_int32(m_eTransmission) * 16;
				if (nEffectFlags > 0)
					strHashName += CFixedString::Format("_e%x", nEffectFlags);

				// grass
				if (m_bUsedAsGrass)
					strHashName += "grs";

				// shadows
				if (eShadowConfig > SHADOW_CONFIG_OFF && m_bReceivesShadows)
					strHashName += CFixedString::Format("_s%d", eShadowConfig);
			}
			else
			{
				// depth-only suffix
				strHashName += "_do";
			}
		}
	}

	return strHashName;
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::GetPixelProperties

ST_INLINE void SRenderState::GetPixelProperties(st_bool abPixelProperties[PIXEL_PROPERTY_COUNT]) const
{
	st_bool bDeferred = (m_eLightingModel == LIGHTING_MODEL_DEFERRED);

	for (st_int32 i = 0; i < PIXEL_PROPERTY_COUNT; ++i)
		abPixelProperties[i] = false;

	// always needed
	abPixelProperties[PIXEL_PROPERTY_POSITION] = true;
	abPixelProperties[PIXEL_PROPERTY_DIFFUSE_TEXCOORDS] = true;

	// detail texcoords
	bool bDetailLayerActive = false; // used later
	if (m_eRenderPass == RENDER_PASS_MAIN && m_eDetailLayer != EFFECT_OFF)
	{
		abPixelProperties[PIXEL_PROPERTY_DETAIL_TEXCOORDS] = true;
		bDetailLayerActive = true;
	}

	// lighting NBT
	if (m_eRenderPass == RENDER_PASS_MAIN)
	{
		if (bDeferred)
		{
			abPixelProperties[PIXEL_PROPERTY_NORMAL] = true;
			abPixelProperties[PIXEL_PROPERTY_TANGENT] = true;
		}
		else
		{
			if (IsPerPixelModelActive( ))
				abPixelProperties[PIXEL_PROPERTY_NORMAL_MAP_VECTOR] = true;
			else
				abPixelProperties[PIXEL_PROPERTY_PER_VERTEX_LIGHTING_COLOR] = true;
		}
	}

	// effects that only matter when using forward rendering
	if (!bDeferred && m_eRenderPass == RENDER_PASS_MAIN)
	{
		// specular
		if (m_eSpecular != EFFECT_OFF)
		{
			if (IsPerPixelModelActive( ))
			{
				abPixelProperties[PIXEL_PROPERTY_SPECULAR_HALF_VECTOR] = true;
				if (IsLightingModelInTransition( ))
					abPixelProperties[PIXEL_PROPERTY_PER_VERTEX_SPECULAR_DOT] = true;
			}
			else
				abPixelProperties[PIXEL_PROPERTY_PER_VERTEX_SPECULAR_DOT] = true;
		}

		// ambient contrast
		if (m_eAmbientContrast != EFFECT_OFF && m_eLightingModel != LIGHTING_MODEL_PER_PIXEL)
			abPixelProperties[PIXEL_PROPERTY_PER_VERTEX_AMBIENT_CONTRAST] = true;

		// transmission lighting
		if (m_eTransmission != EFFECT_OFF)
			abPixelProperties[PIXEL_PROPERTY_TRANSMISSION_FACTOR] = true;

		if (IsLightingModelInTransition( ))
			abPixelProperties[PIXEL_PROPERTY_PER_VERTEX_LIGHTING_COLOR] = true;

		// fog
		if (m_eFogCurve != FOG_CURVE_NONE)
		{
			abPixelProperties[PIXEL_PROPERTY_FOG_SCALAR] = true;
			if (m_eFogColorStyle == FOG_COLOR_TYPE_DYNAMIC)
				abPixelProperties[PIXEL_PROPERTY_FOG_COLOR] = true;
		}
	}

	if (m_eRenderPass == RENDER_PASS_MAIN)
	{
		// ambient occlusion
		if (m_bAmbientOcclusion && (IsPerPixelModelActive( ) || m_eLightingModel == LIGHTING_MODEL_DEFERRED))
			abPixelProperties[PIXEL_PROPERTY_AMBIENT_OCCLUSION] = true;

		// hue variation
		if (m_eHueVariation != EFFECT_OFF)
			abPixelProperties[PIXEL_PROPERTY_HUE_VARIATION] = true;

		// image-based ambient lighting
		if (m_eAmbientImageLighting != EFFECT_OFF)
			abPixelProperties[PIXEL_PROPERTY_NORMAL] = true;

		// render effects fading
		if (IsLightingModelInTransition( ) ||
			m_eAmbientContrast == EFFECT_OFF_X_ON ||
			m_eDetailLayer == EFFECT_OFF_X_ON ||
			m_eSpecular == EFFECT_OFF_X_ON ||
			m_eTransmission == EFFECT_OFF_X_ON ||
			m_eBranchSeamSmoothing == EFFECT_OFF_X_ON ||
			m_eHueVariation == EFFECT_OFF_X_ON ||
			m_eAmbientImageLighting == EFFECT_OFF_X_ON)
		{
			abPixelProperties[PIXEL_PROPERTY_RENDER_EFFECT_FADE] = true;
		}

		// branch seam
		if (m_eBranchSeamSmoothing != EFFECT_OFF)
		{
			// diffuse
			abPixelProperties[PIXEL_PROPERTY_BRANCH_SEAM_DIFFUSE] = true;

			// detail
			if (bDetailLayerActive)
				abPixelProperties[PIXEL_PROPERTY_BRANCH_SEAM_DETAIL] = true;
		}
	}

	// LOD
	if (ShaderGenHasFixedDecls( ) || m_bFadeToBillboard || m_bUsedAsGrass)
		abPixelProperties[PIXEL_PROPERTY_FADE_TO_BILLBOARD] = true;
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::GetInstanceType

ST_INLINE SVertexDecl::EInstanceType SRenderState::GetInstanceType(void) const
{
	SVertexDecl::EInstanceType eInstanceType = SVertexDecl::INSTANCES_NONE;

	if (m_bUsedAsGrass)
		eInstanceType = SVertexDecl::INSTANCES_GRASS;
	else if (m_bVertBillboard || m_bHorzBillboard)
		eInstanceType = SVertexDecl::INSTANCES_BILLBOARDS;
	else if (m_bBranchesPresent || 
			 m_bFrondsPresent || 
			 m_bLeavesPresent ||
			 m_bFacingLeavesPresent || 
			 m_bRigidMeshesPresent)
		eInstanceType = SVertexDecl::INSTANCES_3D_TREES;

	return eInstanceType;
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::ShaderGenHasFixedDecls

ST_INLINE st_bool SRenderState::ShaderGenHasFixedDecls(void) const
{
	return (m_eShaderGenerationMode == SpeedTree::SHADER_GEN_MODE_SPEEDTREE_5X_STYLE ||
			m_eShaderGenerationMode == SpeedTree::SHADER_GEN_MODE_UNREAL_ENGINE_4 ||
			m_eShaderGenerationMode == SpeedTree::SHADER_GEN_MODE_UNIFIED_SHADERS);

}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::ShaderGenerationMode

ST_INLINE EShaderGenerationMode SRenderState::ShaderGenerationMode(void) const
{
	return m_eShaderGenerationMode;
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::IsTextureLayerPresent

ST_INLINE st_bool SRenderState::IsTextureLayerPresent(ETextureLayer eLayer) const
{
	return m_apTextures[eLayer] && (strlen(m_apTextures[eLayer]) > 0);
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::VertexDeclHash

inline CFixedString SRenderState::VertexDeclHash(void) const
{
	st_int32 nEncodedVertexDecl = 0;
	st_int32 nBit = 1;
	for (st_int32 i = 0; i < VERTEX_PROPERTY_COUNT; ++i)
	{
		const SVertexDecl::SProperty& sProp = m_sVertexDecl.m_asProperties[i];

		if (sProp.IsPresent( ))
			nEncodedVertexDecl += nBit;

		nBit *= 2;
	}

	return CFixedString::Format("%x", nEncodedVertexDecl);
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::PixelDeclHash

inline CFixedString SRenderState::PixelDeclHash(void) const
{
	CFixedString strHash;

	// get a bit vector with active pixel properties
	st_bool abPixelProperties[PIXEL_PROPERTY_COUNT];
	GetPixelProperties(abPixelProperties);

	// encode bit vector into single value
	st_int32 nEncodedPixelDecl = 0;
	st_int32 nBit = 1;
	for (st_int32 i = 0; i < PIXEL_PROPERTY_COUNT; ++i)
	{
		if (abPixelProperties[i])
			nEncodedPixelDecl += nBit;

		nBit *= 2;
	}

	return CFixedString::Format("%x", nEncodedPixelDecl);
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::IsBranchWindEnabled

ST_INLINE st_bool SRenderState::IsBranchWindEnabled(void) const
{
	return (m_eWindLod == WIND_LOD_BRANCH ||
			m_eWindLod == WIND_LOD_FULL ||
			m_eWindLod == WIND_LOD_NONE_X_BRANCH ||
			m_eWindLod == WIND_LOD_NONE_X_FULL ||
			m_eWindLod == WIND_LOD_GLOBAL_X_BRANCH ||
			m_eWindLod == WIND_LOD_GLOBAL_X_FULL ||
			m_eWindLod == WIND_LOD_BRANCH_X_FULL);
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::IsGlobalWindEnabled

ST_INLINE st_bool SRenderState::IsGlobalWindEnabled(void) const
{
	return (m_eWindLod != WIND_LOD_NONE);
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::IsFullWindEnabled

ST_INLINE st_bool SRenderState::IsFullWindEnabled(void) const
{
	return (m_eWindLod == WIND_LOD_FULL ||
			m_eWindLod == WIND_LOD_NONE_X_FULL ||
			m_eWindLod == WIND_LOD_GLOBAL_X_FULL ||
			m_eWindLod == WIND_LOD_BRANCH_X_FULL);
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::MakeDepthOnly

ST_INLINE void SRenderState::MakeDepthOnly(void)
{
	// clear unneeded textures (need only diffuse map)
	for (st_int32 i = TL_DIFFUSE + 1; i < TL_NUM_TEX_LAYERS; ++i)
		m_apTextures[i] = "";

	// depth-only support
	m_eRenderPass = RENDER_PASS_DEPTH_PREPASS;
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::MakeShadowCast

ST_INLINE void SRenderState::MakeShadowCast(void)
{
	// clear unneeded textures (need only diffuse map)
	for (st_int32 i = TL_DIFFUSE + 1; i < TL_NUM_TEX_LAYERS; ++i)
		m_apTextures[i] = "";

	// known states
	m_bFadeToBillboard = false;
	
	// shadow-casting support
	m_eRenderPass = RENDER_PASS_SHADOW_CAST;
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::HasOnlyBranches

ST_INLINE st_bool SRenderState::HasOnlyBranches(void) const
{
	return (m_bBranchesPresent && !m_bFrondsPresent && !m_bFacingLeavesPresent && !m_bLeavesPresent && !m_bRigidMeshesPresent);
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::HasOnlyFronds

ST_INLINE st_bool SRenderState::HasOnlyFronds(void) const
{
	return (!m_bBranchesPresent && m_bFrondsPresent && !m_bFacingLeavesPresent && !m_bLeavesPresent && !m_bRigidMeshesPresent);
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::HasOnlyLeaves

ST_INLINE st_bool SRenderState::HasOnlyLeaves(st_bool bFacing, st_bool bNonFacing) const
{
	st_bool bReturn = false;

	if (!bFacing && !bNonFacing)
		bReturn = false;
	else if (bFacing && !bNonFacing)
		bReturn = (!m_bBranchesPresent && !m_bFrondsPresent && m_bFacingLeavesPresent && !m_bLeavesPresent && !m_bRigidMeshesPresent);
	else if (!bFacing && bNonFacing)
		bReturn = (!m_bBranchesPresent && !m_bFrondsPresent && !m_bFacingLeavesPresent && m_bLeavesPresent && !m_bRigidMeshesPresent);
	else
		bReturn = (!m_bBranchesPresent && !m_bFrondsPresent && m_bFacingLeavesPresent && m_bLeavesPresent && !m_bRigidMeshesPresent);

	return bReturn;
}


///////////////////////////////////////////////////////////////////////  
//  SRenderState::HasOnlyRigidMeshes

ST_INLINE st_bool SRenderState::HasOnlyRigidMeshes(void) const
{
	return (!m_bBranchesPresent && !m_bFrondsPresent && !m_bFacingLeavesPresent && !m_bLeavesPresent && m_bRigidMeshesPresent);
}

///////////////////////////////////////////////////////////////////////
//  Misc.inl
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
//  SRenderPass::SRenderPass

ST_INLINE SRenderPass::SRenderPass(EShaderPass eShaderType, 
								st_bool bDrawToShadowMap) :
	m_eShaderType(eShaderType),
	m_bDrawToShadowMap(bDrawToShadowMap)
{
}


///////////////////////////////////////////////////////////////////////
//  SSimpleMaterial::SSimpleMaterial

ST_INLINE SSimpleMaterial::SSimpleMaterial( ) :
	m_vAmbient(0.2f, 0.2f, 0.2f),
	m_vDiffuse(1.0f, 1.0f, 1.0f),
	m_vSpecular(1.0f, 1.0f, 1.0f),
	m_vTransmission(0.0f, 0.0f, 0.0f),
	m_fShininess(25.0f)
{
}

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SystemTextures.cpp: System textures implementation.
=============================================================================*/

#include "AtmosphereTextures.h"
#include "Atmosphere/AtmosphericFogComponent.h"
#include "PostProcess/RenderTargetPool.h"
#include "AtmosphereTextureParameters.h"
#include "ShaderParameterUtils.h"

void FAtmosphereTextures::InitDynamicRHI()
{
	check(PrecomputeParams != NULL);
	// Allocate atmosphere precompute textures...
	{
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

		// todo: Expose
		// Transmittance
		FIntPoint GTransmittanceTexSize(PrecomputeParams->TransmittanceTexWidth, PrecomputeParams->TransmittanceTexHeight);
		FPooledRenderTargetDesc TransmittanceDesc(FPooledRenderTargetDesc::Create2DDesc(GTransmittanceTexSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_None, TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, TransmittanceDesc, AtmosphereTransmittance, TEXT("AtmosphereTransmittance"));
		{
			FRHIRenderTargetView View = FRHIRenderTargetView(AtmosphereTransmittance->GetRenderTargetItem().TargetableTexture, ERenderTargetLoadAction::EClear);
			FRHISetRenderTargetsInfo Info(1, &View, FRHIDepthRenderTargetView());
			RHICmdList.SetRenderTargetsAndClear(Info);
			RHICmdList.CopyToResolveTarget(AtmosphereTransmittance->GetRenderTargetItem().TargetableTexture, AtmosphereTransmittance->GetRenderTargetItem().ShaderResourceTexture, true, FResolveParams());
		}

		// Irradiance
		FIntPoint GIrradianceTexSize(PrecomputeParams->IrradianceTexWidth, PrecomputeParams->IrradianceTexHeight);
		FPooledRenderTargetDesc IrradianceDesc(FPooledRenderTargetDesc::Create2DDesc(GIrradianceTexSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_None, TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, IrradianceDesc, AtmosphereIrradiance, TEXT("AtmosphereIrradiance"));
		{
			FRHIRenderTargetView View = FRHIRenderTargetView(AtmosphereIrradiance->GetRenderTargetItem().TargetableTexture, ERenderTargetLoadAction::EClear);
			FRHISetRenderTargetsInfo Info(1, &View, FRHIDepthRenderTargetView());
			RHICmdList.SetRenderTargetsAndClear(Info);
			RHICmdList.CopyToResolveTarget(AtmosphereIrradiance->GetRenderTargetItem().TargetableTexture, AtmosphereIrradiance->GetRenderTargetItem().ShaderResourceTexture, true, FResolveParams());
		}
		// DeltaE
		GRenderTargetPool.FindFreeElement(RHICmdList, IrradianceDesc, AtmosphereDeltaE, TEXT("AtmosphereDeltaE"));

		// 3D Texture
		// Inscatter
		FPooledRenderTargetDesc InscatterDesc(FPooledRenderTargetDesc::CreateVolumeDesc(PrecomputeParams->InscatterMuSNum * PrecomputeParams->InscatterNuNum, PrecomputeParams->InscatterMuNum, PrecomputeParams->InscatterAltitudeSampleNum, PF_FloatRGBA, FClearValueBinding::None, TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, InscatterDesc, AtmosphereInscatter, TEXT("AtmosphereInscatter"));

		// DeltaSR
		GRenderTargetPool.FindFreeElement(RHICmdList, InscatterDesc, AtmosphereDeltaSR, TEXT("AtmosphereDeltaSR"));

		// DeltaSM
		GRenderTargetPool.FindFreeElement(RHICmdList, InscatterDesc, AtmosphereDeltaSM, TEXT("AtmosphereDeltaSM"));

		// DeltaJ
		GRenderTargetPool.FindFreeElement(RHICmdList, InscatterDesc, AtmosphereDeltaJ, TEXT("AtmosphereDeltaJ"));
	}
}

void FAtmosphereTextures::ReleaseDynamicRHI()
{
	AtmosphereTransmittance.SafeRelease();
	AtmosphereIrradiance.SafeRelease();
	AtmosphereDeltaE.SafeRelease();

	AtmosphereInscatter.SafeRelease();
	AtmosphereDeltaSR.SafeRelease();
	AtmosphereDeltaSM.SafeRelease();
	AtmosphereDeltaJ.SafeRelease();

	GRenderTargetPool.FreeUnusedResources();
}


void FAtmosphereShaderTextureParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	TransmittanceTexture.Bind(ParameterMap,TEXT("AtmosphereTransmittanceTexture"));
	TransmittanceTextureSampler.Bind(ParameterMap,TEXT("AtmosphereTransmittanceTextureSampler"));
	IrradianceTexture.Bind(ParameterMap,TEXT("AtmosphereIrradianceTexture"));
	IrradianceTextureSampler.Bind(ParameterMap,TEXT("AtmosphereIrradianceTextureSampler"));
	InscatterTexture.Bind(ParameterMap,TEXT("AtmosphereInscatterTexture"));
	InscatterTextureSampler.Bind(ParameterMap,TEXT("AtmosphereInscatterTextureSampler"));
}


#define IMPLEMENT_ATMOSPHERE_TEXTURE_PARAM_SET( ShaderRHIParamRef ) \
	template void FAtmosphereShaderTextureParameters::Set< ShaderRHIParamRef >( FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FSceneView& View ) const;

IMPLEMENT_ATMOSPHERE_TEXTURE_PARAM_SET( FVertexShaderRHIParamRef );
IMPLEMENT_ATMOSPHERE_TEXTURE_PARAM_SET( FHullShaderRHIParamRef );
IMPLEMENT_ATMOSPHERE_TEXTURE_PARAM_SET( FDomainShaderRHIParamRef );
IMPLEMENT_ATMOSPHERE_TEXTURE_PARAM_SET( FGeometryShaderRHIParamRef );
IMPLEMENT_ATMOSPHERE_TEXTURE_PARAM_SET( FPixelShaderRHIParamRef );
IMPLEMENT_ATMOSPHERE_TEXTURE_PARAM_SET( FComputeShaderRHIParamRef );

FArchive& operator<<(FArchive& Ar,FAtmosphereShaderTextureParameters& Parameters)
{
	Ar << Parameters.TransmittanceTexture;
	Ar << Parameters.TransmittanceTextureSampler;
	Ar << Parameters.IrradianceTexture;
	Ar << Parameters.IrradianceTextureSampler;
	Ar << Parameters.InscatterTexture;
	Ar << Parameters.InscatterTextureSampler;
	return Ar;
}

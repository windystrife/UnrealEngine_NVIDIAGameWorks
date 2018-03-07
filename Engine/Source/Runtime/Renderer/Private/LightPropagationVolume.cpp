//-----------------------------------------------------------------------------
// File:		LightPropagationVolume.cpp
//
// Summary:		Light Propagation Volumes implementation 
//
// Created:		2013-03-01
//
// Author:		Ben Woodhouse - mailto:benwood@microsoft.com
//
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------

#include "LightPropagationVolume.h"
#include "RHIStaticStates.h"
#include "Shader.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"
#include "LightPropagationVolumeSettings.h"

DECLARE_FLOAT_COUNTER_STAT(TEXT("LPV"), Stat_GPU_LPV, STATGROUP_GPU);

static TAutoConsoleVariable<int32> CVarLightPropagationVolume(
	TEXT("r.LightPropagationVolume"),
	0,
	TEXT("Project setting of the work in progress feature LightPropgationVolume. Cannot be changed at runtime.\n")
	TEXT(" 0: off (default)\n")
	TEXT(" 1: on"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

struct LPVBufferElementUncompressed
{
	FVector elements[9];
	float AO;
}; 
// ----------------------------------------------------------------------------

static const uint32 LPV_GRIDRES = 32;
static float LPV_CENTRE_OFFSET = 10.0f;

// ----------------------------------------------------------------------------

struct VplListEntry
{
	uint32	normalPacked;
	uint32	fluxPacked;
	int		nextIndex;
};

struct FRsmInfo
{
	FVector4 ShadowmapMinMax;
	FMatrix WorldToShadow;
	FMatrix ShadowToWorld;
	float AreaBrightnessMultiplier; 
};


static TAutoConsoleVariable<float> CVarLPVSpecularIntensity(
	TEXT("r.LPV.SpecularIntensity"),
	0.333f,
	TEXT("Multiplier for LPV Specular.") );

static TAutoConsoleVariable<float> CVarLPVDiffuseIntensity(
	TEXT("r.LPV.DiffuseIntensity"),
	0.333f,
	TEXT("Multiplier for LPV Diffuse.") );

static TAutoConsoleVariable<float> CVarLPVIntensity(
	TEXT("r.LPV.Intensity"),
	1.0f,
	TEXT("Multiplier for LPV intensity. 1.0 is the default.") );

static TAutoConsoleVariable<int32> CVarNumPropagationSteps(
	TEXT("r.LPV.NumPropagationSteps"),
	3,
	TEXT("Number of LPV propagation steps") );

static TAutoConsoleVariable<int32> CVarLPVNumAOPropagationSteps(
	TEXT("r.LPV.NumAOPropagationSteps"),
	1,
	TEXT("Number of LPV AO propagation steps\n")
	TEXT("0: noisy (good for debugging)\n")
	TEXT("1: normal (default)\n")
	TEXT("2: blurry") );

static TAutoConsoleVariable<float> CVarLPVEmissiveIntensityMultiplier(
	TEXT("r.LPV.EmissiveMultiplier"),
	1.0f,
	TEXT("Emissive intensity multiplier") );

static TAutoConsoleVariable<float> CVarLPVDirectionalOcclusionDefaultDiffuse(
	TEXT("r.LPV.DirectionalOcclusionDefaultDiffuse"), 
	0.75f, 
	TEXT("") );

static TAutoConsoleVariable<float> CVarLPVDirectionalOcclusionDefaultSpecular(
	TEXT("r.LPV.DirectionalOcclusionDefaultSpecular"), 
	0.75f, 
	TEXT("") );

// ----------------------------------------------------------------------------

/**
 * Uniform buffer parameters for LPV direct injection shaders
 */
BEGIN_UNIFORM_BUFFER_STRUCT( FLpvDirectLightInjectParameters, )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, LightRadius )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, LightPosition )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, LightColor )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, LightFalloffExponent )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, LightSourceLength )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, LightDirection )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector2D, LightSpotAngles )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, bLightInverseSquaredAttenuation )
END_UNIFORM_BUFFER_STRUCT( FLpvDirectLightInjectParameters )

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FLpvDirectLightInjectParameters,TEXT("LpvInject"));

typedef TUniformBufferRef<FLpvDirectLightInjectParameters> FDirectLightInjectBufferRef;

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FLpvWriteUniformBufferParameters,TEXT("LpvWrite"));

// ----------------------------------------------------------------------------
// Base LPV Write Compute shader
// ----------------------------------------------------------------------------
class FLpvWriteShaderCSBase : public FGlobalShader
{
public:
	// Default constructor
	FLpvWriteShaderCSBase()	{	}

	/** Initialization constructor. */
	explicit FLpvWriteShaderCSBase( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		for ( int i = 0; i < 7; i++ )
		{
			LpvBufferSRVParameters[i].Bind(Initializer.ParameterMap, LpvVolumeTextureSRVNames[i] );
			LpvBufferUAVs[i].Bind(Initializer.ParameterMap, LpvVolumeTextureUAVNames[i] );
		}

		LpvVolumeTextureSampler.Bind( Initializer.ParameterMap, TEXT("gLpv3DTextureSampler") );
		VplListHeadBufferSRV.Bind(Initializer.ParameterMap, TEXT("gVplListHeadBuffer") );
		VplListHeadBufferUAV.Bind(Initializer.ParameterMap, TEXT("RWVplListHeadBuffer") );
		VplListBufferSRV.Bind(Initializer.ParameterMap, TEXT("gVplListBuffer") );
		VplListBufferUAV.Bind(Initializer.ParameterMap, TEXT("RWVplListBuffer") );

		for ( int i = 0; i < NUM_GV_TEXTURES; i++ ) 
		{
			GvBufferSRVParameters[i].Bind(Initializer.ParameterMap, LpvGvVolumeTextureSRVNames[i] );
			GvBufferUAVs[i].Bind(Initializer.ParameterMap, LpvGvVolumeTextureUAVNames[i] );
		}
		GvListBufferUAV.Bind(Initializer.ParameterMap, TEXT("RWGvListBuffer") );
		GvListHeadBufferUAV.Bind(Initializer.ParameterMap, TEXT("RWGvListHeadBuffer") );

		GvListBufferSRV.Bind(Initializer.ParameterMap, TEXT("gGvListBuffer") );
		GvListHeadBufferSRV.Bind(Initializer.ParameterMap, TEXT("gGvListHeadBuffer") );

		AOVolumeTextureUAV.Bind(Initializer.ParameterMap, TEXT("gAOVolumeTextureRW") );
		AOVolumeTextureSRV.Bind(Initializer.ParameterMap, TEXT("gAOVolumeTexture") );
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
		OutEnvironment.SetDefine( TEXT("LPV_MULTIPLE_BOUNCES"), (uint32)LPV_MULTIPLE_BOUNCES );
		OutEnvironment.SetDefine( TEXT("LPV_GV_SH_ORDER"),			(uint32)LPV_GV_SH_ORDER );
	}
	// Serialization
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );

		for(int i = 0; i < 7; i++)
		{
			Ar << LpvBufferSRVParameters[i];
			Ar << LpvBufferUAVs[i];
		}

		Ar << LpvVolumeTextureSampler;
		Ar << VplListHeadBufferSRV;
		Ar << VplListHeadBufferUAV;
		Ar << VplListBufferSRV;
		Ar << VplListBufferUAV;
		for ( int i = 0; i < NUM_GV_TEXTURES; i++ )
		{
			Ar << GvBufferSRVParameters[i];
			Ar << GvBufferUAVs[i];
		}
		Ar << GvListBufferUAV;
		Ar << GvListHeadBufferUAV;
		Ar << GvListBufferSRV;
		Ar << GvListHeadBufferSRV;
		Ar << AOVolumeTextureUAV;
		Ar << AOVolumeTextureSRV;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FLpvBaseWriteShaderParams& Params )
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ShaderRHI, GetUniformBufferParameter<FLpvWriteUniformBufferParameters>(), Params.UniformBuffer );

		TArray<int32> ResourceIndices;
		TArray<FUnorderedAccessViewRHIParamRef> UAVs;

		for(int i  =0; i < 7; i++)
		{
			if ( LpvBufferSRVParameters[i].IsBound() )
			{
				RHICmdList.SetShaderTexture(ShaderRHI, LpvBufferSRVParameters[i].GetBaseIndex(), Params.LpvBufferSRVs[i]);
			}
			if ( LpvBufferUAVs[i].IsBound() )
			{
				ResourceIndices.Add(LpvBufferUAVs[i].GetBaseIndex());
				UAVs.Add(Params.LpvBufferUAVs[i]);
			}
			SetTextureParameter(RHICmdList, ShaderRHI, LpvBufferSRVParameters[i], LpvVolumeTextureSampler, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), Params.LpvBufferSRVs[i] );
		}
		if ( VplListHeadBufferSRV.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter( ShaderRHI, VplListHeadBufferSRV.GetBaseIndex(), Params.VplListHeadBufferSRV );
		}
		if ( VplListHeadBufferUAV.IsBound() )
		{
			ResourceIndices.Add(VplListHeadBufferUAV.GetBaseIndex());
			UAVs.Add(Params.VplListHeadBufferUAV);
		}
		if ( VplListBufferSRV.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter( ShaderRHI, VplListBufferSRV.GetBaseIndex(), Params.VplListBufferSRV );
		}
		if ( VplListBufferUAV.IsBound() )
		{			
			ResourceIndices.Add(VplListBufferUAV.GetBaseIndex());
			UAVs.Add(Params.VplListBufferUAV);
		}

		// GV Volume texture
		for ( int i=0;i<NUM_GV_TEXTURES; i++ )
		{
			if ( GvBufferSRVParameters[i].IsBound() )
			{
				RHICmdList.SetShaderTexture(ShaderRHI, GvBufferSRVParameters[i].GetBaseIndex(), Params.GvBufferSRVs[i]);
			}
			if ( GvBufferUAVs[i].IsBound() )
			{
				ResourceIndices.Add(GvBufferUAVs[i].GetBaseIndex());
				UAVs.Add(Params.GvBufferUAVs[i]);				
			}

			SetTextureParameter(RHICmdList, ShaderRHI, GvBufferSRVParameters[i], LpvVolumeTextureSampler, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), Params.GvBufferSRVs[i] );
		}

		if(GvListBufferUAV.IsBound())
		{
			ResourceIndices.Add(GvListBufferUAV.GetBaseIndex());
			UAVs.Add(Params.GvListBufferUAV);
		}
		if(GvListHeadBufferUAV.IsBound())
		{
			ResourceIndices.Add(GvListHeadBufferUAV.GetBaseIndex());
			UAVs.Add(Params.GvListHeadBufferUAV);			
		}
		if(GvListBufferSRV.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ShaderRHI, GvListBufferSRV.GetBaseIndex(), Params.GvListBufferSRV);
		}
		if(GvListHeadBufferSRV.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ShaderRHI, GvListHeadBufferSRV.GetBaseIndex(), Params.GvListHeadBufferSRV);
		}
		if ( AOVolumeTextureUAV.IsBound() )
		{
			ResourceIndices.Add(AOVolumeTextureUAV.GetBaseIndex());
			UAVs.Add(Params.AOVolumeTextureUAV);			
		}
		if ( AOVolumeTextureSRV.IsBound() )
		{
			RHICmdList.SetShaderTexture(ShaderRHI, AOVolumeTextureSRV.GetBaseIndex(), Params.AOVolumeTextureSRV );
		}

		check(ResourceIndices.Num() == UAVs.Num());
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, UAVs.GetData(), UAVs.Num());
		for (int32 i = 0; i < ResourceIndices.Num(); ++i)
		{
			RHICmdList.SetUAVParameter(ShaderRHI, ResourceIndices[i], UAVs[i]);
		}
	}

	// Unbinds any buffers that have been bound.
	void UnbindBuffers(FRHICommandList& RHICmdList, const FLpvBaseWriteShaderParams& Params)
	{
		TArray<int32> ResourceIndices;
		TArray<FUnorderedAccessViewRHIParamRef> UAVs;

		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		for ( int i = 0; i < 7; i++ )
		{
			if ( LpvBufferSRVParameters[i].IsBound() )
		    {
				RHICmdList.SetShaderTexture(ShaderRHI, LpvBufferSRVParameters[i].GetBaseIndex(), FTextureRHIParamRef());
		    }
			if ( LpvBufferUAVs[i].IsBound() )
		    {
				ResourceIndices.Add(LpvBufferUAVs[i].GetBaseIndex());
				UAVs.Add(Params.LpvBufferUAVs[i]);			    
		    }
	    }
		if ( VplListHeadBufferSRV.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter( ShaderRHI, VplListHeadBufferSRV.GetBaseIndex(), FShaderResourceViewRHIParamRef() );
		}
		if ( VplListHeadBufferUAV.IsBound() )
		{
			ResourceIndices.Add(VplListHeadBufferUAV.GetBaseIndex());
			UAVs.Add(Params.VplListHeadBufferUAV);			
		}
		if ( VplListBufferSRV.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter( ShaderRHI, VplListBufferSRV.GetBaseIndex(), FShaderResourceViewRHIParamRef() );
		}
		if ( VplListBufferUAV.IsBound() )
		{
			ResourceIndices.Add(VplListBufferUAV.GetBaseIndex());
			UAVs.Add(Params.VplListBufferUAV);			
		}
		for ( int i = 0; i < NUM_GV_TEXTURES; i++ )
		{
			if ( GvBufferSRVParameters[i].IsBound() )
			{
					RHICmdList.SetShaderTexture(ShaderRHI, GvBufferSRVParameters[i].GetBaseIndex(), FTextureRHIParamRef());
			}
			if ( GvBufferUAVs[i].IsBound() )
			{
				ResourceIndices.Add(GvBufferUAVs[i].GetBaseIndex());
				UAVs.Add(Params.GvBufferUAVs[i]);
			}
		}

		if ( AOVolumeTextureUAV.IsBound() )
		{
			ResourceIndices.Add(AOVolumeTextureUAV.GetBaseIndex());
			UAVs.Add(Params.AOVolumeTextureUAV);
		}
		if ( AOVolumeTextureSRV.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter( ShaderRHI, AOVolumeTextureSRV.GetBaseIndex(), FShaderResourceViewRHIParamRef() );
		}
		if(GvListBufferUAV.IsBound())
		{
			ResourceIndices.Add(GvListBufferUAV.GetBaseIndex());
			UAVs.Add(Params.GvListBufferUAV);			
		}
		if(GvListHeadBufferUAV.IsBound())
		{
			ResourceIndices.Add(GvListHeadBufferUAV.GetBaseIndex());
			UAVs.Add(Params.GvListHeadBufferUAV);
		}
		if ( GvListBufferSRV.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter( ShaderRHI, GvListBufferSRV.GetBaseIndex(), FShaderResourceViewRHIParamRef() );
		}
		if ( GvListHeadBufferSRV.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter( ShaderRHI, GvListHeadBufferSRV.GetBaseIndex(), FShaderResourceViewRHIParamRef() );
		}

		check(ResourceIndices.Num() == UAVs.Num());
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, UAVs.GetData(), UAVs.Num());
		FUnorderedAccessViewRHIParamRef NullUAV = nullptr;
		for (int32 i = 0; i < ResourceIndices.Num(); ++i)
		{
			RHICmdList.SetUAVParameter(ShaderRHI, ResourceIndices[i], NullUAV);
		}
	}

protected:
	FShaderResourceParameter LpvBufferSRVParameters[7];
	FShaderResourceParameter LpvBufferUAVs[7];
	FShaderResourceParameter LpvVolumeTextureSampler;
	FShaderResourceParameter VplListHeadBufferSRV;
	FShaderResourceParameter VplListHeadBufferUAV;
	FShaderResourceParameter VplListBufferSRV;
	FShaderResourceParameter VplListBufferUAV;

	FShaderResourceParameter GvBufferSRVParameters[NUM_GV_TEXTURES];
	FShaderResourceParameter GvBufferUAVs[NUM_GV_TEXTURES];
	FShaderResourceParameter GvListBufferSRV;
	FShaderResourceParameter GvListBufferUAV;
	FShaderResourceParameter GvListHeadBufferSRV;
	FShaderResourceParameter GvListHeadBufferUAV;
	FShaderResourceParameter AOVolumeTextureUAV;
	FShaderResourceParameter AOVolumeTextureSRV;
};


// ----------------------------------------------------------------------------
// LPV clear Compute shader
// ----------------------------------------------------------------------------
class FLpvClearCS : public FLpvWriteShaderCSBase
{
	DECLARE_SHADER_TYPE(FLpvClearCS,Global);

public:
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && IsLPVSupported(Platform); }

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FLpvWriteShaderCSBase::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	FLpvClearCS()	{	}

	explicit FLpvClearCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvWriteShaderCSBase(Initializer)		{	}

	virtual bool Serialize( FArchive& Ar ) override			{ return FLpvWriteShaderCSBase::Serialize( Ar ); }
};
IMPLEMENT_SHADER_TYPE(,FLpvClearCS,TEXT("/Engine/Private/LPVClear.usf"),TEXT("CSClear"),SF_Compute);


// ----------------------------------------------------------------------------
// LPV clear geometryVolume Compute shader
// ----------------------------------------------------------------------------
class FLpvClearGeometryVolumeCS : public FLpvWriteShaderCSBase
{
	DECLARE_SHADER_TYPE(FLpvClearGeometryVolumeCS,Global);

public:
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && IsLPVSupported(Platform); }

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FLpvWriteShaderCSBase::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	FLpvClearGeometryVolumeCS()	{	}

	explicit FLpvClearGeometryVolumeCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvWriteShaderCSBase(Initializer)		{	}

	virtual bool Serialize( FArchive& Ar ) override			{ return FLpvWriteShaderCSBase::Serialize( Ar ); }
};
IMPLEMENT_SHADER_TYPE(,FLpvClearGeometryVolumeCS,TEXT("/Engine/Private/LPVClear.usf"),TEXT("CSClearGeometryVolume"),SF_Compute);


// ----------------------------------------------------------------------------
// LPV clear lists Compute shader
// ----------------------------------------------------------------------------
class FLpvClearListsCS : public FLpvWriteShaderCSBase
{
	DECLARE_SHADER_TYPE(FLpvClearListsCS,Global);

public:
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && IsLPVSupported(Platform); }

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FLpvWriteShaderCSBase::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	FLpvClearListsCS()	{	}

	explicit FLpvClearListsCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvWriteShaderCSBase(Initializer)		{	}

	virtual bool Serialize( FArchive& Ar ) override			{ return FLpvWriteShaderCSBase::Serialize( Ar ); }
};
IMPLEMENT_SHADER_TYPE(,FLpvClearListsCS,TEXT("/Engine/Private/LPVClearLists.usf"),TEXT("CSClearLists"),SF_Compute);

// ----------------------------------------------------------------------------
// LPV generate VPL lists compute shader (for a directional light)
// ----------------------------------------------------------------------------
class FLpvInject_GenerateVplListsCS : public FLpvWriteShaderCSBase
{
	DECLARE_SHADER_TYPE(FLpvInject_GenerateVplListsCS,Global);

public:
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && IsLPVSupported(Platform); }

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FLpvWriteShaderCSBase::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	FLpvInject_GenerateVplListsCS()	{	}

	explicit FLpvInject_GenerateVplListsCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvWriteShaderCSBase(Initializer)		
	{	
		RsmDiffuseTexture.Bind(		Initializer.ParameterMap, TEXT("gRsmFluxTex") );
		RsmNormalTexture.Bind(		Initializer.ParameterMap, TEXT("gRsmNormalTex") );
		RsmDepthTexture.Bind(		Initializer.ParameterMap, TEXT("gRsmDepthTex") );

		LinearTextureSampler.Bind(	Initializer.ParameterMap, TEXT("LinearSampler") );
		PointTextureSampler.Bind(	Initializer.ParameterMap, TEXT("PointSampler") );
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		FLpvBaseWriteShaderParams& BaseParams,
		FTextureRHIParamRef RsmDiffuseTextureRHI,
		FTextureRHIParamRef RsmNormalTextureRHI,
		FTextureRHIParamRef RsmDepthTextureRHI )
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FLpvWriteShaderCSBase::SetParameters(RHICmdList, BaseParams );

		FSamplerStateRHIParamRef SamplerStateLinear  = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
		FSamplerStateRHIParamRef SamplerStatePoint   = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

		// FIXME: Why do we have to bind a samplerstate to a sampler here? Presumably this is for legacy reasons... 
		SetTextureParameter(RHICmdList, ShaderRHI, RsmDiffuseTexture, LinearTextureSampler, SamplerStateLinear, RsmDiffuseTextureRHI );
		SetTextureParameter(RHICmdList, ShaderRHI, RsmNormalTexture, LinearTextureSampler, SamplerStateLinear, RsmNormalTextureRHI );
		SetTextureParameter(RHICmdList, ShaderRHI, RsmDepthTexture, PointTextureSampler, SamplerStatePoint, RsmDepthTextureRHI );
	}

	// Unbinds any buffers that have been bound.
	void UnbindBuffers(FRHICommandList& RHICmdList, FLpvBaseWriteShaderParams& BaseParams)
	{
		FLpvWriteShaderCSBase::UnbindBuffers(RHICmdList, BaseParams);
	}


	virtual bool Serialize( FArchive& Ar ) override			
	{ 
		bool rv = FLpvWriteShaderCSBase::Serialize( Ar ); 
		Ar << RsmDiffuseTexture;
		Ar << RsmNormalTexture;
		Ar << RsmDepthTexture;
		Ar << LinearTextureSampler;
		Ar << PointTextureSampler;
		return rv;
	}
protected:
	FShaderResourceParameter RsmDiffuseTexture;
	FShaderResourceParameter RsmNormalTexture;
	FShaderResourceParameter RsmDepthTexture;

	FShaderResourceParameter LinearTextureSampler;
	FShaderResourceParameter PointTextureSampler;
};
IMPLEMENT_SHADER_TYPE(,FLpvInject_GenerateVplListsCS,TEXT("/Engine/Private/LPVInject_GenerateVplLists.usf"),TEXT("CSGenerateVplLists_LightDirectional"),SF_Compute);

// ----------------------------------------------------------------------------
// LPV accumulate VPL lists compute shader
// ----------------------------------------------------------------------------
class FLpvInject_AccumulateVplListsCS : public FLpvWriteShaderCSBase
{
	DECLARE_SHADER_TYPE(FLpvInject_AccumulateVplListsCS,Global);

public:
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && IsLPVSupported(Platform); }

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FLpvWriteShaderCSBase::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	FLpvInject_AccumulateVplListsCS()	{	}

	explicit FLpvInject_AccumulateVplListsCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvWriteShaderCSBase(Initializer)		{	}

	virtual bool Serialize( FArchive& Ar ) override			{ return FLpvWriteShaderCSBase::Serialize( Ar ); }
};
IMPLEMENT_SHADER_TYPE(,FLpvInject_AccumulateVplListsCS,TEXT("/Engine/Private/LPVInject_AccumulateVplLists.usf"),TEXT("CSAccumulateVplLists"),SF_Compute);

// ----------------------------------------------------------------------------
// LPV directional occlusion compute shader
// ----------------------------------------------------------------------------
class FLpvDirectionalOcclusionCS : public FLpvWriteShaderCSBase
{
	DECLARE_SHADER_TYPE(FLpvDirectionalOcclusionCS,Global);

public:
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && IsLPVSupported(Platform); }

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FLpvWriteShaderCSBase::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	FLpvDirectionalOcclusionCS()	{	}

	explicit FLpvDirectionalOcclusionCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvWriteShaderCSBase(Initializer)		
	{	
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{

		bool bShaderHasOutdatedParameters = FLpvWriteShaderCSBase::Serialize( Ar );
		return bShaderHasOutdatedParameters;
	}

	virtual void SetParameters(
		FRHICommandList& RHICmdList, 
		FLpvBaseWriteShaderParams& BaseParams )
	{
		FLpvWriteShaderCSBase::SetParameters( RHICmdList, BaseParams );
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
	}
};
IMPLEMENT_SHADER_TYPE(,FLpvDirectionalOcclusionCS,TEXT("/Engine/Private/LPVDirectionalOcclusion.usf"),TEXT("CSDirectionalOcclusion"),SF_Compute);

// ----------------------------------------------------------------------------
// LPV directional occlusion compute shader
// ----------------------------------------------------------------------------
class FLpvCopyAOVolumeCS : public FLpvWriteShaderCSBase
{
	DECLARE_SHADER_TYPE(FLpvCopyAOVolumeCS,Global);

public:
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && IsLPVSupported(Platform); }

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FLpvWriteShaderCSBase::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	FLpvCopyAOVolumeCS()	{	}

	explicit FLpvCopyAOVolumeCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvWriteShaderCSBase(Initializer)		
	{	
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{

		bool bShaderHasOutdatedParameters = FLpvWriteShaderCSBase::Serialize( Ar );
		return bShaderHasOutdatedParameters;
	}

	virtual void SetParameters(
		FRHICommandList& RHICmdList, 
		FLpvBaseWriteShaderParams& BaseParams )
	{	
		FLpvWriteShaderCSBase::SetParameters( RHICmdList, BaseParams );
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
	}
};
IMPLEMENT_SHADER_TYPE(,FLpvCopyAOVolumeCS,TEXT("/Engine/Private/LPVDirectionalOcclusion.usf"),TEXT("CSCopyAOVolume"),SF_Compute);


// ----------------------------------------------------------------------------
// Compute shader to build a geometry volume
// ----------------------------------------------------------------------------
class FLpvBuildGeometryVolumeCS : public FLpvWriteShaderCSBase
{
	DECLARE_SHADER_TYPE(FLpvBuildGeometryVolumeCS,Global);

public:
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && IsLPVSupported(Platform); }

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FLpvWriteShaderCSBase::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	FLpvBuildGeometryVolumeCS()	{	}

	explicit FLpvBuildGeometryVolumeCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvWriteShaderCSBase(Initializer)		{	}

	virtual bool Serialize( FArchive& Ar ) override			{ return FLpvWriteShaderCSBase::Serialize( Ar ); }
};
IMPLEMENT_SHADER_TYPE(,FLpvBuildGeometryVolumeCS,TEXT("/Engine/Private/LPVBuildGeometryVolume.usf"),TEXT("CSBuildGeometryVolume"),SF_Compute);

// ----------------------------------------------------------------------------
// LPV propagate compute shader
// ----------------------------------------------------------------------------
enum PropagateShaderFlags
{
	PROPAGATE_SECONDARY_OCCLUSION	= 0x01,
	PROPAGATE_MULTIPLE_BOUNCES		= 0x02,
	PROPAGATE_AO					= 0x04,
};

template<uint32 ShaderFlags>
class TLpvPropagateCS : public FLpvWriteShaderCSBase
{
	DECLARE_SHADER_TYPE(TLpvPropagateCS,Global);

public:
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && IsLPVSupported(Platform); }

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		CA_SUPPRESS(6313);
		OutEnvironment.SetDefine(TEXT("LPV_SECONDARY_OCCLUSION"), (uint32)(ShaderFlags & PROPAGATE_SECONDARY_OCCLUSION ? 1 : 0));
		CA_SUPPRESS(6313);
		OutEnvironment.SetDefine(TEXT("LPV_MULTIPLE_BOUNCES_ENABLED"), (uint32)(ShaderFlags & PROPAGATE_MULTIPLE_BOUNCES ? 1 : 0));
		CA_SUPPRESS(6313);
		OutEnvironment.SetDefine(TEXT("LPV_PROPAGATE_AO"), (uint32)(ShaderFlags & PROPAGATE_AO ? 1 : 0));
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);

		FLpvWriteShaderCSBase::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	TLpvPropagateCS()	{	}

	explicit TLpvPropagateCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvWriteShaderCSBase(Initializer)		{	}

	virtual bool Serialize( FArchive& Ar ) override			{ return FLpvWriteShaderCSBase::Serialize( Ar ); }
};

IMPLEMENT_SHADER_TYPE(template<>,TLpvPropagateCS<0>,																		TEXT("/Engine/Private/LPVPropagate.usf"),TEXT("CSPropagate"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TLpvPropagateCS<PROPAGATE_SECONDARY_OCCLUSION>,											TEXT("/Engine/Private/LPVPropagate.usf"),TEXT("CSPropagate"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TLpvPropagateCS<PROPAGATE_MULTIPLE_BOUNCES>,												TEXT("/Engine/Private/LPVPropagate.usf"),TEXT("CSPropagate"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TLpvPropagateCS<PROPAGATE_SECONDARY_OCCLUSION|PROPAGATE_MULTIPLE_BOUNCES>,					TEXT("/Engine/Private/LPVPropagate.usf"),TEXT("CSPropagate"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TLpvPropagateCS<PROPAGATE_AO>,																TEXT("/Engine/Private/LPVPropagate.usf"),TEXT("CSPropagate"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TLpvPropagateCS<PROPAGATE_AO|PROPAGATE_SECONDARY_OCCLUSION>,								TEXT("/Engine/Private/LPVPropagate.usf"),TEXT("CSPropagate"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TLpvPropagateCS<PROPAGATE_AO|PROPAGATE_MULTIPLE_BOUNCES>,									TEXT("/Engine/Private/LPVPropagate.usf"),TEXT("CSPropagate"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TLpvPropagateCS<PROPAGATE_AO|PROPAGATE_SECONDARY_OCCLUSION|PROPAGATE_MULTIPLE_BOUNCES>,	TEXT("/Engine/Private/LPVPropagate.usf"),TEXT("CSPropagate"),SF_Compute);

FLpvWriteShaderCSBase* GetPropagateShader( FViewInfo& View, uint32 ShaderFlags )
{
	switch( ShaderFlags )
	{
	case 0:
		return (FLpvWriteShaderCSBase*)*TShaderMapRef<TLpvPropagateCS<0> >( View.ShaderMap );
	case PROPAGATE_SECONDARY_OCCLUSION:										
		return (FLpvWriteShaderCSBase*)*TShaderMapRef<TLpvPropagateCS<PROPAGATE_SECONDARY_OCCLUSION> >( View.ShaderMap );
	case PROPAGATE_MULTIPLE_BOUNCES:	
		return (FLpvWriteShaderCSBase*)*TShaderMapRef<TLpvPropagateCS<PROPAGATE_MULTIPLE_BOUNCES> >( View.ShaderMap );
	case PROPAGATE_SECONDARY_OCCLUSION|PROPAGATE_MULTIPLE_BOUNCES:	
		return (FLpvWriteShaderCSBase*)*TShaderMapRef<TLpvPropagateCS<PROPAGATE_SECONDARY_OCCLUSION|PROPAGATE_MULTIPLE_BOUNCES> >( View.ShaderMap );
	case PROPAGATE_AO:							
		return (FLpvWriteShaderCSBase*)*TShaderMapRef<TLpvPropagateCS<PROPAGATE_AO> >( View.ShaderMap );
	case PROPAGATE_AO|PROPAGATE_SECONDARY_OCCLUSION:							
		return (FLpvWriteShaderCSBase*)*TShaderMapRef<TLpvPropagateCS<PROPAGATE_AO|PROPAGATE_SECONDARY_OCCLUSION> >( View.ShaderMap );
	case PROPAGATE_AO|PROPAGATE_MULTIPLE_BOUNCES:
		return (FLpvWriteShaderCSBase*)*TShaderMapRef<TLpvPropagateCS<PROPAGATE_AO|PROPAGATE_MULTIPLE_BOUNCES> >( View.ShaderMap );
	case PROPAGATE_AO|PROPAGATE_SECONDARY_OCCLUSION|PROPAGATE_MULTIPLE_BOUNCES:
		return (FLpvWriteShaderCSBase*)*TShaderMapRef<TLpvPropagateCS<PROPAGATE_AO|PROPAGATE_SECONDARY_OCCLUSION|PROPAGATE_MULTIPLE_BOUNCES> >( View.ShaderMap );
	}
	return NULL;
}

// ----------------------------------------------------------------------------
// Base injection compute shader
// ----------------------------------------------------------------------------
enum EInjectFlags
{
	INJECT_SHADOW_CASTING	 = 0x01,
	INJECT_SPOT_ATTENUATION  = 0x02,
};

class FLpvInjectShader_Base : public FLpvWriteShaderCSBase
{
public:
	void SetParameters(
		FRHICommandList& RHICmdList, 
		FLpvBaseWriteShaderParams& BaseParams,
		FDirectLightInjectBufferRef& InjectUniformBuffer )
	{
		FLpvWriteShaderCSBase::SetParameters(RHICmdList, BaseParams );
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FLpvDirectLightInjectParameters>(), InjectUniformBuffer );
	}

	FLpvInjectShader_Base()	{	}

	explicit FLpvInjectShader_Base( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvWriteShaderCSBase(Initializer)		{	}

	virtual bool Serialize( FArchive& Ar ) override			{ return FLpvWriteShaderCSBase::Serialize( Ar ); }
};

// ----------------------------------------------------------------------------
// Point light injection compute shader
// ----------------------------------------------------------------------------

template <uint32 InjectFlags>
class TLpvInject_LightCS : public FLpvInjectShader_Base
{
	DECLARE_SHADER_TYPE(TLpvInject_LightCS,Global);

public:
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && IsLPVSupported(Platform); }

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		CA_SUPPRESS(6313);
		OutEnvironment.SetDefine(TEXT("SHADOW_CASTING"),   (uint32)(InjectFlags & INJECT_SHADOW_CASTING ? 1 : 0));
		CA_SUPPRESS(6313);
		OutEnvironment.SetDefine(TEXT("SPOT_ATTENUATION"), (uint32)(InjectFlags & INJECT_SPOT_ATTENUATION ? 1 : 0));
		FLpvWriteShaderCSBase::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	TLpvInject_LightCS()	{	}

	explicit TLpvInject_LightCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvInjectShader_Base(Initializer)		{	}

	virtual bool Serialize( FArchive& Ar ) override			{ return FLpvInjectShader_Base::Serialize( Ar ); }
};

IMPLEMENT_SHADER_TYPE(template<>,TLpvInject_LightCS<0>,TEXT("/Engine/Private/LPVDirectLightInject.usf"),TEXT("CSLightInject_ListGenCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TLpvInject_LightCS<1>,TEXT("/Engine/Private/LPVDirectLightInject.usf"),TEXT("CSLightInject_ListGenCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TLpvInject_LightCS<2>,TEXT("/Engine/Private/LPVDirectLightInject.usf"),TEXT("CSLightInject_ListGenCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TLpvInject_LightCS<3>,TEXT("/Engine/Private/LPVDirectLightInject.usf"),TEXT("CSLightInject_ListGenCS"),SF_Compute);

// ----------------------------------------------------------------------------
// FLightPropagationVolume
// ----------------------------------------------------------------------------
FLightPropagationVolume::FLightPropagationVolume() :
	  mGridOffset( 0, 0, 0 )
	, mOldGridOffset( 0, 0, 0 )
	, mInjectedLightCount( 0 )
	, SecondaryOcclusionStrength( 0.0f )
	, SecondaryBounceStrength( 0.0f )
	, CubeSize( 5312.0f )
	, bEnabled( false )
	, bDirectionalOcclusionEnabled( false )
	, bGeometryVolumeNeeded( false )
	, mWriteBufferIndex( 0 )
	, bNeedsBufferClear( true )
	, GeometryVolumeGenerated( false )
	, AsyncJobFenceID(-1)
{
	// VPL List buffers
	mVplListBuffer = new FRWBufferStructured();
	int32 RSMResolution = FSceneRenderTargets::Get_FrameConstantsOnly().GetReflectiveShadowMapResolution();
	int32 GvListBufferSize = RSMResolution * RSMResolution * 16; // Allow 16 layers of depth per every pixel of the RSM (on average) 
	int32 VplListBufferSize = RSMResolution * RSMResolution * 4; // Allow 4 layers of depth per pixel in the RSM (1 for the RSM injection + 3 for light injection)
	mVplListBuffer->Initialize( sizeof( VplListEntry ), VplListBufferSize, 0, TEXT("mVplListBuffer"), true, false );
	mVplListHeadBuffer = new FRWBufferByteAddress();
	mVplListHeadBuffer->Initialize( LPV_GRIDRES*LPV_GRIDRES*LPV_GRIDRES*4, BUF_ByteAddressBuffer );

	// Geometry volume buffers
	GvListBuffer = new FRWBufferStructured();
	GvListBuffer->Initialize( sizeof( VplListEntry ), GvListBufferSize, 0, TEXT("GvListBuffer"), true, false );
	GvListHeadBuffer = new FRWBufferByteAddress();
	GvListHeadBuffer->Initialize( LPV_GRIDRES*LPV_GRIDRES*LPV_GRIDRES*4, BUF_ByteAddressBuffer );

	LpvWriteUniformBufferParams = new FLpvWriteUniformBufferParameters;
	FMemory::Memzero( LpvWriteUniformBufferParams, sizeof(FLpvWriteUniformBufferParameters) );
	bInitialized = false;
}


/**
* Destructor
*/
FLightPropagationVolume::~FLightPropagationVolume()
{
	LpvWriteUniformBuffer.ReleaseResource();
	RsmRenderUniformBuffer.ReleaseResource();

	// Note: this is double-buffered!
	for ( int i = 0; i < 2; i++ )
	{
		for ( int j = 0; j < 7; j++ )
		{
			LpvVolumeTextures[i][j].SafeRelease();
		}
	}

	mVplListHeadBuffer->Release();
	delete mVplListHeadBuffer;

	mVplListBuffer->Release();
	delete mVplListBuffer;

	for ( int i = 0; i < NUM_GV_TEXTURES; i++ )
	{
		GvVolumeTextures[i].SafeRelease();
	}

	GvListHeadBuffer->Release();
	delete GvListHeadBuffer;

	GvListBuffer->Release();
	delete GvListBuffer;

	delete LpvWriteUniformBufferParams;
}  

/**
* Sets up the LPV at the beginning of the frame
*/
void FLightPropagationVolume::InitSettings(FRHICommandListImmediate& RHICmdList, const FSceneView& View)
{
	int32 NumFastLpvTextures = 7;
	int32 NumFastGvTextures = 2;
	FIntPoint BufferSize = FSceneRenderTargets::Get_FrameConstantsOnly().GetBufferSizeXY();
	if ( BufferSize.X >= 1600 && BufferSize.Y >= 900 )
	{
		NumFastLpvTextures = 5;
		NumFastGvTextures = 1;
	}
	check(View.GetFeatureLevel() >= ERHIFeatureLevel::SM5);
	if ( !bInitialized )
	{
		// @TODO: this should probably be derived from FRenderResource (with InitDynamicRHI etc)
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::CreateVolumeDesc(
			LPV_GRIDRES,
			LPV_GRIDRES,
			LPV_GRIDRES,
			PF_FloatRGBA,
			FClearValueBinding::None,
			TexCreate_HideInVisualizeTexture | GFastVRamConfig.LPV,
			TexCreate_ShaderResource | TexCreate_UAV,
			false,
			1));

		{
			const TCHAR* Names[] = { TEXT("LPV_A0"), TEXT("LPV_B0"), TEXT("LPV_A1"), TEXT("LPV_B1"), TEXT("LPV_A2"), TEXT("LPV_B2"), TEXT("LPV_A3"), TEXT("LPV_B3"), TEXT("LPV_A4"), TEXT("LPV_B4"), TEXT("LPV_A5"), TEXT("LPV_B5"), TEXT("LPV_A6"), TEXT("LPV_B6") };

			// Note: this is double-buffered!
			for ( int i = 0; i < 2; i++ )
			{
				for ( int j = 0; j < 7; j++ )
				{
					GRenderTargetPool.FindFreeElement(RHICmdList, Desc, LpvVolumeTextures[i][j], Names[j * 2 + i] );
				}
			}
		}

		{
			const TCHAR* Names[] = { TEXT("LPV_GV0"), TEXT("LPV_GV1"), TEXT("LPV_GV2") };

			for ( int i = 0; i < NUM_GV_TEXTURES; i++ )
			{
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, GvVolumeTextures[i], Names[i]);
			}
		}

		{
			FPooledRenderTargetDesc AODesc(FPooledRenderTargetDesc::CreateVolumeDesc(
				LPV_GRIDRES,
				LPV_GRIDRES,
				LPV_GRIDRES,
				PF_G8,
				FClearValueBinding::None,
				TexCreate_HideInVisualizeTexture | GFastVRamConfig.LPV,
				TexCreate_ShaderResource | TexCreate_UAV,
				false,
				1));
			GRenderTargetPool.FindFreeElement(RHICmdList, AODesc, AOVolumeTexture, TEXT("LPVAOVolume"));
		}

		bInitialized = true;
	}  

	const FLightPropagationVolumeSettings& LPVSettings = View.FinalPostProcessSettings.BlendableManager.GetSingleFinalDataConst<FLightPropagationVolumeSettings>();

	// Copy the LPV postprocess settings
	Strength	 = LPVSettings.LPVIntensity;
	bEnabled     = Strength > 0.0f;
	CubeSize	 = LPVSettings.LPVSize;
	bDirectionalOcclusionEnabled = bEnabled && ( LPVSettings.LPVDirectionalOcclusionIntensity > 0.0001f );

	SecondaryOcclusionStrength = LPVSettings.LPVSecondaryOcclusionIntensity;
	SecondaryBounceStrength = LPVSettings.LPVSecondaryBounceIntensity;

	bGeometryVolumeNeeded = LPVSettings.LPVSecondaryOcclusionIntensity > 0.001f || bDirectionalOcclusionEnabled;
	GeometryVolumeGenerated = false;

	if ( !bEnabled )
	{
		return;
	}

	// Clear the UAVs if necessary
	float ClearMultiplier = 1.0f;
	if ( bNeedsBufferClear )
	{
		ClearMultiplier = 0.0f;
		// Since this is double buffered, the clear flag should remain set for the first 2 frames so that all buffers get cleared.
		if ( mWriteBufferIndex > 0 )
		{
			bNeedsBufferClear = false;
		}
	}

	mInjectedLightCount = 0;
	// Update the grid offset based on the camera
	{
		mOldGridOffset = mGridOffset;
		FVector CentrePos = View.ViewMatrices.GetViewOrigin();
		FVector CameraAt = View.GetViewDirection();

		float LpvScale = CubeSize / float(LPV_GRIDRES);
		float OneOverLpvScale = 1.0f / LpvScale;

		CentrePos += CameraAt * (LPV_CENTRE_OFFSET*LpvScale);
		FVector HalfGridRes = FVector( LPV_GRIDRES/2, LPV_GRIDRES/2, LPV_GRIDRES/2 );
		FVector Offset = ( CentrePos * OneOverLpvScale - HalfGridRes ) * -1.0f;
		mGridOffset = FIntVector( Offset.X, Offset.Y, Offset.Z );

		LpvWriteUniformBufferParams->mOldGridOffset		= mOldGridOffset;
		LpvWriteUniformBufferParams->mLpvGridOffset		= mGridOffset;
		LpvWriteUniformBufferParams->mEyePos			= View.ViewMatrices.GetViewOrigin();
		LpvWriteUniformBufferParams->ClearMultiplier	= ClearMultiplier;
		LpvWriteUniformBufferParams->LpvScale			= LpvScale;
		LpvWriteUniformBufferParams->OneOverLpvScale	= OneOverLpvScale;
		LpvWriteUniformBufferParams->SecondaryOcclusionStrength = SecondaryOcclusionStrength;
		LpvWriteUniformBufferParams->SecondaryBounceStrength = SecondaryBounceStrength;

		// Convert the bias values from LPV grid space to world space
		LpvWriteUniformBufferParams->GeometryVolumeInjectionBias	= LPVSettings.LPVGeometryVolumeBias * LpvScale;
		LpvWriteUniformBufferParams->VplInjectionBias				= LPVSettings.LPVVplInjectionBias * LpvScale;
		LpvWriteUniformBufferParams->PropagationIndex				= 0;
		LpvWriteUniformBufferParams->EmissiveInjectionMultiplier	= LPVSettings.LPVEmissiveInjectionIntensity
			* LpvWriteUniformBufferParams->RsmAreaIntensityMultiplier * CVarLPVEmissiveIntensityMultiplier.GetValueOnRenderThread() * 0.25f;
		LpvWriteUniformBufferParams->DirectionalOcclusionIntensity	= LPVSettings.LPVDirectionalOcclusionIntensity;
		LpvWriteUniformBufferParams->DirectionalOcclusionRadius		= LPVSettings.LPVDirectionalOcclusionRadius;
		LpvWriteUniformBufferParams->RsmPixelToTexcoordMultiplier	= 1.0f / float(FSceneRenderTargets::Get_FrameConstantsOnly().GetReflectiveShadowMapResolution() - 1);

		LpvReadUniformBufferParams.DirectionalOcclusionIntensity	= LPVSettings.LPVDirectionalOcclusionIntensity;
		LpvReadUniformBufferParams.DiffuseOcclusionExponent			= LPVSettings.LPVDiffuseOcclusionExponent;
		LpvReadUniformBufferParams.SpecularOcclusionExponent		= LPVSettings.LPVSpecularOcclusionExponent;
		LpvReadUniformBufferParams.DiffuseOcclusionIntensity		= LPVSettings.LPVDiffuseOcclusionIntensity;
		LpvReadUniformBufferParams.SpecularOcclusionIntensity		= LPVSettings.LPVSpecularOcclusionIntensity;

		LpvReadUniformBufferParams.DirectionalOcclusionDefaultValue = FVector( CVarLPVDirectionalOcclusionDefaultDiffuse.GetValueOnRenderThread(), CVarLPVDirectionalOcclusionDefaultSpecular.GetValueOnRenderThread(), 0.0f );
		LpvReadUniformBufferParams.DirectionalOcclusionFadeRange 	= LPVSettings.LPVDirectionalOcclusionFadeRange;
		LpvReadUniformBufferParams.FadeRange = LPVSettings.LPVFadeRange;

		LpvReadUniformBufferParams.mLpvGridOffset		= mGridOffset;
		LpvReadUniformBufferParams.LpvScale				= LpvScale;
		LpvReadUniformBufferParams.OneOverLpvScale		= OneOverLpvScale;
		LpvReadUniformBufferParams.SpecularIntensity	= CVarLPVSpecularIntensity.GetValueOnRenderThread();
		LpvReadUniformBufferParams.DiffuseIntensity		= CVarLPVDiffuseIntensity.GetValueOnRenderThread();

		LpvReadUniformBufferParams.LpvGridOffsetSmooth  = Offset;

		// Compute the bounding box
		FVector Centre = ( FVector(mGridOffset.X, mGridOffset.Y, mGridOffset.Z ) + FVector(0.5f,0.5f,0.5f) - HalfGridRes ) * -LpvScale;
		FVector Extent = FVector(CubeSize,CubeSize,CubeSize) * 0.5f;
		BoundingBox = FBox( Centre-Extent, Centre+Extent );
	}
}

/**
* Clears the LPV
*/
void FLightPropagationVolume::Clear(FRHICommandListImmediate& RHICmdList, FViewInfo& View)
{
	if ( !bEnabled )
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, LpvClear);

	if ( !LpvWriteUniformBuffer.IsInitialized() )
	{
		LpvWriteUniformBuffer.InitResource();
	}
	LpvWriteUniformBuffer.SetContents( *LpvWriteUniformBufferParams );

	// TODO: these could be run in parallel... 
	RHICmdList.AutomaticCacheFlushAfterComputeShader(false);

	// Clear the list buffers
	{
		TShaderMapRef<FLpvClearListsCS> Shader(View.ShaderMap);
		RHICmdList.SetComputeShader(Shader->GetComputeShader());

		FLpvBaseWriteShaderParams ShaderParams;
		GetShaderParams( ShaderParams );
		Shader->SetParameters(RHICmdList, ShaderParams );
		DispatchComputeShader(RHICmdList, *Shader, LPV_GRIDRES/4, LPV_GRIDRES/4, LPV_GRIDRES/4 );
		Shader->UnbindBuffers(RHICmdList, ShaderParams );
	}

	// Clear the LPV (or fade, if REFINE_OVER_TIME is enabled)
	{
		TShaderMapRef<FLpvClearCS> Shader(View.ShaderMap);
		RHICmdList.SetComputeShader(Shader->GetComputeShader());

		FLpvBaseWriteShaderParams ShaderParams;
		GetShaderParams( ShaderParams );
		Shader->SetParameters(RHICmdList, ShaderParams );
		DispatchComputeShader(RHICmdList, *Shader, LPV_GRIDRES/4, LPV_GRIDRES/4, LPV_GRIDRES/4 );
		Shader->UnbindBuffers(RHICmdList, ShaderParams);
	}

	// Clear the geometry volume if necessary
	if ( bGeometryVolumeNeeded )
	{
		TShaderMapRef<FLpvClearGeometryVolumeCS> Shader(View.ShaderMap);
		RHICmdList.SetComputeShader(Shader->GetComputeShader());

		FLpvBaseWriteShaderParams ShaderParams;
		GetShaderParams( ShaderParams );
		Shader->SetParameters(RHICmdList, ShaderParams );
		DispatchComputeShader(RHICmdList, *Shader, LPV_GRIDRES/4, LPV_GRIDRES/4, LPV_GRIDRES/4 );
		Shader->UnbindBuffers(RHICmdList, ShaderParams);
	}
	RHICmdList.AutomaticCacheFlushAfterComputeShader(true);
	RHICmdList.FlushComputeShaderCache();

	RHICmdList.SetUAVParameter( FComputeShaderRHIRef(), 7, mVplListBuffer->UAV, 0 );
	RHICmdList.SetUAVParameter( FComputeShaderRHIRef(), 7, GvListBuffer->UAV, 0 );
	RHICmdList.SetUAVParameter( FComputeShaderRHIRef(), 7, FUnorderedAccessViewRHIParamRef(), 0 );
}

/**
* Gets shadow info
*/
void FLightPropagationVolume::GetShadowInfo( const FProjectedShadowInfo& ProjectedShadowInfo, FRsmInfo& RsmInfoOut )
{
	FIntPoint ShadowBufferResolution( ProjectedShadowInfo.ResolutionX, ProjectedShadowInfo.ResolutionY);
	RsmInfoOut.WorldToShadow = ProjectedShadowInfo.GetWorldToShadowMatrix(RsmInfoOut.ShadowmapMinMax, &ShadowBufferResolution);
	RsmInfoOut.ShadowToWorld = RsmInfoOut.WorldToShadow.InverseFast();

	// Determine the shadow area in world space, so we can scale the brightness if needed. 
	FVector ShadowUp    = FVector( 1.0f,0.0f,0.0f );
	FVector ShadowRight = FVector( 0.0f,1.0f,0.0f );
	FVector4 WorldUp = RsmInfoOut.ShadowToWorld.TransformVector( ShadowUp );
	FVector4 WorldRight = RsmInfoOut.ShadowToWorld.TransformVector( ShadowRight );
	float ShadowArea = WorldUp.Size3() * WorldRight.Size3();

	int32 RSMResolution = FSceneRenderTargets::Get_FrameConstantsOnly().GetReflectiveShadowMapResolution();
	float ResolutionMultiplier = RSMResolution / 256.0f;
	float IdealCubeSizeMultiplier = 0.5f * ResolutionMultiplier; 
	float IdealRsmArea = ( CubeSize * IdealCubeSizeMultiplier * CubeSize * IdealCubeSizeMultiplier );
	RsmInfoOut.AreaBrightnessMultiplier = ShadowArea/IdealRsmArea;
}


/**
* Injects a Directional light into the LPV
*/
void FLightPropagationVolume::SetVplInjectionConstants(
	const FProjectedShadowInfo&	ProjectedShadowInfo,
	const FLightSceneProxy*		LightProxy )
{
	FLinearColor LightColor = LightProxy->GetColor();
	FRsmInfo RsmInfo;
	GetShadowInfo( ProjectedShadowInfo, RsmInfo );

	float LpvStrength = 0.0f;
	if ( bEnabled )
	{
		LpvStrength = Strength;
	}
	LpvStrength *= RsmInfo.AreaBrightnessMultiplier;
	LpvStrength *= CVarLPVIntensity.GetValueOnRenderThread();
	LpvWriteUniformBufferParams->RsmAreaIntensityMultiplier = RsmInfo.AreaBrightnessMultiplier;

	LpvStrength *= LightProxy->GetIndirectLightingScale();
	LpvWriteUniformBufferParams->mRsmToWorld = RsmInfo.ShadowToWorld;
	LpvWriteUniformBufferParams->mLightColour = FVector4( LightColor.R, LightColor.G, LightColor.B, LightColor.A ) * LpvStrength; 
	LpvWriteUniformBuffer.SetContents( *LpvWriteUniformBufferParams );
}

/**
* Injects a Directional light into the LPV
*/
void FLightPropagationVolume::InjectDirectionalLightRSM(
	FRHICommandListImmediate& RHICmdList,
	FViewInfo&					View,
	const FTexture2DRHIRef&		RsmNormalTex, 
	const FTexture2DRHIRef&		RsmDiffuseTex, 
	const FTexture2DRHIRef&		RsmDepthTex, 
	const FProjectedShadowInfo&	ProjectedShadowInfo,
	const FLinearColor&			LightColour )
{
	const FLightSceneProxy* LightProxy = ProjectedShadowInfo.GetLightSceneInfo().Proxy;
	{
		SCOPED_DRAW_EVENT(RHICmdList, LpvInjectDirectionalLightRSM);

		SetVplInjectionConstants(ProjectedShadowInfo, LightProxy ); //-V595

		TShaderMapRef<FLpvInject_GenerateVplListsCS> Shader(View.ShaderMap);
		RHICmdList.SetComputeShader(Shader->GetComputeShader());

		// Clear the list counter the first time this function is called in a frame
		FLpvBaseWriteShaderParams ShaderParams;
		GetShaderParams( ShaderParams );
		Shader->SetParameters(RHICmdList, ShaderParams, RsmDiffuseTex, RsmNormalTex, RsmDepthTex );

		int32 RSMResolution = FSceneRenderTargets::Get_FrameConstantsOnly().GetReflectiveShadowMapResolution();
		// todo: what if not divisible by 8?
		DispatchComputeShader(RHICmdList, *Shader, RSMResolution / 8, RSMResolution / 8, 1 ); 

		Shader->UnbindBuffers(RHICmdList, ShaderParams);
	}

	// If this is the first directional light, build the geometry volume with it
	if ( !GeometryVolumeGenerated && bGeometryVolumeNeeded )
	{
		SCOPED_DRAW_EVENT(RHICmdList, LpvBuildGeometryVolume);
		GeometryVolumeGenerated = true;
		FVector LightDirection( 0.0f, 0.0f, 1.0f );
		if ( LightProxy )
		{
			LightDirection = LightProxy->GetDirection();
		}
		LpvWriteUniformBufferParams->GeometryVolumeCaptureLightDirection = LightDirection;

		TShaderMapRef<FLpvBuildGeometryVolumeCS> Shader(View.ShaderMap);
		RHICmdList.SetComputeShader(Shader->GetComputeShader());

		LpvWriteUniformBuffer.SetContents( *LpvWriteUniformBufferParams ); // FIXME: is this causing a stall? Double-buffer?

		FLpvBaseWriteShaderParams ShaderParams;
		GetShaderParams( ShaderParams );
		Shader->SetParameters(RHICmdList, ShaderParams );

		DispatchComputeShader(RHICmdList, *Shader, LPV_GRIDRES/4, LPV_GRIDRES/4, LPV_GRIDRES/4 );

		Shader->UnbindBuffers(RHICmdList, ShaderParams);
	}

	mInjectedLightCount++;
}

/**
* Injects sky lighting into the LPV
*/
void FLightPropagationVolume::ComputeDirectionalOcclusion( FRHICommandListImmediate& RHICmdList, FViewInfo& View )
{
	//if ( View.FinalPostProcessSettings.ContributingCubemaps.Num() > 0 || GScene-> )
	{
		// Compute directional occlusion
		{
			SCOPED_DRAW_EVENT(RHICmdList, LpvDirectionalOcclusion);

			mWriteBufferIndex = 1-mWriteBufferIndex; // Swap buffers with each iteration
			TShaderMapRef<FLpvDirectionalOcclusionCS> Shader(View.ShaderMap);
			RHICmdList.SetComputeShader(Shader->GetComputeShader());
			FLpvBaseWriteShaderParams ShaderParams;
			GetShaderParams( ShaderParams );
			Shader->SetParameters( RHICmdList, ShaderParams );
			LpvWriteUniformBuffer.SetContents( *LpvWriteUniformBufferParams );

			DispatchComputeShader( RHICmdList, *Shader, LPV_GRIDRES/4, LPV_GRIDRES/4, LPV_GRIDRES/4 );
			Shader->UnbindBuffers(RHICmdList, ShaderParams);
		}
	}
	RHICmdList.FlushComputeShaderCache();
}

/**
* Propagates light in the LPV 
*/
void FLightPropagationVolume::Update( FRHICommandListImmediate& RHICmdList, FViewInfo& View )
{
	if ( !bEnabled )
	{
		return;
	}

	LpvWriteUniformBuffer.SetContents( *LpvWriteUniformBufferParams );

	check(View.GetFeatureLevel() == ERHIFeatureLevel::SM5);

	bool bSecondaryOcclusion	= (SecondaryOcclusionStrength > 0.001f); 
	bool bSecondaryBounces		= (SecondaryBounceStrength > 0.001f); 
	bool bDirectionalOcclusion	= (LpvWriteUniformBufferParams->DirectionalOcclusionIntensity > 0.001f);

	if ( mInjectedLightCount )
	{
		SCOPED_DRAW_EVENT(RHICmdList, LpvAccumulateVplLists);
		mWriteBufferIndex = 1-mWriteBufferIndex; // Swap buffers with each iteration

		TShaderMapRef<FLpvInject_AccumulateVplListsCS> Shader(View.ShaderMap);
		RHICmdList.SetComputeShader(Shader->GetComputeShader());

		//LpvWriteUniformBuffer.SetContents( *LpvWriteUniformBufferParams );

		FLpvBaseWriteShaderParams ShaderParams;
		GetShaderParams( ShaderParams );
		Shader->SetParameters(RHICmdList, ShaderParams );

		DispatchComputeShader(RHICmdList, *Shader, LPV_GRIDRES/4, LPV_GRIDRES/4, LPV_GRIDRES/4 );
		RHICmdList.FlushComputeShaderCache();

		Shader->UnbindBuffers(RHICmdList, ShaderParams);
	}

	// Propagate lighting, ping-ponging between the two buffers
	if ( bDirectionalOcclusion ) 
	{
		ComputeDirectionalOcclusion( RHICmdList, View );
	}

	// Propagate lighting, ping-ponging between the two buffers
	{
		SCOPED_DRAW_EVENT(RHICmdList, LpvPropagate);

		int32 LPVNumPropagationSteps = CVarNumPropagationSteps.GetValueOnRenderThread();

		for ( int i = 0; i < LPVNumPropagationSteps; i++ )
		{
			mWriteBufferIndex = 1-mWriteBufferIndex; // Swap buffers with each iteration

			// Compute shader flags
			uint32 ShaderFlags = 0;
			if ( bSecondaryOcclusion ) 
				ShaderFlags |= PROPAGATE_SECONDARY_OCCLUSION;
			if ( bSecondaryBounces ) 
				ShaderFlags |= PROPAGATE_MULTIPLE_BOUNCES;
			if ( i < CVarLPVNumAOPropagationSteps.GetValueOnRenderThread() )
				ShaderFlags |= PROPAGATE_AO;

			FLpvWriteShaderCSBase* Shader = GetPropagateShader( View, ShaderFlags );
			RHICmdList.SetComputeShader(Shader->GetComputeShader());

			LpvWriteUniformBufferParams->PropagationIndex = i;

			FLpvBaseWriteShaderParams ShaderParams;
			GetShaderParams( ShaderParams );
			Shader->SetParameters(RHICmdList, ShaderParams );

			DispatchComputeShader(RHICmdList, Shader, LPV_GRIDRES/4, LPV_GRIDRES/4, LPV_GRIDRES/4 );

			// Insert a flush for all iterations except the last - these dispatches can't overlap!
			if ( i < LPVNumPropagationSteps - 1 )
			{
				RHICmdList.FlushComputeShaderCache();
			}

			Shader->UnbindBuffers(RHICmdList, ShaderParams);
		}
	}

	// Swap buffers
	mWriteBufferIndex = 1-mWriteBufferIndex; 

	// Copy the AO volume from the LPV to a separate volume texture
	{
		SCOPED_DRAW_EVENT(RHICmdList, LpvCopyAOVolume);

		TShaderMapRef<FLpvCopyAOVolumeCS> Shader(View.ShaderMap);
		RHICmdList.SetComputeShader(Shader->GetComputeShader());
		FLpvBaseWriteShaderParams ShaderParams;
		GetShaderParams( ShaderParams );
		Shader->SetParameters( RHICmdList, ShaderParams );
		LpvWriteUniformBuffer.SetContents( *LpvWriteUniformBufferParams );
		DispatchComputeShader( RHICmdList, *Shader, LPV_GRIDRES/4, LPV_GRIDRES/4, LPV_GRIDRES/4 );
		Shader->UnbindBuffers(RHICmdList, ShaderParams);
	}
}

void FLightPropagationVolume::SetRsmUniformBuffer()
{
	if (!RsmRenderUniformBuffer.IsInitialized())
	{
		RsmRenderUniformBuffer.InitResource();
	}

	RsmRenderUniformBuffer.SetContents(*LpvWriteUniformBufferParams);
}

void FLightPropagationVolume::InsertGPUWaitForAsyncUpdate(FRHICommandListImmediate& RHICmdList)
{
}

/**
* Helper function to generate shader params
*/
void FLightPropagationVolume::GetShaderParams( FLpvBaseWriteShaderParams& OutParams ) const
{
	for(int i = 0; i < 7; ++i)
	{
		OutParams.LpvBufferSRVs[i] = LpvVolumeTextures[ 1 - mWriteBufferIndex ][i]->GetRenderTargetItem().ShaderResourceTexture;
		OutParams.LpvBufferUAVs[i] = LpvVolumeTextures[ mWriteBufferIndex ][i]->GetRenderTargetItem().UAV;
	}

	OutParams.VplListBufferSRV = mVplListBuffer->SRV;
	OutParams.VplListBufferUAV = mVplListBuffer->UAV;
	OutParams.VplListHeadBufferSRV = mVplListHeadBuffer->SRV;
	OutParams.VplListHeadBufferUAV = mVplListHeadBuffer->UAV;

	for ( int i = 0; i < NUM_GV_TEXTURES; i++ )
	{
		OutParams.GvBufferSRVs[i] = GvVolumeTextures[i]->GetRenderTargetItem().ShaderResourceTexture;
		OutParams.GvBufferUAVs[i] = GvVolumeTextures[i]->GetRenderTargetItem().UAV;
	}
	OutParams.GvListBufferSRV = GvListBuffer->SRV;
	OutParams.GvListBufferUAV = GvListBuffer->UAV;
	OutParams.GvListHeadBufferSRV = GvListHeadBuffer->SRV;
	OutParams.GvListHeadBufferUAV = GvListHeadBuffer->UAV;
	OutParams.AOVolumeTextureUAV = AOVolumeTexture->GetRenderTargetItem().UAV;
	OutParams.AOVolumeTextureSRV = AOVolumeTexture->GetRenderTargetItem().ShaderResourceTexture;
	OutParams.UniformBuffer = LpvWriteUniformBuffer;
}

void FLightPropagationVolume::InjectLightDirect(FRHICommandListImmediate& RHICmdList, const FLightSceneProxy& Light, const FViewInfo& View)
{
	if(!bEnabled)
	{
		return;
	}

	// Only point and spot lights are supported (directional lights use the RSM instead)
	if(Light.GetLightType() != LightType_Point && Light.GetLightType() != LightType_Spot )
	{
		return;
	}

	// A geometry volume is required for direct light injection. This currently requires a directional light to be injected
	//@TODO: Add support for generating a GV when there's no directional light
	if ( GeometryVolumeGenerated )
	{
		// Inject the VPLs into the LPV
		SCOPED_DRAW_EVENT(RHICmdList, LpvDirectLightInjection);

		FLpvDirectLightInjectParameters InjectUniformBufferParams;

		FLightParameters LightParameters;

		Light.GetParameters(LightParameters);
		
		InjectUniformBufferParams.LightColor = Light.GetColor() * Light.GetIndirectLightingScale();
		InjectUniformBufferParams.LightPosition = Light.GetPosition();
		InjectUniformBufferParams.LightRadius = Light.GetRadius();
		InjectUniformBufferParams.LightFalloffExponent = LightParameters.LightColorAndFalloffExponent.W;
		InjectUniformBufferParams.LightDirection = LightParameters.NormalizedLightDirection;
		InjectUniformBufferParams.LightSpotAngles = LightParameters.SpotAngles;
		InjectUniformBufferParams.LightSourceLength = LightParameters.LightSourceLength;
		InjectUniformBufferParams.bLightInverseSquaredAttenuation = Light.IsInverseSquared() ? 1.0f : 0.0f;

		FLpvInjectShader_Base* Shader = nullptr;

		switch ( Light.GetLightType() )
		{
			case LightType_Point:
				{
					if ( Light.CastsStaticShadow() || Light.CastsDynamicShadow() )
					{
						Shader = (FLpvInjectShader_Base*)*TShaderMapRef<TLpvInject_LightCS<INJECT_SHADOW_CASTING> >(View.ShaderMap);
					}
					else
					{
						Shader = (FLpvInjectShader_Base*)*TShaderMapRef<TLpvInject_LightCS<0> >(View.ShaderMap);
					}
				}
				break;
			case LightType_Spot:
				{
					if ( Light.CastsStaticShadow() || Light.CastsDynamicShadow() )
					{
						Shader = (FLpvInjectShader_Base*)*TShaderMapRef<TLpvInject_LightCS<INJECT_SPOT_ATTENUATION | INJECT_SHADOW_CASTING> >(View.ShaderMap);
					}
					else
					{
						Shader = (FLpvInjectShader_Base*)*TShaderMapRef<TLpvInject_LightCS<INJECT_SPOT_ATTENUATION | 0> >(View.ShaderMap);
					}
				}
				break;
		}
		RHICmdList.SetComputeShader(Shader->GetComputeShader());

  	    FDirectLightInjectBufferRef InjectUniformBuffer = 
			FDirectLightInjectBufferRef::CreateUniformBufferImmediate(InjectUniformBufferParams, UniformBuffer_SingleFrame );

		mWriteBufferIndex = 1 - mWriteBufferIndex; // Swap buffers with each iteration

		FLpvBaseWriteShaderParams ShaderParams;
		GetShaderParams( ShaderParams );

		LpvWriteUniformBuffer.SetContents( *LpvWriteUniformBufferParams );

		Shader->SetParameters(RHICmdList, ShaderParams, InjectUniformBuffer );
		DispatchComputeShader(RHICmdList, Shader, LPV_GRIDRES / 4, LPV_GRIDRES / 4, LPV_GRIDRES / 4 );
		Shader->UnbindBuffers(RHICmdList, ShaderParams);
	}
}


// use for render thread only
bool UseLightPropagationVolumeRT(ERHIFeatureLevel::Type InFeatureLevel)
{
	if (InFeatureLevel < ERHIFeatureLevel::SM5)
	{
		return false;
	}

	int32 Value = CVarLightPropagationVolume.GetValueOnRenderThread();

	return Value != 0;
}


// ----------------------------------------------------------------------------
// FSceneViewState
// ----------------------------------------------------------------------------
void FSceneViewState::SetupLightPropagationVolume(FSceneView& View, FSceneViewFamily& ViewFamily)
{
	check(IsInRenderingThread());

	if(LightPropagationVolume)
	{
		// not needed
		return;
	}

	const ERHIFeatureLevel::Type ViewFeatureLevel = View.GetFeatureLevel();

	if (View.StereoPass == eSSP_RIGHT_EYE)
	{
		// The right eye will reference the left eye's LPV with the assumption that the left eye uses the primary view (index 0)
		const FSceneView* PrimaryView = ViewFamily.Views[0];
		if (PrimaryView->StereoPass == eSSP_LEFT_EYE && PrimaryView->State)
		{
			FSceneViewState* PrimaryViewState = PrimaryView->State->GetConcreteViewState();
			if (PrimaryViewState)
			{
				LightPropagationVolume = PrimaryViewState->GetLightPropagationVolume(ViewFeatureLevel);
				if (LightPropagationVolume.IsValid())
				{
					bIsStereoView = true;
				}
			}
		}
	}
	else
	{
		if (UseLightPropagationVolumeRT(ViewFeatureLevel) && IsLPVSupported(GShaderPlatformForFeatureLevel[ViewFeatureLevel]))
		{
			LightPropagationVolume = new FLightPropagationVolume();
		}
	}
}

FLightPropagationVolume* FSceneViewState::GetLightPropagationVolume(ERHIFeatureLevel::Type InFeatureLevel, bool bIncludeStereo) const
{
	if (InFeatureLevel < ERHIFeatureLevel::SM5)
	{
		// to prevent crash when starting in SM5 and the using the editor preview SM4
		return 0;
	}

	if (bIsStereoView && !bIncludeStereo)
	{
		// return 0 on stereo views when they aren't explicitly included
		return 0;
	}

	return LightPropagationVolume;
}

void FSceneViewState::DestroyLightPropagationVolume()
{
	if ( LightPropagationVolume ) 
	{
		LightPropagationVolume->AddRef();
		FLightPropagationVolume* LPV = LightPropagationVolume;
		LightPropagationVolume = nullptr;

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			DeleteLPV,
			FLightPropagationVolume*, LPV, LPV,
		{
			LPV->Release();
		}
		);
		bIsStereoView = false;
	}
}


void FDeferredShadingSceneRenderer::ClearLPVs(FRHICommandListImmediate& RHICmdList)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateLPVs);
	bool bAnyViewHasLPVs = false;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		FSceneViewState* ViewState = (FSceneViewState*)Views[ViewIndex].State;

		if (ViewState)
		{
			FLightPropagationVolume* LightPropagationVolume = ViewState->GetLightPropagationVolume(View.GetFeatureLevel());

			if (LightPropagationVolume)
			{
				bAnyViewHasLPVs = true;
				break;
			}
		}
	}

	if (bAnyViewHasLPVs)
	{
		SCOPED_DRAW_EVENT(RHICmdList, ClearLPVs);

		for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			FViewInfo& View = Views[ViewIndex];

			FSceneViewState* ViewState = (FSceneViewState*)Views[ViewIndex].State;
			if(ViewState)
			{
				FLightPropagationVolume* LightPropagationVolume = ViewState->GetLightPropagationVolume(View.GetFeatureLevel());

				if(LightPropagationVolume)
				{
					SCOPED_GPU_STAT(RHICmdList, Stat_GPU_LPV);
					LightPropagationVolume->InitSettings(RHICmdList, Views[ViewIndex]);
					LightPropagationVolume->Clear(RHICmdList, View);
				}
			}
		}
	}
}

void FDeferredShadingSceneRenderer::UpdateLPVs(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_DRAW_EVENT(RHICmdList, UpdateLPVs);
	SCOPE_CYCLE_COUNTER(STAT_UpdateLPVs);

	for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

		FViewInfo& View = Views[ViewIndex];
		FSceneViewState* ViewState = (FSceneViewState*)Views[ViewIndex].State;

		if(ViewState)
		{
			FLightPropagationVolume* LightPropagationVolume = ViewState->GetLightPropagationVolume(View.GetFeatureLevel());

			if(LightPropagationVolume)
			{
//				SCOPED_DRAW_EVENT(RHICmdList, UpdateLPVs);
//				SCOPE_CYCLE_COUNTER(STAT_UpdateLPVs);
				SCOPED_GPU_STAT(RHICmdList, Stat_GPU_LPV);

				LightPropagationVolume->Update(RHICmdList, View);
			}
		}
	}
}

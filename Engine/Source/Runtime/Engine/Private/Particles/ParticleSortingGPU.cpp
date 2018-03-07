// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	ParticleSortingGPU.cpp: Sorting GPU particles.
==============================================================================*/

#include "Particles/ParticleSortingGPU.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "SceneUtils.h"
#include "ParticleHelper.h"
#include "Particles/ParticleSimulationGPU.h"
#include "ShaderParameterUtils.h"
#include "GlobalShader.h"
#include "GPUSort.h"

/*------------------------------------------------------------------------------
	Shaders used to generate particle sort keys.
------------------------------------------------------------------------------*/

/** The number of threads per group used to generate particle keys. */
#define PARTICLE_KEY_GEN_THREAD_COUNT 64

/**
 * Uniform buffer parameters for generating particle sort keys.
 */
BEGIN_UNIFORM_BUFFER_STRUCT( FParticleKeyGenParameters, )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, ViewOrigin )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( uint32, ChunksPerGroup )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( uint32, ExtraChunkCount )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( uint32, OutputOffset )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( uint32, EmitterKey )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( uint32, KeyCount )
END_UNIFORM_BUFFER_STRUCT( FParticleKeyGenParameters )

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FParticleKeyGenParameters,TEXT("ParticleKeyGen"));

typedef TUniformBufferRef<FParticleKeyGenParameters> FParticleKeyGenUniformBufferRef;

/**
 * Compute shader used to generate particle sort keys.
 */
class FParticleSortKeyGenCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FParticleSortKeyGenCS,Global);

public:

	static bool ShouldCache( EShaderPlatform Platform )
	{
		return RHISupportsComputeShaders(Platform);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
		OutEnvironment.SetDefine( TEXT("THREAD_COUNT"), PARTICLE_KEY_GEN_THREAD_COUNT );
		OutEnvironment.SetDefine( TEXT("TEXTURE_SIZE_X"), GParticleSimulationTextureSizeX );
		OutEnvironment.SetDefine( TEXT("TEXTURE_SIZE_Y"), GParticleSimulationTextureSizeY );
	}

	/** Default constructor. */
	FParticleSortKeyGenCS()
	{
	}

	/** Initialization constructor. */
	explicit FParticleSortKeyGenCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		InParticleIndices.Bind( Initializer.ParameterMap, TEXT("InParticleIndices") );
		PositionTexture.Bind( Initializer.ParameterMap, TEXT("PositionTexture") );
		PositionTextureSampler.Bind( Initializer.ParameterMap, TEXT("PositionTextureSampler") );
		OutKeys.Bind( Initializer.ParameterMap, TEXT("OutKeys") );
		OutParticleIndices.Bind( Initializer.ParameterMap, TEXT("OutParticleIndices") );
	}

	/** Serialization. */
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << InParticleIndices;
		Ar << PositionTexture;
		Ar << PositionTextureSampler;
		Ar << OutKeys;
		Ar << OutParticleIndices;
		return bShaderHasOutdatedParameters;
	}

	/**
	 * Set output buffers for this shader.
	 */
	void SetOutput(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef OutKeysUAV, FUnorderedAccessViewRHIParamRef OutIndicesUAV )
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if ( OutKeys.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutKeys.GetBaseIndex(), OutKeysUAV);
		}
		if ( OutParticleIndices.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutParticleIndices.GetBaseIndex(), OutIndicesUAV);
		}
	}

	/**
	 * Set input parameters.
	 */
	void SetParameters(
		FRHICommandList& RHICmdList,
		FParticleKeyGenUniformBufferRef& UniformBuffer,
		FShaderResourceViewRHIParamRef InIndicesSRV
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FParticleKeyGenParameters>(), UniformBuffer );
		if ( InParticleIndices.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InParticleIndices.GetBaseIndex(), InIndicesSRV);
		}
	}

	/**
	 * Set the texture from which particle positions can be read.
	 */
	void SetPositionTextures(FRHICommandList& RHICmdList, FTexture2DRHIParamRef PositionTextureRHI)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (PositionTexture.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, PositionTexture.GetBaseIndex(), PositionTextureRHI);
		}
	}

	/**
	 * Unbinds any buffers that have been bound.
	 */
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if ( InParticleIndices.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InParticleIndices.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if ( OutKeys.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutKeys.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if ( OutParticleIndices.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutParticleIndices.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:

	/** Input buffer containing particle indices. */
	FShaderResourceParameter InParticleIndices;
	/** Texture containing particle positions. */
	FShaderResourceParameter PositionTexture;
	FShaderResourceParameter PositionTextureSampler;
	/** Output key buffer. */
	FShaderResourceParameter OutKeys;
	/** Output indices buffer. */
	FShaderResourceParameter OutParticleIndices;
};
IMPLEMENT_SHADER_TYPE(,FParticleSortKeyGenCS,TEXT("/Engine/Private/ParticleSortKeyGen.usf"),TEXT("GenerateParticleSortKeys"),SF_Compute);

/**
 * Generate sort keys for a list of particles.
 * @param KeyBufferUAV - Unordered access view of the buffer where sort keys will be stored.
 * @param SortedVertexBufferUAV - Unordered access view of the vertex buffer where particle indices will be stored.
 * @param PositionTextureRHI - The texture containing world space positions for all particles.
 * @param SimulationsToSort - A list of simulations to generate sort keys for.
 * @returns the total number of particles being sorted.
 */
static int32 GenerateParticleSortKeys(
	FRHICommandListImmediate& RHICmdList,
	FUnorderedAccessViewRHIParamRef KeyBufferUAV,
	FUnorderedAccessViewRHIParamRef SortedVertexBufferUAV,
	FTexture2DRHIParamRef PositionTextureRHI,
	const TArray<FParticleSimulationSortInfo>& SimulationsToSort,
	ERHIFeatureLevel::Type FeatureLevel
	)
{
	SCOPED_DRAW_EVENT(RHICmdList, ParticleSortKeyGen);
	check(RHISupportsComputeShaders(GShaderPlatformForFeatureLevel[FeatureLevel]));

	FParticleKeyGenParameters KeyGenParameters;
	FParticleKeyGenUniformBufferRef KeyGenUniformBuffer;
	const uint32 MaxGroupCount = 128;
	int32 TotalParticleCount = 0;

	FUnorderedAccessViewRHIParamRef OutputUAVs[2];
	OutputUAVs[0] = KeyBufferUAV;
	OutputUAVs[1] = SortedVertexBufferUAV;

	//make sure our outputs are safe to write to.
	RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, OutputUAVs, 2);

	// Grab the shader, set output.
	TShaderMapRef<FParticleSortKeyGenCS> KeyGenCS(GetGlobalShaderMap(FeatureLevel));
	RHICmdList.SetComputeShader(KeyGenCS->GetComputeShader());
	KeyGenCS->SetOutput(RHICmdList, KeyBufferUAV, SortedVertexBufferUAV);
	KeyGenCS->SetPositionTextures(RHICmdList, PositionTextureRHI);

	// For each simulation, generate keys and store them in the sorting buffers.
	const int32 SimulationCount = SimulationsToSort.Num();
	for (int32 SimulationIndex = 0; SimulationIndex < SimulationCount; ++SimulationIndex)
	{
		const FParticleSimulationSortInfo& SortInfo = SimulationsToSort[SimulationIndex];

		// Create the uniform buffer.
		const uint32 ParticleCount = SortInfo.ParticleCount;
		const uint32 AlignedParticleCount = ((ParticleCount + PARTICLE_KEY_GEN_THREAD_COUNT - 1) & (~(PARTICLE_KEY_GEN_THREAD_COUNT - 1)));
		const uint32 ChunkCount = AlignedParticleCount / PARTICLE_KEY_GEN_THREAD_COUNT;
		const uint32 GroupCount = FMath::Clamp<uint32>( ChunkCount, 1, MaxGroupCount );
		KeyGenParameters.ViewOrigin = SortInfo.ViewOrigin;
		KeyGenParameters.ChunksPerGroup = ChunkCount / GroupCount;
		KeyGenParameters.ExtraChunkCount = ChunkCount % GroupCount;
		KeyGenParameters.OutputOffset = TotalParticleCount;
		KeyGenParameters.EmitterKey = SimulationIndex << 16;
		KeyGenParameters.KeyCount = ParticleCount;
		KeyGenUniformBuffer = FParticleKeyGenUniformBufferRef::CreateUniformBufferImmediate( KeyGenParameters, UniformBuffer_SingleDraw );

		// Dispatch.
		KeyGenCS->SetParameters(RHICmdList, KeyGenUniformBuffer, SortInfo.VertexBufferSRV);
		DispatchComputeShader(RHICmdList, *KeyGenCS, GroupCount, 1, 1);

		//we may be able to remove this transition if each step isn't dependent on the previous one.
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutputUAVs, 2);

		// Update offset in to the buffer.
		TotalParticleCount += ParticleCount;
	}

	// Clear the output buffer.
	KeyGenCS->UnbindBuffers(RHICmdList);

	//make sure our outputs are readable as SRVs to further gfx steps.
	RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, OutputUAVs, 2);

	return TotalParticleCount;
}

/*------------------------------------------------------------------------------
	Buffers used to hold sorting results.
------------------------------------------------------------------------------*/

/**
 * Initialize RHI resources.
 */
void FParticleSortBuffers::InitRHI()
{
	if (RHISupportsComputeShaders(GShaderPlatformForFeatureLevel[GetFeatureLevel()]))
	{
		for (int32 BufferIndex = 0; BufferIndex < 2; ++BufferIndex)
		{
			FRHIResourceCreateInfo CreateInfo;

			KeyBuffers[BufferIndex] = RHICreateVertexBuffer( BufferSize * sizeof(uint32), BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess, CreateInfo);
			KeyBufferSRVs[BufferIndex] = RHICreateShaderResourceView( KeyBuffers[BufferIndex], /*Stride=*/ sizeof(uint32), PF_R32_UINT );
			KeyBufferUAVs[BufferIndex] = RHICreateUnorderedAccessView( KeyBuffers[BufferIndex], PF_R32_UINT );

			VertexBuffers[BufferIndex] = RHICreateVertexBuffer( BufferSize * sizeof(uint32), BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess, CreateInfo);
			VertexBufferSRVs[BufferIndex] = RHICreateShaderResourceView( VertexBuffers[BufferIndex], /*Stride=*/ sizeof(FFloat16)*2, PF_G16R16F );
			VertexBufferUAVs[BufferIndex] = RHICreateUnorderedAccessView( VertexBuffers[BufferIndex], PF_G16R16F );
			VertexBufferSortSRVs[BufferIndex] = RHICreateShaderResourceView( VertexBuffers[BufferIndex], /*Stride=*/ sizeof(uint32), PF_R32_UINT );
			VertexBufferSortUAVs[BufferIndex] = RHICreateUnorderedAccessView( VertexBuffers[BufferIndex], PF_R32_UINT );
		}
	}
}

/**
 * Release RHI resources.
 */
void FParticleSortBuffers::ReleaseRHI()
{
	for ( int32 BufferIndex = 0; BufferIndex < 2; ++BufferIndex )
	{
		KeyBufferUAVs[BufferIndex].SafeRelease();
		KeyBufferSRVs[BufferIndex].SafeRelease();
		KeyBuffers[BufferIndex].SafeRelease();

		VertexBufferSortUAVs[BufferIndex].SafeRelease();
		VertexBufferSortSRVs[BufferIndex].SafeRelease();
		VertexBufferUAVs[BufferIndex].SafeRelease();
		VertexBufferSRVs[BufferIndex].SafeRelease();
		VertexBuffers[BufferIndex].SafeRelease();
	}
}

/**
 * Retrieve buffers needed to sort on the GPU.
 */
FGPUSortBuffers FParticleSortBuffers::GetSortBuffers()
{
	FGPUSortBuffers SortBuffers;
		 
	for ( int32 BufferIndex = 0; BufferIndex < 2; ++BufferIndex )
	{
		SortBuffers.RemoteKeySRVs[BufferIndex] = KeyBufferSRVs[BufferIndex];
		SortBuffers.RemoteKeyUAVs[BufferIndex] = KeyBufferUAVs[BufferIndex];
		SortBuffers.RemoteValueSRVs[BufferIndex] = VertexBufferSortSRVs[BufferIndex];
		SortBuffers.RemoteValueUAVs[BufferIndex] = VertexBufferSortUAVs[BufferIndex];
	}

	return SortBuffers;
}

/*------------------------------------------------------------------------------
	Public interface.
------------------------------------------------------------------------------*/

/**
 * Sort particles on the GPU.
 * @param ParticleSortBuffers - Buffers to use while sorting GPU particles.
 * @param PositionTextureRHI - Texture containing world space position for all particles.
 * @param SimulationsToSort - A list of simulations that must be sorted.
 * @returns the buffer index in which sorting results are stored.
 */
int32 SortParticlesGPU(
	FRHICommandListImmediate& RHICmdList,
	FParticleSortBuffers& ParticleSortBuffers,
	FTexture2DRHIParamRef PositionTextureRHI,
	const TArray<FParticleSimulationSortInfo>& SimulationsToSort,
	ERHIFeatureLevel::Type FeatureLevel
	)
{
	SCOPED_DRAW_EVENTF(RHICmdList, ParticleSort, TEXT("ParticleSort_%d"), SimulationsToSort.Num());

	// Ensure the sorted vertex buffers are not currently bound as input streams.
	// They should only ever be bound to streams 0 or 1, so clear them.
	{
		const int32 StreamCount = 2;
		for (int32 StreamIndex = 0; StreamIndex < StreamCount; ++StreamIndex)
		{
			RHICmdList.SetStreamSource(StreamIndex, FVertexBufferRHIParamRef(), 0);
		}
	}

	// First generate keys for each emitter to be sorted.
	
	const int32 TotalParticleCount = GenerateParticleSortKeys(
		RHICmdList,
		ParticleSortBuffers.GetKeyBufferUAV(),
		ParticleSortBuffers.GetVertexBufferUAV(),
		PositionTextureRHI,
		SimulationsToSort,
		FeatureLevel
		);

	// Update stats.
	INC_DWORD_STAT_BY( STAT_SortedGPUEmitters, SimulationsToSort.Num() );
	INC_DWORD_STAT_BY( STAT_SortedGPUParticles, TotalParticleCount );

	// Now sort the particles based on the generated keys.
	const uint32 EmitterKeyMask = (1 << FMath::CeilLogTwo( SimulationsToSort.Num() )) - 1;
	const uint32 KeyMask = (EmitterKeyMask << 16) | 0xFFFF;
	FGPUSortBuffers SortBuffers = ParticleSortBuffers.GetSortBuffers();
	return SortGPUBuffers(RHICmdList, SortBuffers, 0, KeyMask, TotalParticleCount, FeatureLevel);
}

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GPUSort.cpp: Implementation for sorting buffers on the GPU.
=============================================================================*/

#include "GPUSort.h"
#include "RenderResource.h"
#include "Math/RandomStream.h"
#include "RenderingThread.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "SceneUtils.h"
#include "ShaderParameterUtils.h"
#include "GlobalShader.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPUSort, Log, All);

/*------------------------------------------------------------------------------
	Global settings.
------------------------------------------------------------------------------*/

static TAutoConsoleVariable<int32> CVarDebugOffsets(TEXT("GPUSort.DebugOffsets"),0,TEXT("Debug GPU sort offsets."));
static TAutoConsoleVariable<int32> CVarDebugSort(TEXT("GPUSort.DebugSort"),0,TEXT("Debug GPU sorting."));

#define RADIX_BITS 4
#define DIGIT_COUNT (1 << RADIX_BITS)
#define KEYS_PER_LOOP 8
#define THREAD_COUNT 128
#define TILE_SIZE (THREAD_COUNT * KEYS_PER_LOOP)
#define MAX_GROUP_COUNT 64
#define MAX_PASS_COUNT (32 / RADIX_BITS)

/**
 * Setup radix sort shader compiler environment.
 * @param OutEnvironment - The environment to set.
 */
void SetRadixSortShaderCompilerEnvironment( FShaderCompilerEnvironment& OutEnvironment )
{
	OutEnvironment.SetDefine( TEXT("RADIX_BITS"), RADIX_BITS );
	OutEnvironment.SetDefine( TEXT("THREAD_COUNT"), THREAD_COUNT );
	OutEnvironment.SetDefine( TEXT("KEYS_PER_LOOP"), KEYS_PER_LOOP );
	OutEnvironment.SetDefine( TEXT("MAX_GROUP_COUNT"), MAX_GROUP_COUNT );
	OutEnvironment.CompilerFlags.Add( CFLAG_StandardOptimization );
}

/*------------------------------------------------------------------------------
	Uniform buffer for passing in radix sort parameters.
------------------------------------------------------------------------------*/

BEGIN_UNIFORM_BUFFER_STRUCT( FRadixSortParameters, )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( uint32, RadixShift )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( uint32, TilesPerGroup )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( uint32, ExtraTileCount )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( uint32, ExtraKeyCount )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( uint32, GroupCount )
END_UNIFORM_BUFFER_STRUCT( FRadixSortParameters )

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FRadixSortParameters,TEXT("RadixSortUB"));

typedef TUniformBufferRef<FRadixSortParameters> FRadixSortUniformBufferRef;

/*------------------------------------------------------------------------------
	Global resources.
------------------------------------------------------------------------------*/

/**
 * Global sort offset buffer resources.
 */
class FSortOffsetBuffers : public FRenderResource
{
public:

	/** Vertex buffer storage for the actual offsets. */
	FVertexBufferRHIRef Buffers[2];
	/** Shader resource views for offset buffers. */
	FShaderResourceViewRHIRef BufferSRVs[2];
	/** Unordered access views for offset buffers. */
	FUnorderedAccessViewRHIRef BufferUAVs[2];

	/**
	 * Initialize RHI resources.
	 */
	virtual void InitRHI() override
	{
		if (RHISupportsComputeShaders(GShaderPlatformForFeatureLevel[GetFeatureLevel()]))
		{
			const int32 OffsetsCount = DIGIT_COUNT * MAX_GROUP_COUNT;
			const int32 OffsetsBufferSize = OffsetsCount * sizeof(uint32);
		
			for (int32 BufferIndex = 0; BufferIndex < 2; ++BufferIndex)
			{
				FRHIResourceCreateInfo CreateInfo;
				Buffers[BufferIndex] = RHICreateVertexBuffer(
					OffsetsBufferSize,
					BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess,
					CreateInfo);
				BufferSRVs[BufferIndex] = RHICreateShaderResourceView(
					Buffers[BufferIndex],
					/*Stride=*/ sizeof(uint32),
					/*Format=*/ PF_R32_UINT );
				BufferUAVs[BufferIndex] = RHICreateUnorderedAccessView(
					Buffers[BufferIndex],
					/*Format=*/ PF_R32_UINT );
			}
		}
	}

	/**
	 * Release RHI resources.
	 */
	virtual void ReleaseRHI() override
	{
		for ( int32 BufferIndex = 0; BufferIndex < 2; ++BufferIndex )
		{
			BufferUAVs[BufferIndex].SafeRelease();
			BufferSRVs[BufferIndex].SafeRelease();
			Buffers[BufferIndex].SafeRelease();
		}
	}

	/**
	 * Retrieve offsets from the buffer.
	 * @param OutOffsets - Array to hold the offsets.
	 * @param BufferIndex - Which buffer to retrieve.
	 */
	void GetOffsets( TArray<uint32>& OutOffsets, int32 BufferIndex )
	{
		const int32 OffsetsCount = DIGIT_COUNT * MAX_GROUP_COUNT;
		const int32 OffsetsBufferSize = OffsetsCount * sizeof(uint32);

		OutOffsets.Empty( OffsetsCount );
		OutOffsets.AddUninitialized( OffsetsCount );
		uint32* MappedOffsets = (uint32*)RHILockVertexBuffer( Buffers[BufferIndex], 0, OffsetsBufferSize, RLM_ReadOnly );
		FMemory::Memcpy( OutOffsets.GetData(), MappedOffsets, OffsetsBufferSize );
		RHIUnlockVertexBuffer( Buffers[BufferIndex] );
	}

	/**
	 * Dumps the contents of the offsets buffer via debugf.
	 * @param BufferIndex - Which buffer to dump.
	 */
	void DumpOffsets(int32 BufferIndex)
	{
		TArray<uint32> Offsets;
		uint32 GrandTotal = 0;

		GetOffsets(Offsets, BufferIndex);
		for (int32 GroupIndex = 0; GroupIndex < MAX_GROUP_COUNT; ++GroupIndex)
		{
			uint32 DigitTotal = 0;
			FString GroupOffsets;
			for (int32 DigitIndex = 0; DigitIndex < DIGIT_COUNT; ++DigitIndex)
			{
				const uint32 Value = Offsets[GroupIndex * DIGIT_COUNT + DigitIndex];
				GroupOffsets += FString::Printf(TEXT(" %04d"), Value);
				DigitTotal += Value;
				GrandTotal += Value;
			}
			UE_LOG(LogGPUSort, Log, TEXT("%s = %u"), *GroupOffsets, DigitTotal);
		}
		UE_LOG(LogGPUSort, Log, TEXT("Total: %u"), GrandTotal);
	}
};

/** The global sort offset buffer resources. */
TGlobalResource<FSortOffsetBuffers> GSortOffsetBuffers;

/**
 * This buffer is used to workaround a constant buffer bug that appears to
 * manifest itself on NVIDIA GPUs.
 */
class FRadixSortParametersBuffer : public FRenderResource
{
public:

	/** The vertex buffer used for storage. */
	FVertexBufferRHIRef SortParametersBufferRHI;
	/** Shader resource view in to the vertex buffer. */
	FShaderResourceViewRHIRef SortParametersBufferSRV;
	
	/**
	 * Initialize RHI resources.
	 */
	virtual void InitRHI() override
	{
		if (RHISupportsComputeShaders(GShaderPlatformForFeatureLevel[GetFeatureLevel()]))
		{
			FRHIResourceCreateInfo CreateInfo;
			SortParametersBufferRHI = RHICreateVertexBuffer(
				/*Size=*/ sizeof(FRadixSortParameters),
				/*Usage=*/ BUF_Volatile | BUF_ShaderResource,
				CreateInfo);
			SortParametersBufferSRV = RHICreateShaderResourceView(
				SortParametersBufferRHI, /*Stride=*/ sizeof(uint32), PF_R32_UINT 
				);
		}
	}

	/**
	 * Release RHI resources.
	 */
	virtual void ReleaseRHI() override
	{
		SortParametersBufferSRV.SafeRelease();
		SortParametersBufferRHI.SafeRelease();
	}
};

/** The global resource for the radix sort parameters buffer. */
TGlobalResource<FRadixSortParametersBuffer> GRadixSortParametersBuffer;

/*------------------------------------------------------------------------------
	The offset clearing kernel. This kernel just zeroes out the offsets buffer.

	Note that MAX_GROUP_COUNT * DIGIT_COUNT must be a multiple of THREAD_COUNT.
------------------------------------------------------------------------------*/

class FRadixSortClearOffsetsCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRadixSortClearOffsetsCS,Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return RHISupportsComputeShaders(Platform);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
		OutEnvironment.SetDefine( TEXT("RADIX_SORT_CLEAR_OFFSETS"), 1 );
		SetRadixSortShaderCompilerEnvironment( OutEnvironment );
	}

	/** Default constructor. */
	FRadixSortClearOffsetsCS()
	{
	}

	/** Initialization constructor. */
	explicit FRadixSortClearOffsetsCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		OutOffsets.Bind( Initializer.ParameterMap, TEXT("OutOffsets") );
	}

	/** Serialization. */
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << OutOffsets;
		return bShaderHasOutdatedParameters;
	}

	/**
	 * Set parameters for this shader.
	 */
	void SetParameters(FRHICommandList& RHICmdList, const FRadixSortUniformBufferRef& UniformBuffer )
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FRadixSortParameters>(), UniformBuffer );
	}

	/**
	 * Set output buffer for this shader.
	 */
	void SetOutput(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef OutOffsetsUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if ( OutOffsets.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutOffsets.GetBaseIndex(), OutOffsetsUAV);
		}
	}

	/**
	 * Unbinds any buffers that have been bound.
	 */
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if ( OutOffsets.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutOffsets.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:

	/** The buffer to which offsets will be written. */
	FShaderResourceParameter OutOffsets;
};
IMPLEMENT_SHADER_TYPE(,FRadixSortClearOffsetsCS,TEXT("/Engine/Private/RadixSortShaders.usf"),TEXT("RadixSort_ClearOffsets"),SF_Compute);

/*------------------------------------------------------------------------------
	The upsweep sorting kernel. This kernel performs an upsweep scan on all
	tiles allocated to this group and computes per-digit totals. These totals
	are output to the offsets buffer.
------------------------------------------------------------------------------*/

class FRadixSortUpsweepCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRadixSortUpsweepCS,Global);

public:

	static bool ShouldCache( EShaderPlatform Platform )
	{
		return RHISupportsComputeShaders(Platform);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
		OutEnvironment.SetDefine( TEXT("RADIX_SORT_UPSWEEP"), 1 );
		SetRadixSortShaderCompilerEnvironment( OutEnvironment );
	}

	/** Default constructor. */
	FRadixSortUpsweepCS()
	{
	}

	/** Initialization constructor. */
	explicit FRadixSortUpsweepCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		RadixSortParameterBuffer.Bind( Initializer.ParameterMap, TEXT("RadixSortParameterBuffer") );
		InKeys.Bind( Initializer.ParameterMap, TEXT("InKeys") );
		OutOffsets.Bind( Initializer.ParameterMap, TEXT("OutOffsets") );
	}

	/** Serialization. */
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << RadixSortParameterBuffer;
		Ar << InKeys;
		Ar << OutOffsets;
		return bShaderHasOutdatedParameters;
	}

	/**
	 * Returns true if this shader was compiled to require the constant buffer
	 * workaround.
	 */
	bool RequiresConstantBufferWorkaround() const
	{
		return RadixSortParameterBuffer.IsBound();
	}

	/**
	 * Set parameters for this shader.
	 */
	void SetParameters(FRHICommandList& RHICmdList, FShaderResourceViewRHIParamRef InKeysSRV, FRadixSortUniformBufferRef& RadixSortUniformBuffer, FShaderResourceViewRHIParamRef RadixSortParameterBufferSRV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FRadixSortParameters>(), RadixSortUniformBuffer );
		if ( InKeys.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InKeys.GetBaseIndex(), InKeysSRV);
		}
		if ( RadixSortParameterBuffer.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, RadixSortParameterBuffer.GetBaseIndex(), RadixSortParameterBufferSRV);
		}
	}

	/**
	 * Set output buffer for this shader.
	 */
	void SetOutput(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef OutOffsetsUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if ( OutOffsets.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutOffsets.GetBaseIndex(), OutOffsetsUAV);
		}
	}

	/**
	 * Unbinds any buffers that have been bound.
	 */
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if ( RadixSortParameterBuffer.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, RadixSortParameterBuffer.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if ( InKeys.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InKeys.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if ( OutOffsets.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutOffsets.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:

	/** Uniform parameters stored in a vertex buffer, used to workaround an NVIDIA driver bug. */
	FShaderResourceParameter RadixSortParameterBuffer;
	/** The buffer containing input keys. */
	FShaderResourceParameter InKeys;
	/** The buffer to which offsets will be written. */
	FShaderResourceParameter OutOffsets;
};
IMPLEMENT_SHADER_TYPE(,FRadixSortUpsweepCS,TEXT("/Engine/Private/RadixSortShaders.usf"),TEXT("RadixSort_Upsweep"),SF_Compute);

/*------------------------------------------------------------------------------
	The spine sorting kernel. This kernel performs a parallel prefix sum on
	the offsets computed by each work group in upsweep. The outputs will be used
	by individual work groups in downsweep to compute the final location of keys.
------------------------------------------------------------------------------*/

class FRadixSortSpineCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRadixSortSpineCS,Global);

public:

	static bool ShouldCache( EShaderPlatform Platform )
	{
		return RHISupportsComputeShaders(Platform);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
		OutEnvironment.SetDefine( TEXT("RADIX_SORT_SPINE"), 1 );
		SetRadixSortShaderCompilerEnvironment( OutEnvironment );
	}

	/** Default constructor. */
	FRadixSortSpineCS()
	{
	}

	/** Initialization constructor. */
	explicit FRadixSortSpineCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		InOffsets.Bind( Initializer.ParameterMap, TEXT("InOffsets") );
		OutOffsets.Bind( Initializer.ParameterMap, TEXT("OutOffsets") );
	}

	/** Serialization. */
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << InOffsets;
		Ar << OutOffsets;
		return bShaderHasOutdatedParameters;
	}

	/**
	 * Set parameters for this shader.
	 */
	void SetParameters(FRHICommandList& RHICmdList, FShaderResourceViewRHIParamRef InOffsetsSRV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if ( InOffsets.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InOffsets.GetBaseIndex(), InOffsetsSRV);
		}
	}

	/**
	 * Set output buffer for this shader.
	 */
	void SetOutput(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef OutOffsetsUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if ( OutOffsets.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutOffsets.GetBaseIndex(), OutOffsetsUAV);
		}
	}

	/**
	 * Unbinds any buffers that have been bound.
	 */
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if ( InOffsets.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InOffsets.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if ( OutOffsets.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutOffsets.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:

	/** The buffer containing input offsets. */
	FShaderResourceParameter InOffsets;
	/** The buffer to which offsets will be written. */
	FShaderResourceParameter OutOffsets;
};
IMPLEMENT_SHADER_TYPE(,FRadixSortSpineCS,TEXT("/Engine/Private/RadixSortShaders.usf"),TEXT("RadixSort_Spine"),SF_Compute);

/*------------------------------------------------------------------------------
	The downsweep sorting kernel. This kernel reads the per-work group partial
	sums in to LocalTotals. The kernel then recomputes much of the work done
	upsweep, this time computing a full set of prefix sums so that keys can be
	scattered in to global memory.
------------------------------------------------------------------------------*/

class FRadixSortDownsweepCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRadixSortDownsweepCS,Global);

public:

	static bool ShouldCache( EShaderPlatform Platform )
	{
		return RHISupportsComputeShaders(Platform);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
		OutEnvironment.SetDefine( TEXT("RADIX_SORT_DOWNSWEEP"), 1 );
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
		SetRadixSortShaderCompilerEnvironment( OutEnvironment );
	}

	/** Default constructor. */
	FRadixSortDownsweepCS()
	{
	}

	/** Initialization constructor. */
	explicit FRadixSortDownsweepCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		RadixSortParameterBuffer.Bind( Initializer.ParameterMap, TEXT("RadixSortParameterBuffer") );
		InKeys.Bind( Initializer.ParameterMap, TEXT("InKeys") );
		InValues.Bind( Initializer.ParameterMap, TEXT("InValues") );
		InOffsets.Bind( Initializer.ParameterMap, TEXT("InOffsets") );
		OutKeys.Bind( Initializer.ParameterMap, TEXT("OutKeys") );
		OutValues.Bind( Initializer.ParameterMap, TEXT("OutValues") );
	}

	/** Serialization. */
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << RadixSortParameterBuffer;
		Ar << InKeys;
		Ar << InValues;
		Ar << InOffsets;
		Ar << OutKeys;
		Ar << OutValues;
		return bShaderHasOutdatedParameters;
	}

	/**
	 * Returns true if this shader was compiled to require the constant buffer
	 * workaround.
	 */
	bool RequiresConstantBufferWorkaround() const
	{
		return RadixSortParameterBuffer.IsBound();
	}

	/**
	 * Set parameters for this shader.
	 */
	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef InKeysSRV,
		FShaderResourceViewRHIParamRef InValuesSRV,
		FShaderResourceViewRHIParamRef InOffsetsSRV,
		FRadixSortUniformBufferRef& RadixSortUniformBuffer,
		FShaderResourceViewRHIParamRef RadixSortParameterBufferSRV )
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FRadixSortParameters>(), RadixSortUniformBuffer );
		if ( RadixSortParameterBuffer.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, RadixSortParameterBuffer.GetBaseIndex(), RadixSortParameterBufferSRV);
		}
		if ( InKeys.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InKeys.GetBaseIndex(), InKeysSRV);
		}
		if ( InValues.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InValues.GetBaseIndex(), InValuesSRV);
		}
		if ( InOffsets.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InOffsets.GetBaseIndex(), InOffsetsSRV);
		}
	}

	/**
	 * Set output buffer for this shader.
	 */
	void SetOutput(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef OutKeysUAV, FUnorderedAccessViewRHIParamRef OutValuesUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if ( OutKeys.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutKeys.GetBaseIndex(), OutKeysUAV);
		}
		if ( OutValues.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutValues.GetBaseIndex(), OutValuesUAV);
		}
	}

	/**
	 * Unbinds any buffers that have been bound.
	 */
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if ( RadixSortParameterBuffer.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, RadixSortParameterBuffer.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if ( InKeys.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InKeys.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if ( InValues.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InValues.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if ( InOffsets.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InOffsets.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if ( OutKeys.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutKeys.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if ( OutValues.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutValues.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:

	/** Uniform parameters stored in a vertex buffer, used to workaround an NVIDIA driver bug. */
	FShaderResourceParameter RadixSortParameterBuffer;
	/** The buffer containing input keys. */
	FShaderResourceParameter InKeys;
	/** The buffer containing input values. */
	FShaderResourceParameter InValues;
	/** The buffer from which offsets will be read. */
	FShaderResourceParameter InOffsets;
	/** The buffer to which sorted keys will be written. */
	FShaderResourceParameter OutKeys;
	/** The buffer to which sorted values will be written. */
	FShaderResourceParameter OutValues;
};
IMPLEMENT_SHADER_TYPE(,FRadixSortDownsweepCS,TEXT("/Engine/Private/RadixSortShaders.usf"),TEXT("RadixSort_Downsweep"),SF_Compute);

/*------------------------------------------------------------------------------
	Public interface.
------------------------------------------------------------------------------*/

/**
 * Sort a buffer on the GPU.
 * @param SortBuffers - The buffer to sort including required views and a ping-
 *			pong location of appropriate size.
 * @param BufferIndex - Index of the buffer containing keys.
 * @param KeyMask - Bitmask indicating which key bits contain useful information.
 * @param Count - How many items in the buffer need to be sorted.
 * @returns The index of the buffer containing sorted results.
 */
int32 SortGPUBuffers(FRHICommandListImmediate& RHICmdList, FGPUSortBuffers SortBuffers, int32 BufferIndex, uint32 KeyMask, int32 Count, ERHIFeatureLevel::Type FeatureLevel)
{
	FRadixSortParameters SortParameters;
	FRadixSortUniformBufferRef SortUniformBufferRef;
	const bool bDebugOffsets = CVarDebugOffsets.GetValueOnRenderThread() != 0;
	const bool bDebugSort = CVarDebugSort.GetValueOnRenderThread() != 0;

	check(RHISupportsComputeShaders(GShaderPlatformForFeatureLevel[FeatureLevel]));

	SCOPED_DRAW_EVENTF(RHICmdList, SortGPU, TEXT("SortGPU_%d"), Count);

	// Determine how many tiles need to be sorted.
	const int32 TileCount = Count / TILE_SIZE;

	// Determine how many groups will be needed.
	int32 GroupCount = TileCount;
	if ( GroupCount > MAX_GROUP_COUNT )
	{
		GroupCount = MAX_GROUP_COUNT;
	}
	else if ( GroupCount == 0 )
	{
		GroupCount = 1;
	}

	// The number of tiles processed by each group.
	const int32 TilesPerGroup = TileCount / GroupCount;

	// The number of additional tiles that need to be processed.
	const int32 ExtraTileCount = TileCount % GroupCount;

	// The number of additional keys that need to be processed.
	const int32 ExtraKeyCount = Count % TILE_SIZE;

	// Determine how many passes are required.
	const int32 BitCount = 32;
	const int32 PassCount = BitCount / RADIX_BITS;

	// Setup sort parameters.
	SortParameters.RadixShift = 0;
	SortParameters.TilesPerGroup = TilesPerGroup;
	SortParameters.ExtraTileCount = ExtraTileCount;
	SortParameters.ExtraKeyCount = ExtraKeyCount;
	SortParameters.GroupCount = GroupCount;

	// Grab shaders.
	auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<FRadixSortClearOffsetsCS> ClearOffsetsCS(ShaderMap);
	TShaderMapRef<FRadixSortUpsweepCS> UpsweepCS(ShaderMap);
	TShaderMapRef<FRadixSortSpineCS> SpineCS(ShaderMap);
	TShaderMapRef<FRadixSortDownsweepCS> DownsweepCS(ShaderMap);

	// Constant buffer workaround. Both shaders must use either the constant buffer or vertex buffer.
	check( UpsweepCS->RequiresConstantBufferWorkaround() == DownsweepCS->RequiresConstantBufferWorkaround() );
	const bool bUseConstantBufferWorkaround = UpsweepCS->RequiresConstantBufferWorkaround();


	
	// Execute each pass as needed.
	uint32 PassBits = DIGIT_COUNT - 1;
	for ( int32 PassIndex = 0; PassIndex < PassCount; ++PassIndex )
	{
		// Check to see if these key bits matter.
		if ( (PassBits & KeyMask) != 0 )
		{
			// Update uniform buffer.
			if ( bUseConstantBufferWorkaround )
			{
				void* ParameterBuffer = RHILockVertexBuffer( GRadixSortParametersBuffer.SortParametersBufferRHI, 0, sizeof(FRadixSortParameters), RLM_WriteOnly );
				FMemory::Memcpy( ParameterBuffer, &SortParameters, sizeof(FRadixSortParameters) );
				RHIUnlockVertexBuffer( GRadixSortParametersBuffer.SortParametersBufferRHI );
			}
			else
			{
				SortUniformBufferRef = FRadixSortUniformBufferRef::CreateUniformBufferImmediate( SortParameters, UniformBuffer_SingleDraw );
			}

			//make UAV safe for clear
			RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, GSortOffsetBuffers.BufferUAVs[0]);
			
			// Clear the offsets buffer.			
			RHICmdList.SetComputeShader(ClearOffsetsCS->GetComputeShader());			
			ClearOffsetsCS->SetOutput(RHICmdList, GSortOffsetBuffers.BufferUAVs[0]);
			DispatchComputeShader(RHICmdList, *ClearOffsetsCS, 1, 1 ,1 );
			ClearOffsetsCS->UnbindBuffers(RHICmdList);

			//make UAV safe for readback
			RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, GSortOffsetBuffers.BufferUAVs[0]);

			// Phase 1: Scan upsweep to compute per-digit totals.
			RHICmdList.SetComputeShader(UpsweepCS->GetComputeShader());
			UpsweepCS->SetOutput(RHICmdList, GSortOffsetBuffers.BufferUAVs[0]);
			UpsweepCS->SetParameters(RHICmdList, SortBuffers.RemoteKeySRVs[BufferIndex], SortUniformBufferRef, GRadixSortParametersBuffer.SortParametersBufferSRV );
			DispatchComputeShader(RHICmdList, *UpsweepCS, GroupCount, 1, 1 );
			UpsweepCS->UnbindBuffers(RHICmdList);

			//barrier both UAVS since for next step.
			FUnorderedAccessViewRHIParamRef PrePhase2BarrierUAVS[2];
			PrePhase2BarrierUAVS[0] = GSortOffsetBuffers.BufferUAVs[0];
			PrePhase2BarrierUAVS[1] = GSortOffsetBuffers.BufferUAVs[1];

			RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, PrePhase2BarrierUAVS, 2);

			if (bDebugOffsets)
			{
				UE_LOG(LogGPUSort, Log, TEXT("\n========== UPSWEEP =========="));
				GSortOffsetBuffers.DumpOffsets(0);
			}

			// Phase 2: Parallel prefix scan on the offsets buffer.
			RHICmdList.SetComputeShader(SpineCS->GetComputeShader());
			SpineCS->SetOutput(RHICmdList, GSortOffsetBuffers.BufferUAVs[1]);
			SpineCS->SetParameters(RHICmdList, GSortOffsetBuffers.BufferSRVs[0] );
			DispatchComputeShader(RHICmdList, *SpineCS, 1, 1, 1 );
			SpineCS->UnbindBuffers(RHICmdList);

			if (bDebugOffsets)
			{
				UE_LOG(LogGPUSort, Log, TEXT("\n========== SPINE =========="));
				GSortOffsetBuffers.DumpOffsets(1);
			}

			//UAV is going to SRV, so transition to Readable.
			RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, GSortOffsetBuffers.BufferUAVs[1]);

			FUnorderedAccessViewRHIParamRef PrePhase3BarrierUAVS[2];
			PrePhase3BarrierUAVS[0] = SortBuffers.RemoteKeyUAVs[BufferIndex ^ 0x1];
			PrePhase3BarrierUAVS[1] = SortBuffers.RemoteValueUAVs[BufferIndex ^ 0x1];
			RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, PrePhase3BarrierUAVS, 2);

			// Phase 3: Downsweep to compute final offsets and scatter keys.
			RHICmdList.SetComputeShader(DownsweepCS->GetComputeShader());
			DownsweepCS->SetOutput(RHICmdList, SortBuffers.RemoteKeyUAVs[BufferIndex ^ 0x1], SortBuffers.RemoteValueUAVs[BufferIndex ^ 0x1]);
			DownsweepCS->SetParameters(RHICmdList, SortBuffers.RemoteKeySRVs[BufferIndex], SortBuffers.RemoteValueSRVs[BufferIndex], GSortOffsetBuffers.BufferSRVs[1], SortUniformBufferRef, GRadixSortParametersBuffer.SortParametersBufferSRV );
			DispatchComputeShader(RHICmdList, *DownsweepCS, GroupCount, 1, 1 );
			DownsweepCS->UnbindBuffers(RHICmdList);

			RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, PrePhase3BarrierUAVS, 2);

			// Flip buffers.
			BufferIndex ^= 0x1;

			if (bDebugSort || bDebugOffsets)
			{
				return BufferIndex;
			}
		}

		// Update the radix shift for the next pass and flip buffers.
		SortParameters.RadixShift += RADIX_BITS;
		PassBits <<= RADIX_BITS;
	}

	return BufferIndex;
}

/*------------------------------------------------------------------------------
	Testing.
------------------------------------------------------------------------------*/

enum
{
	GPU_SORT_TEST_SIZE_SMALL = (1 << 9),
	GPU_SORT_TEST_SIZE_LARGE = (1 << 20),
	GPU_SORT_TEST_SIZE_MIN = (1 << 4),
	GPU_SORT_TEST_SIZE_MAX = (1 << 20)
};

/**
 * Execute a GPU sort test.
 * @param TestSize - The number of elements to sort.
 * @returns true if the sort succeeded.
 */
static bool RunGPUSortTest(FRHICommandListImmediate& RHICmdList, int32 TestSize, ERHIFeatureLevel::Type FeatureLevel)
{
	FRandomStream RandomStream(0x3819FFE4);
	FGPUSortBuffers SortBuffers;
	TArray<uint32> Keys;
	TArray<uint32> RefSortedKeys;
	TArray<uint32> SortedKeys;
	TArray<uint32> SortedValues;
	FVertexBufferRHIRef KeysBufferRHI[2], ValuesBufferRHI[2];
	FShaderResourceViewRHIRef KeysBufferSRV[2], ValuesBufferSRV[2];
	FUnorderedAccessViewRHIRef KeysBufferUAV[2], ValuesBufferUAV[2];
	int32 ResultBufferIndex;
	int32 IncorrectKeyIndex = 0;
	const int32 BufferSize = TestSize * sizeof(uint32);
	const bool bDebugOffsets = CVarDebugOffsets.GetValueOnRenderThread() != 0;
	const bool bDebugSort = CVarDebugSort.GetValueOnRenderThread() != 0;

	if (!RHISupportsComputeShaders(GShaderPlatformForFeatureLevel[FeatureLevel]))
	{
		return false;
	}

	// Generate the test keys.
	Keys.Reserve(TestSize);
	Keys.AddUninitialized(TestSize);
	for (int32 KeyIndex = 0; KeyIndex < TestSize; ++KeyIndex)
	{
		Keys[KeyIndex] = RandomStream.GetUnsignedInt();
	}

	// Perform a reference sort on the CPU.
	RefSortedKeys = Keys;
	RefSortedKeys.Sort();

	// Allocate GPU resources.
	for (int32 BufferIndex = 0; BufferIndex < 2; ++BufferIndex)
	{
		FRHIResourceCreateInfo CreateInfo;
		KeysBufferRHI[BufferIndex] = RHICmdList.CreateVertexBuffer(BufferSize, BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess, CreateInfo);
		KeysBufferSRV[BufferIndex] = RHICmdList.CreateShaderResourceView(KeysBufferRHI[BufferIndex], /*Stride=*/ sizeof(uint32), PF_R32_UINT);
		KeysBufferUAV[BufferIndex] = RHICmdList.CreateUnorderedAccessView(KeysBufferRHI[BufferIndex], PF_R32_UINT);
		ValuesBufferRHI[BufferIndex] = RHICmdList.CreateVertexBuffer(BufferSize, BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess, CreateInfo);
		ValuesBufferSRV[BufferIndex] = RHICmdList.CreateShaderResourceView(ValuesBufferRHI[BufferIndex], /*Stride=*/ sizeof(uint32), PF_R32_UINT);
		ValuesBufferUAV[BufferIndex] = RHICmdList.CreateUnorderedAccessView(ValuesBufferRHI[BufferIndex], PF_R32_UINT);
	}

	// Upload initial keys and values to the GPU.
	{
		uint32* Buffer;

		Buffer = (uint32*)RHICmdList.LockVertexBuffer(KeysBufferRHI[0], /*Offset=*/ 0, BufferSize, RLM_WriteOnly);
		FMemory::Memcpy(Buffer, Keys.GetData(), BufferSize);
		RHICmdList.UnlockVertexBuffer(KeysBufferRHI[0]);
		Buffer = (uint32*)RHICmdList.LockVertexBuffer(ValuesBufferRHI[0], /*Offset=*/ 0, BufferSize, RLM_WriteOnly);
		FMemory::Memcpy(Buffer, Keys.GetData(), BufferSize);
		RHICmdList.UnlockVertexBuffer(ValuesBufferRHI[0]);
	}

	// Execute the GPU sort.
	for (int32 BufferIndex = 0; BufferIndex < 2; ++BufferIndex)
	{
		SortBuffers.RemoteKeySRVs[BufferIndex] = KeysBufferSRV[BufferIndex];
		SortBuffers.RemoteKeyUAVs[BufferIndex] = KeysBufferUAV[BufferIndex];
		SortBuffers.RemoteValueSRVs[BufferIndex] = ValuesBufferSRV[BufferIndex];
		SortBuffers.RemoteValueUAVs[BufferIndex] = ValuesBufferUAV[BufferIndex];
	}
	ResultBufferIndex = SortGPUBuffers(RHICmdList, SortBuffers, 0, 0xFFFFFFFF, TestSize, FeatureLevel);

	// Download results from the GPU.
	{
		uint32* Buffer;

		SortedKeys.Reserve(TestSize);
		SortedKeys.AddUninitialized(TestSize);
		SortedValues.Reserve(TestSize);
		SortedValues.AddUninitialized(TestSize);

		Buffer = (uint32*)RHICmdList.LockVertexBuffer(KeysBufferRHI[ResultBufferIndex], /*Offset=*/ 0, BufferSize, RLM_ReadOnly);
		FMemory::Memcpy(SortedKeys.GetData(), Buffer, BufferSize);
		RHICmdList.UnlockVertexBuffer(KeysBufferRHI[ResultBufferIndex]);
		Buffer = (uint32*)RHICmdList.LockVertexBuffer(ValuesBufferRHI[ResultBufferIndex], /*Offset=*/ 0, BufferSize, RLM_ReadOnly);
		FMemory::Memcpy(SortedValues.GetData(), Buffer, BufferSize);
		RHICmdList.UnlockVertexBuffer(ValuesBufferRHI[ResultBufferIndex]);
	}

	// Verify results.
	bool bSucceeded = true;
	for (int32 KeyIndex = 0; KeyIndex < TestSize; ++KeyIndex)
	{
		if (SortedKeys[KeyIndex] != RefSortedKeys[KeyIndex] || SortedValues[KeyIndex] != RefSortedKeys[KeyIndex])
		{
			IncorrectKeyIndex = KeyIndex;
			bSucceeded = false;
			break;
		}
	}

	if (bSucceeded)
	{
		UE_LOG(LogGPUSort, Log, TEXT("GPU Sort Test (%d keys+values) succeeded."), TestSize);
	}
	else
	{
		UE_LOG(LogGPUSort, Log, TEXT("GPU Sort Test (%d keys+values) FAILED."), TestSize);

		if (bDebugSort || !bDebugOffsets)
		{
			const int32 FirstKeyIndex = FMath::Max<int32>(IncorrectKeyIndex - 8, 0);
			const int32 LastKeyIndex = FMath::Min<int32>(FirstKeyIndex + 1024, TestSize - 1);
			UE_LOG(LogGPUSort, Log, TEXT("       Input    : S.Keys   : S.Values : Ref Sorted Keys"));
			for (int32 KeyIndex = FirstKeyIndex; KeyIndex <= LastKeyIndex; ++KeyIndex)
			{
				UE_LOG(LogGPUSort, Log, TEXT("%04u : %08X : %08X : %08X : %08X%s"),
					KeyIndex,
					Keys[KeyIndex],
					SortedKeys[KeyIndex],
					SortedValues[KeyIndex],
					RefSortedKeys[KeyIndex],
					(KeyIndex == IncorrectKeyIndex) ? TEXT(" <----") : TEXT("")
					);
			}
		}
	}

	return bSucceeded;
}

/**
 * Executes a sort test with debug information enabled.
 * @param TestSize - The number of elements to sort.
 */
static void RunGPUSortTestWithDebug(FRHICommandListImmediate& RHICmdList, int32 TestSize, ERHIFeatureLevel::Type FeatureLevel)
{
	static IConsoleVariable* IVarDebugOffsets = IConsoleManager::Get().FindConsoleVariable(TEXT("GPUSort.DebugOffsets"));
	static IConsoleVariable* IVarDebugSort = IConsoleManager::Get().FindConsoleVariable(TEXT("GPUSort.DebugSort"));
	const bool bWasDebuggingOffsets = CVarDebugOffsets.GetValueOnRenderThread() != 0;
	const bool bWasDebuggingSort = CVarDebugSort.GetValueOnRenderThread() != 0;
	IVarDebugOffsets->Set(1, ECVF_SetByCode);
	IVarDebugSort->Set(1, ECVF_SetByCode);
	RunGPUSortTest(RHICmdList, TestSize, FeatureLevel);
	IVarDebugOffsets->Set(bWasDebuggingOffsets ? 1 : 0, ECVF_SetByCode);
	IVarDebugSort->Set(bWasDebuggingSort ? 1 : 0, ECVF_SetByCode);
}

/**
 * Executes a sort test. If the sort fails, it reruns the sort with debug
 * information enabled.
 * @param TestSize - The number of elements to sort.
 */
static bool TestGPUSortForSize(FRHICommandListImmediate& RHICmdList, int32 TestSize, ERHIFeatureLevel::Type FeatureLevel)
{
	check(IsInRenderingThread());
	const bool bResult = RunGPUSortTest(RHICmdList, TestSize, FeatureLevel);
	if (bResult == false)
	{
		RunGPUSortTestWithDebug(RHICmdList, TestSize, FeatureLevel);
	}
	return bResult;
}

/**
 * Test that GPU sorting works.
 * @param TestToRun - The test to run.
 */
static bool TestGPUSort_RenderThread(FRHICommandListImmediate& RHICmdList, EGPUSortTest TestToRun, ERHIFeatureLevel::Type FeatureLevel)
{
	check(IsInRenderingThread());

	switch (TestToRun)
	{
	case GPU_SORT_TEST_SMALL:
		return TestGPUSortForSize(RHICmdList, GPU_SORT_TEST_SIZE_SMALL, FeatureLevel);

	case GPU_SORT_TEST_LARGE:
		return TestGPUSortForSize(RHICmdList, GPU_SORT_TEST_SIZE_LARGE, FeatureLevel);

	case GPU_SORT_TEST_EXHAUSTIVE:
		{
			// First test all power-of-two sizes within the range.
			int32 TestSize = GPU_SORT_TEST_SIZE_MIN;
			while (TestSize <= GPU_SORT_TEST_SIZE_MAX)
			{
				if (TestGPUSortForSize(RHICmdList, TestSize, FeatureLevel) == false)
				{
					return false;
				}
				TestSize <<= 1;
			}

			// Offset the size by one to test non-power-of-two.
			TestSize = GPU_SORT_TEST_SIZE_MIN;
			while (TestSize <= GPU_SORT_TEST_SIZE_MAX)
			{
				if (TestGPUSortForSize(RHICmdList, TestSize - 1, FeatureLevel) == false)
				{
					return false;
				}
				TestSize <<= 1;
			}
		}
		return true;

	case GPU_SORT_TEST_RANDOM:
		for ( int32 i = 0; i < 1000; ++i )
		{
			int32 TestSize = FMath::TruncToInt(FMath::SRand() * (float)(GPU_SORT_TEST_SIZE_MAX - GPU_SORT_TEST_SIZE_MIN)) + GPU_SORT_TEST_SIZE_MIN;
			if (TestGPUSortForSize(RHICmdList, (TestSize + 0xF) & 0xFFFFFFF0, FeatureLevel) == false)
			{
				return false;
			}
		}
		return true;
	}

	return true;
}

/**
 * Test that GPU sorting works.
 * @param TestToRun - The test to run.
 */
void TestGPUSort(EGPUSortTest TestToRun, ERHIFeatureLevel::Type FeatureLevel)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FTestGPUSortCommand,
		EGPUSortTest, TestToRun, TestToRun,
		ERHIFeatureLevel::Type, FeatureLevel, FeatureLevel,
	{
		TestGPUSort_RenderThread(RHICmdList, TestToRun, FeatureLevel);
	});
}

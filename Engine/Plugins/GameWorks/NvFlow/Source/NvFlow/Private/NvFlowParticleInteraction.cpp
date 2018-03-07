#include "NvFlowCommon.h"

#include "RHI.h"
#include "NvFlowInterop.h"

#include "FlowGridAsset.h"
#include "FlowGridActor.h"
#include "FlowGridComponent.h"
#include "FlowEmitterComponent.h"
#include "FlowMaterial.h"
#include "FlowRenderMaterial.h"
#include "FlowGridSceneProxy.h"

// NvFlow begin
#if WITH_NVFLOW

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "ScreenRendering.h"
#include "PostProcess/SceneFilterRendering.h"
#include "LightPropagationVolume.h"
#include "SceneUtils.h"
#include "HardwareInfo.h"

#include "Stats.h"
#include "GridAccessHooksNvFlow.h"

#include "NvFlowScene.h"

#if WITH_NVFLOW_BACKEND

namespace
{
	inline FIntVector NvFlowConvert(const NvFlowUint4& in)
	{
		return FIntVector(in.x, in.y, in.z);
	}

	inline FVector NvFlowConvert(const NvFlowFloat4& in)
	{
		return FVector(in.x, in.y, in.z);
	}

	inline FMatrix NvFlowGetVolumeToWorld(FFlowGridSceneProxy* FlowGridSceneProxy)
	{
		const FBoxSphereBounds& LocalBounds = FlowGridSceneProxy->GetLocalBounds();
		const FMatrix& LocalToWorld = FlowGridSceneProxy->GetLocalToWorld();

		FMatrix VolumeToLocal = FMatrix(
			FPlane(LocalBounds.BoxExtent.X * 2, 0.0f, 0.0f, 0.0f),
			FPlane(0.0f, LocalBounds.BoxExtent.Y * 2, 0.0f, 0.0f),
			FPlane(0.0f, 0.0f, LocalBounds.BoxExtent.Z * 2, 0.0f),
			FPlane(LocalBounds.Origin - LocalBounds.BoxExtent, 1.0f));

		return VolumeToLocal * LocalToWorld;
	}
	inline FMatrix NvFlowGetWorldToVolume(FFlowGridSceneProxy* FlowGridSceneProxy)
	{
		return NvFlowGetVolumeToWorld(FlowGridSceneProxy).Inverse();
	}
}

bool NvFlow::Scene::getExportParams(FRHICommandListImmediate& RHICmdList, GridExportParamsNvFlow& OutParams)
{
	auto gridExport = NvFlowGridGetGridExport(m_renderContext, m_grid);

	auto gridExportHandle = NvFlowGridExportGetHandle(gridExport, m_renderContext, eNvFlowGridTextureChannelVelocity);
	check(gridExportHandle.numLayerViews > 0);

	// Note: assuming single layer
	const NvFlowUint layerIdx = 0u;

	NvFlowGridExportLayeredView gridExportLayeredView;
	NvFlowGridExportGetLayeredView(gridExportHandle, &gridExportLayeredView);
	NvFlowGridExportLayerView gridExportLayerView;
	NvFlowGridExportGetLayerView(gridExportHandle, layerIdx, &gridExportLayerView);

	OutParams.DataSRV = m_context->m_flowInterop->CreateSRV(RHICmdList.GetContext(), m_renderContext, gridExportLayerView.data);
	OutParams.BlockTableSRV = m_context->m_flowInterop->CreateSRV(RHICmdList.GetContext(), m_renderContext, gridExportLayerView.mapping.blockTable);

	const auto& shaderParams = gridExportLayeredView.mapping.shaderParams;

	OutParams.BlockDim = NvFlowConvert(shaderParams.blockDim);
	OutParams.BlockDimBits = NvFlowConvert(shaderParams.blockDimBits);
	OutParams.BlockDimInv = NvFlowConvert(shaderParams.blockDimInv);
	OutParams.LinearBlockDim = NvFlowConvert(shaderParams.linearBlockDim);
	OutParams.LinearBlockOffset = NvFlowConvert(shaderParams.linearBlockOffset);
	OutParams.DimInv = NvFlowConvert(shaderParams.dimInv);
	OutParams.VDim = NvFlowConvert(shaderParams.vdim);
	OutParams.VDimInv = NvFlowConvert(shaderParams.vdimInv);
	OutParams.PoolGridDim = NvFlowConvert(shaderParams.poolGridDim);
	OutParams.GridDim = NvFlowConvert(shaderParams.gridDim);
	OutParams.IsVTR = (shaderParams.isVTR.x != 0);

	OutParams.WorldToVolume = NvFlowGetWorldToVolume(FlowGridSceneProxy);
	OutParams.VelocityScale = scale;

	OutParams.GridToParticleAccelTimeConstant = FlowGridSceneProxy->FlowGridProperties->GridToParticleAccelTimeConstant;
	OutParams.GridToParticleDecelTimeConstant = FlowGridSceneProxy->FlowGridProperties->GridToParticleDecelTimeConstant;
	OutParams.GridToParticleThresholdMultiplier = FlowGridSceneProxy->FlowGridProperties->GridToParticleThresholdMultiplier;
	return true;
}


#define MASK_FROM_PARTICLES_THREAD_COUNT 64

BEGIN_UNIFORM_BUFFER_STRUCT(FNvFlowMaskFromParticlesParameters, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, TextureSizeX)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, TextureSizeY)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, ParticleCount)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FIntVector, MaskDim)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FMatrix, WorldToVolume)
END_UNIFORM_BUFFER_STRUCT(FNvFlowMaskFromParticlesParameters)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FNvFlowMaskFromParticlesParameters, TEXT("NvFlowMaskFromParticles"));
typedef TUniformBufferRef<FNvFlowMaskFromParticlesParameters> FNvFlowMaskFromParticlesUniformBufferRef;

class FNvFlowMaskFromParticlesCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FNvFlowMaskFromParticlesCS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_COUNT"), MASK_FROM_PARTICLES_THREAD_COUNT);
	}

	/** Default constructor. */
	FNvFlowMaskFromParticlesCS()
	{
	}

	/** Initialization constructor. */
	explicit FNvFlowMaskFromParticlesCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		InParticleIndices.Bind(Initializer.ParameterMap, TEXT("InParticleIndices"));
		PositionTexture.Bind(Initializer.ParameterMap, TEXT("PositionTexture"));
		PositionTextureSampler.Bind(Initializer.ParameterMap, TEXT("PositionTextureSampler"));
		OutMask.Bind(Initializer.ParameterMap, TEXT("OutMask"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InParticleIndices;
		Ar << PositionTexture;
		Ar << PositionTextureSampler;
		Ar << OutMask;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(IRHICommandContext* RHICmdCtx, FUnorderedAccessViewRHIParamRef OutMaskUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (OutMask.IsBound())
		{
			RHICmdCtx->RHISetUAVParameter(ComputeShaderRHI, OutMask.GetBaseIndex(), OutMaskUAV);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		IRHICommandContext* RHICmdCtx,
		FNvFlowMaskFromParticlesUniformBufferRef& UniformBuffer,
		FShaderResourceViewRHIParamRef InIndicesSRV,
		FTexture2DRHIParamRef PositionTextureRHI
	)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		//SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FNvFlowMaskFromParticlesParameters>(), UniformBuffer);
		{
			auto Shader = ComputeShaderRHI;
			const auto& Parameter = GetUniformBufferParameter<FNvFlowMaskFromParticlesParameters>();
			auto UniformBufferRHI = UniformBuffer;
			if (Parameter.IsBound())
			{
				RHICmdCtx->RHISetShaderUniformBuffer(Shader, Parameter.GetBaseIndex(), UniformBufferRHI);
			}
		}
		if (InParticleIndices.IsBound())
		{
			RHICmdCtx->RHISetShaderResourceViewParameter(ComputeShaderRHI, InParticleIndices.GetBaseIndex(), InIndicesSRV);
		}
		if (PositionTexture.IsBound())
		{
			RHICmdCtx->RHISetShaderTexture(ComputeShaderRHI, PositionTexture.GetBaseIndex(), PositionTextureRHI);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(IRHICommandContext* RHICmdCtx)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (InParticleIndices.IsBound())
		{
			RHICmdCtx->RHISetShaderResourceViewParameter(ComputeShaderRHI, InParticleIndices.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (OutMask.IsBound())
		{
			RHICmdCtx->RHISetUAVParameter(ComputeShaderRHI, OutMask.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:

	/** Input buffer containing particle indices. */
	FShaderResourceParameter InParticleIndices;
	/** Texture containing particle positions. */
	FShaderResourceParameter PositionTexture;
	FShaderResourceParameter PositionTextureSampler;
	/** Output key buffer. */
	FShaderResourceParameter OutMask;
};
IMPLEMENT_SHADER_TYPE(, FNvFlowMaskFromParticlesCS, TEXT("/Plugin/NvFlow/Private/NvFlowAllocShader.usf"), TEXT("ComputeMaskFromParticles"), SF_Compute);


void NvFlow::Scene::emitCustomAllocCallback(IRHICommandContext* RHICmdCtx, const NvFlowGridEmitCustomAllocParams* params, const class FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData)
{
	if (m_particleParamsArray.Num() == 0)
	{
		return;
	}
	m_context->m_flowInterop->Pop(*RHICmdCtx, m_renderContext);

	TShaderMapRef<FNvFlowMaskFromParticlesCS> MaskFromParticlesCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	RHICmdCtx->RHISetComputeShader(MaskFromParticlesCS->GetComputeShader());

	FNvFlowMaskFromParticlesParameters MaskFromParticlesParameters;
	MaskFromParticlesParameters.WorldToVolume = NvFlowGetWorldToVolume(FlowGridSceneProxy);
	MaskFromParticlesParameters.MaskDim = FIntVector(params->maskDim.x, params->maskDim.y, params->maskDim.z);

	FUnorderedAccessViewRHIRef MaskUAV;
	FRHINvFlowResourceRW* pMaskResourceRW = m_context->m_flowInterop->CreateResourceRW(
		*RHICmdCtx, m_renderContext, params->maskResourceRW, nullptr, &MaskUAV);

	for (int32 i = 0; i < m_particleParamsArray.Num(); ++i)
	{
		const ParticleSimulationParamsNvFlow& ParticleParams = m_particleParamsArray[i];
		if (ParticleParams.ParticleCount > 0)
		{
			MaskFromParticlesParameters.ParticleCount = ParticleParams.ParticleCount;
			MaskFromParticlesParameters.TextureSizeX = ParticleParams.TextureSizeX;
			MaskFromParticlesParameters.TextureSizeY = ParticleParams.TextureSizeY;
			FNvFlowMaskFromParticlesUniformBufferRef UniformBuffer = FNvFlowMaskFromParticlesUniformBufferRef::CreateUniformBufferImmediate(MaskFromParticlesParameters, UniformBuffer_SingleFrame);

			uint32 GroupCount = (ParticleParams.ParticleCount + MASK_FROM_PARTICLES_THREAD_COUNT - 1) / MASK_FROM_PARTICLES_THREAD_COUNT;

			MaskFromParticlesCS->SetOutput(RHICmdCtx, MaskUAV);
			MaskFromParticlesCS->SetParameters(RHICmdCtx, UniformBuffer, ParticleParams.VertexBufferSRV, ParticleParams.PositionTextureRHI);
			//DispatchComputeShader(RHICmdCtx, *MaskFromParticlesCS, GroupCount, 1, 1);
			RHICmdCtx->RHIDispatchComputeShader(GroupCount, 1, 1);
			MaskFromParticlesCS->UnbindBuffers(RHICmdCtx);
		}
	}

	m_context->m_flowInterop->ReleaseResourceRW(*RHICmdCtx, pMaskResourceRW);

	m_context->m_flowInterop->Push(*RHICmdCtx, m_renderContext);
}


#define COPY_THREAD_COUNT_X 4
#define COPY_THREAD_COUNT_Y 4
#define COPY_THREAD_COUNT_Z 4

BEGIN_UNIFORM_BUFFER_STRUCT(FNvFlowCopyGridDataParameters, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FIntVector, ThreadDim)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FIntVector, BlockDim)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FIntVector, BlockDimBits)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int32, IsVTR)
END_UNIFORM_BUFFER_STRUCT(FNvFlowCopyGridDataParameters)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FNvFlowCopyGridDataParameters, TEXT("NvFlowCopyGridData"));
typedef TUniformBufferRef<FNvFlowCopyGridDataParameters> FNvFlowCopyGridDataUniformBufferRef;

BEGIN_UNIFORM_BUFFER_STRUCT(FNvFlowApplyDistanceFieldParameters, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FIntVector, ThreadDim)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FIntVector, BlockDim)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FIntVector, BlockDimBits)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int32, IsVTR)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector, VDimInv)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FMatrix, VolumeToWorld)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, DistanceScale)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, MinActiveDist)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, MaxActiveDist)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, ValueCoupleRate)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, EmitValue)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, SlipFactor)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, SlipThickness)
END_UNIFORM_BUFFER_STRUCT(FNvFlowApplyDistanceFieldParameters)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FNvFlowApplyDistanceFieldParameters, TEXT("NvFlowApplyDistanceField"));
typedef TUniformBufferRef<FNvFlowApplyDistanceFieldParameters> FNvFlowApplyDistanceFieldUniformBufferRef;


class FNvFlowCopyGridDataCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FNvFlowCopyGridDataCS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_COUNT_X"), COPY_THREAD_COUNT_X);
		OutEnvironment.SetDefine(TEXT("THREAD_COUNT_Y"), COPY_THREAD_COUNT_Y);
		OutEnvironment.SetDefine(TEXT("THREAD_COUNT_Z"), COPY_THREAD_COUNT_Z);
	}

	/** Default constructor. */
	FNvFlowCopyGridDataCS()
	{
	}

	/** Initialization constructor. */
	explicit FNvFlowCopyGridDataCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		BlockList.Bind(Initializer.ParameterMap, TEXT("BlockList"));
		BlockTable.Bind(Initializer.ParameterMap, TEXT("BlockTable"));
		DataIn.Bind(Initializer.ParameterMap, TEXT("DataIn"));
		DataOut.Bind(Initializer.ParameterMap, TEXT("DataOut"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << BlockList;
		Ar << BlockTable;
		Ar << DataIn;
		Ar << DataOut;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(IRHICommandContext* RHICmdCtx, FUnorderedAccessViewRHIParamRef DataOutUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (DataOut.IsBound())
		{
			RHICmdCtx->RHISetUAVParameter(ComputeShaderRHI, DataOut.GetBaseIndex(), DataOutUAV);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		IRHICommandContext* RHICmdCtx,
		TUniformBufferRef<FNvFlowCopyGridDataParameters>& UniformBuffer,
		FShaderResourceViewRHIParamRef BlockListSRV,
		FShaderResourceViewRHIParamRef BlockTableSRV,
		FShaderResourceViewRHIParamRef DataInSRV
	)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		//SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FNvFlowCopyGridDataParameters>(), UniformBuffer);
		{
			auto Shader = ComputeShaderRHI;
			const auto& Parameter = GetUniformBufferParameter<FNvFlowCopyGridDataParameters>();
			auto UniformBufferRHI = UniformBuffer;
			if (Parameter.IsBound())
			{
				RHICmdCtx->RHISetShaderUniformBuffer(Shader, Parameter.GetBaseIndex(), UniformBufferRHI);
			}
		}
		if (BlockList.IsBound())
		{
			RHICmdCtx->RHISetShaderResourceViewParameter(ComputeShaderRHI, BlockList.GetBaseIndex(), BlockListSRV);
		}
		if (BlockTable.IsBound())
		{
			RHICmdCtx->RHISetShaderResourceViewParameter(ComputeShaderRHI, BlockTable.GetBaseIndex(), BlockTableSRV);
		}
		if (DataIn.IsBound())
		{
			RHICmdCtx->RHISetShaderResourceViewParameter(ComputeShaderRHI, DataIn.GetBaseIndex(), DataInSRV);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(IRHICommandContext* RHICmdCtx)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (DataOut.IsBound())
		{
			RHICmdCtx->RHISetUAVParameter(ComputeShaderRHI, DataOut.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter BlockList;
	FShaderResourceParameter BlockTable;
	FShaderResourceParameter DataIn;
	FShaderResourceParameter DataOut;
};
IMPLEMENT_SHADER_TYPE(, FNvFlowCopyGridDataCS, TEXT("/Plugin/NvFlow/Private/NvFlowCopyShader.usf"), TEXT("CopyGridData"), SF_Compute);


class FNvFlowApplyDistanceFieldCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FNvFlowApplyDistanceFieldCS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_COUNT_X"), COPY_THREAD_COUNT_X);
		OutEnvironment.SetDefine(TEXT("THREAD_COUNT_Y"), COPY_THREAD_COUNT_Y);
		OutEnvironment.SetDefine(TEXT("THREAD_COUNT_Z"), COPY_THREAD_COUNT_Z);
	}

	/** Default constructor. */
	FNvFlowApplyDistanceFieldCS()
	{
	}

	/** Initialization constructor. */
	explicit FNvFlowApplyDistanceFieldCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		BlockList.Bind(Initializer.ParameterMap, TEXT("BlockList"));
		BlockTable.Bind(Initializer.ParameterMap, TEXT("BlockTable"));
		DataIn.Bind(Initializer.ParameterMap, TEXT("DataIn"));
		DataOut.Bind(Initializer.ParameterMap, TEXT("DataOut"));

		GlobalDistanceFieldParameters.Bind(Initializer.ParameterMap);
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << BlockList;
		Ar << BlockTable;
		Ar << DataIn;
		Ar << DataOut;
		Ar << GlobalDistanceFieldParameters;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(IRHICommandContext* RHICmdCtx, FUnorderedAccessViewRHIParamRef DataOutUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (DataOut.IsBound())
		{
			RHICmdCtx->RHISetUAVParameter(ComputeShaderRHI, DataOut.GetBaseIndex(), DataOutUAV);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		IRHICommandContext* RHICmdCtx,
		TUniformBufferRef<FNvFlowApplyDistanceFieldParameters>& UniformBuffer,
		FShaderResourceViewRHIParamRef BlockListSRV,
		FShaderResourceViewRHIParamRef BlockTableSRV,
		FShaderResourceViewRHIParamRef DataInSRV,
		const class FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData
	)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		//SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FNvFlowApplyDistanceFieldParameters>(), UniformBuffer);
		{
			auto Shader = ComputeShaderRHI;
			const auto& Parameter = GetUniformBufferParameter<FNvFlowApplyDistanceFieldParameters>();
			auto UniformBufferRHI = UniformBuffer;
			if (Parameter.IsBound())
			{
				RHICmdCtx->RHISetShaderUniformBuffer(Shader, Parameter.GetBaseIndex(), UniformBufferRHI);
			}
		}
		if (BlockList.IsBound())
		{
			RHICmdCtx->RHISetShaderResourceViewParameter(ComputeShaderRHI, BlockList.GetBaseIndex(), BlockListSRV);
		}
		if (BlockTable.IsBound())
		{
			RHICmdCtx->RHISetShaderResourceViewParameter(ComputeShaderRHI, BlockTable.GetBaseIndex(), BlockTableSRV);
		}
		if (DataIn.IsBound())
		{
			RHICmdCtx->RHISetShaderResourceViewParameter(ComputeShaderRHI, DataIn.GetBaseIndex(), DataInSRV);
		}
		if (GlobalDistanceFieldParameterData != nullptr)
		{
			GlobalDistanceFieldParameters.Set(RHICmdCtx, ComputeShaderRHI, *GlobalDistanceFieldParameterData);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(IRHICommandContext* RHICmdCtx)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (DataOut.IsBound())
		{
			RHICmdCtx->RHISetUAVParameter(ComputeShaderRHI, DataOut.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter BlockList;
	FShaderResourceParameter BlockTable;
	FShaderResourceParameter DataIn;
	FShaderResourceParameter DataOut;

	FGlobalDistanceFieldParameters GlobalDistanceFieldParameters;
};
IMPLEMENT_SHADER_TYPE(, FNvFlowApplyDistanceFieldCS, TEXT("/Plugin/NvFlow/Private/NvFlowDistanceFieldShader.usf"), TEXT("ApplyDistanceField"), SF_Compute);


#define COUPLE_PARTICLES_THREAD_COUNT 64

BEGIN_UNIFORM_BUFFER_STRUCT(FNvFlowCoupleParticlesParameters, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, TextureSizeX)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, TextureSizeY)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, ParticleCount)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FMatrix, WorldToVolume)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FIntVector, VDim)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FIntVector, BlockDim)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FIntVector, BlockDimBits)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int32, IsVTR)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, AccelRate)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, DecelRate)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, Threshold)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, InvVelocityScale)
END_UNIFORM_BUFFER_STRUCT(FNvFlowCoupleParticlesParameters)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FNvFlowCoupleParticlesParameters, TEXT("NvFlowCoupleParticles"));
typedef TUniformBufferRef<FNvFlowCoupleParticlesParameters> FNvFlowCoupleParticlesUniformBufferRef;

class FNvFlowCoupleParticlesCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FNvFlowCoupleParticlesCS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_COUNT"), COUPLE_PARTICLES_THREAD_COUNT);
	}

	/** Default constructor. */
	FNvFlowCoupleParticlesCS()
	{
	}

	/** Initialization constructor. */
	explicit FNvFlowCoupleParticlesCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		InParticleIndices.Bind(Initializer.ParameterMap, TEXT("InParticleIndices"));
		PositionTexture.Bind(Initializer.ParameterMap, TEXT("PositionTexture"));
		VelocityTexture.Bind(Initializer.ParameterMap, TEXT("VelocityTexture"));
		BlockTable.Bind(Initializer.ParameterMap, TEXT("BlockTable"));
		DataIn.Bind(Initializer.ParameterMap, TEXT("DataIn"));
		DataOut.Bind(Initializer.ParameterMap, TEXT("DataOut"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InParticleIndices;
		Ar << PositionTexture;
		Ar << VelocityTexture;
		Ar << BlockTable;
		Ar << DataIn;
		Ar << DataOut;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(IRHICommandContext* RHICmdCtx, FUnorderedAccessViewRHIParamRef DataOutUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (DataOut.IsBound())
		{
			RHICmdCtx->RHISetUAVParameter(ComputeShaderRHI, DataOut.GetBaseIndex(), DataOutUAV);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		IRHICommandContext* RHICmdCtx,
		FNvFlowCoupleParticlesUniformBufferRef& UniformBuffer,
		FShaderResourceViewRHIParamRef InIndicesSRV,
		FTexture2DRHIParamRef PositionTextureRHI,
		FTexture2DRHIParamRef VelocityTextureRHI,
		FShaderResourceViewRHIParamRef BlockTableSRV,
		FShaderResourceViewRHIParamRef DataInSRV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		//SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FNvFlowCoupleParticlesParameters>(), UniformBuffer);
		{
			auto Shader = ComputeShaderRHI;
			const auto& Parameter = GetUniformBufferParameter<FNvFlowCoupleParticlesParameters>();
			auto UniformBufferRHI = UniformBuffer;
			if (Parameter.IsBound())
			{
				RHICmdCtx->RHISetShaderUniformBuffer(Shader, Parameter.GetBaseIndex(), UniformBufferRHI);
			}
		}
		if (InParticleIndices.IsBound())
		{
			RHICmdCtx->RHISetShaderResourceViewParameter(ComputeShaderRHI, InParticleIndices.GetBaseIndex(), InIndicesSRV);
		}
		if (PositionTexture.IsBound())
		{
			RHICmdCtx->RHISetShaderTexture(ComputeShaderRHI, PositionTexture.GetBaseIndex(), PositionTextureRHI);
		}
		if (VelocityTexture.IsBound())
		{
			RHICmdCtx->RHISetShaderTexture(ComputeShaderRHI, VelocityTexture.GetBaseIndex(), VelocityTextureRHI);
		}
		if (BlockTable.IsBound())
		{
			RHICmdCtx->RHISetShaderResourceViewParameter(ComputeShaderRHI, BlockTable.GetBaseIndex(), BlockTableSRV);
		}
		if (DataIn.IsBound())
		{
			RHICmdCtx->RHISetShaderResourceViewParameter(ComputeShaderRHI, DataIn.GetBaseIndex(), DataInSRV);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(IRHICommandContext* RHICmdCtx)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (InParticleIndices.IsBound())
		{
			RHICmdCtx->RHISetShaderResourceViewParameter(ComputeShaderRHI, InParticleIndices.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (DataOut.IsBound())
		{
			RHICmdCtx->RHISetUAVParameter(ComputeShaderRHI, DataOut.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:

	/** Input buffer containing particle indices. */
	FShaderResourceParameter InParticleIndices;
	/** Texture containing particle positions. */
	FShaderResourceParameter PositionTexture;
	FShaderResourceParameter VelocityTexture;

	FShaderResourceParameter BlockTable;
	FShaderResourceParameter DataIn;
	FShaderResourceParameter DataOut;
};
IMPLEMENT_SHADER_TYPE(, FNvFlowCoupleParticlesCS, TEXT("/Plugin/NvFlow/Private/NvFlowCoupleShader.usf"), TEXT("CoupleParticlesToGrid"), SF_Compute);


void NvFlow::Scene::applyDistanceField(IRHICommandContext* RHICmdCtx, NvFlowUint dataFrontIdx, const NvFlowGridEmitCustomEmitLayerParams& layerParams, const class FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData, float dt,
	FShaderResourceViewRHIRef DataInSRV, FUnorderedAccessViewRHIRef DataOutUAV, FShaderResourceViewRHIRef BlockListSRV, FShaderResourceViewRHIRef BlockTableSRV,
	float InSlipFactor, float InSlipThickness, FVector4 InEmitValue)
{
	if (layerParams.numBlocks == 0)
	{
		return;
	}

	TShaderMapRef<FNvFlowApplyDistanceFieldCS> ApplyDistanceFieldCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	RHICmdCtx->RHISetComputeShader(ApplyDistanceFieldCS->GetComputeShader());

	FIntVector VDim = FIntVector(
		layerParams.shaderParams.blockDim.x * layerParams.shaderParams.gridDim.x,
		layerParams.shaderParams.blockDim.y * layerParams.shaderParams.gridDim.y,
		layerParams.shaderParams.blockDim.z * layerParams.shaderParams.gridDim.z);

	FNvFlowApplyDistanceFieldParameters Parameters;
	Parameters.BlockDim = NvFlowConvert(layerParams.shaderParams.blockDim);
	Parameters.BlockDimBits = NvFlowConvert(layerParams.shaderParams.blockDimBits);
	Parameters.IsVTR = layerParams.shaderParams.isVTR.x;
	Parameters.ThreadDim = Parameters.BlockDim;
	Parameters.ThreadDim.X *= layerParams.numBlocks;
	Parameters.VDimInv = FVector(1.0f / VDim.X, 1.0f / VDim.Y, 1.0f / VDim.Z);
	Parameters.VolumeToWorld = NvFlowGetVolumeToWorld(FlowGridSceneProxy);
	Parameters.MinActiveDist = FlowGridSceneProxy->FlowGridProperties->MinActiveDistance;
	Parameters.MaxActiveDist = FlowGridSceneProxy->FlowGridProperties->MaxActiveDistance;
	Parameters.ValueCoupleRate = 100.0f * dt;
	Parameters.DistanceScale = 1.0f / scale;
	Parameters.EmitValue = InEmitValue;
	Parameters.SlipFactor = InSlipFactor;
	Parameters.SlipThickness = InSlipThickness;

	FNvFlowApplyDistanceFieldUniformBufferRef UniformBuffer = FNvFlowApplyDistanceFieldUniformBufferRef::CreateUniformBufferImmediate(Parameters, UniformBuffer_SingleFrame);

	uint32 GroupCountX = (Parameters.ThreadDim.X + COPY_THREAD_COUNT_X - 1) / COPY_THREAD_COUNT_X;
	uint32 GroupCountY = (Parameters.ThreadDim.Y + COPY_THREAD_COUNT_Y - 1) / COPY_THREAD_COUNT_Y;
	uint32 GroupCountZ = (Parameters.ThreadDim.Z + COPY_THREAD_COUNT_Z - 1) / COPY_THREAD_COUNT_Z;

	ApplyDistanceFieldCS->SetOutput(RHICmdCtx, DataOutUAV);
	ApplyDistanceFieldCS->SetParameters(RHICmdCtx, UniformBuffer, BlockListSRV, BlockTableSRV, DataInSRV, GlobalDistanceFieldParameterData);
	//DispatchComputeShader(RHICmdCtx, *ApplyDistanceFieldCS, GroupCountX, GroupCountY, GroupCountZ);
	RHICmdCtx->RHIDispatchComputeShader(GroupCountX, GroupCountY, GroupCountZ);
	ApplyDistanceFieldCS->UnbindBuffers(RHICmdCtx);
}

void NvFlow::Scene::emitCustomEmitVelocityCallback(IRHICommandContext* RHICmdCtx, NvFlowUint* dataFrontIdx, const NvFlowGridEmitCustomEmitParams* emitParams, const class FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData, float dt)
{
	const bool bHasDistanceFieldCollision = FlowGridSceneProxy->FlowGridProperties->bDistanceFieldCollisionEnabled &&
		(GlobalDistanceFieldParameterData->Textures[0] != nullptr);

	const bool bHasParticles = m_particleParamsArray.Num() > 0;

	if (emitParams->numLayers == 0 || !(bHasDistanceFieldCollision || bHasParticles))
	{
		return;
	}

	m_context->m_flowInterop->Pop(*RHICmdCtx, m_renderContext);

	for (uint32 layerId = 0; layerId < emitParams->numLayers; ++layerId)
	{
		NvFlowGridEmitCustomEmitLayerParams layerParams;
		NvFlowGridEmitCustomGetLayerParams(emitParams, layerId, &layerParams);

		FShaderResourceViewRHIRef Data0SRV, Data1SRV;
		FUnorderedAccessViewRHIRef Data0UAV, Data1UAV;

		FRHINvFlowResourceRW* pData0ResourceRW = m_context->m_flowInterop->CreateResourceRW(
			*RHICmdCtx, m_renderContext, layerParams.dataRW[*dataFrontIdx], &Data0SRV, &Data0UAV);

		FRHINvFlowResourceRW* pData1ResourceRW = m_context->m_flowInterop->CreateResourceRW(
			*RHICmdCtx, m_renderContext, layerParams.dataRW[*dataFrontIdx ^ 1], &Data1SRV, &Data1UAV);

		FShaderResourceViewRHIRef BlockListSRV = m_context->m_flowInterop->CreateSRV(*RHICmdCtx, m_renderContext, layerParams.blockList);
		FShaderResourceViewRHIRef BlockTableSRV = m_context->m_flowInterop->CreateSRV(*RHICmdCtx, m_renderContext, layerParams.blockTable);

		if (bHasParticles)
		{
			{
				TShaderMapRef<FNvFlowCopyGridDataCS> CopyGridDataCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
				RHICmdCtx->RHISetComputeShader(CopyGridDataCS->GetComputeShader());

				FNvFlowCopyGridDataParameters CopyGridDataParameters;
				CopyGridDataParameters.BlockDim = NvFlowConvert(layerParams.shaderParams.blockDim);
				CopyGridDataParameters.BlockDimBits = NvFlowConvert(layerParams.shaderParams.blockDimBits);
				CopyGridDataParameters.IsVTR = layerParams.shaderParams.isVTR.x;
				CopyGridDataParameters.ThreadDim = CopyGridDataParameters.BlockDim;
				CopyGridDataParameters.ThreadDim.X *= layerParams.numBlocks;
				FNvFlowCopyGridDataUniformBufferRef UniformBuffer = FNvFlowCopyGridDataUniformBufferRef::CreateUniformBufferImmediate(CopyGridDataParameters, UniformBuffer_SingleFrame);

				uint32 GroupCountX = (CopyGridDataParameters.ThreadDim.X + COPY_THREAD_COUNT_X - 1) / COPY_THREAD_COUNT_X;
				uint32 GroupCountY = (CopyGridDataParameters.ThreadDim.Y + COPY_THREAD_COUNT_Y - 1) / COPY_THREAD_COUNT_Y;
				uint32 GroupCountZ = (CopyGridDataParameters.ThreadDim.Z + COPY_THREAD_COUNT_Z - 1) / COPY_THREAD_COUNT_Z;

				FShaderResourceViewRHIRef DataInSRV = Data0SRV;
				FUnorderedAccessViewRHIRef DataOutUAV = Data1UAV;

				CopyGridDataCS->SetOutput(RHICmdCtx, DataOutUAV);
				CopyGridDataCS->SetParameters(RHICmdCtx, UniformBuffer, BlockListSRV, BlockTableSRV, DataInSRV);
				//DispatchComputeShader(RHICmdCtx, *CopyGridDataCS, GroupCountX, GroupCountY, GroupCountZ);
				RHICmdCtx->RHIDispatchComputeShader(GroupCountX, GroupCountY, GroupCountZ);
				CopyGridDataCS->UnbindBuffers(RHICmdCtx);
			}

			TShaderMapRef<FNvFlowCoupleParticlesCS> CoupleParticlesCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			RHICmdCtx->RHISetComputeShader(CoupleParticlesCS->GetComputeShader());

			FNvFlowCoupleParticlesParameters CoupleParticlesParameters;
			CoupleParticlesParameters.WorldToVolume = NvFlowGetWorldToVolume(FlowGridSceneProxy);
			CoupleParticlesParameters.VDim = FIntVector(
				layerParams.shaderParams.blockDim.x * layerParams.shaderParams.gridDim.x,
				layerParams.shaderParams.blockDim.y * layerParams.shaderParams.gridDim.y,
				layerParams.shaderParams.blockDim.z * layerParams.shaderParams.gridDim.z);
			CoupleParticlesParameters.BlockDim = NvFlowConvert(layerParams.shaderParams.blockDim);
			CoupleParticlesParameters.BlockDimBits = NvFlowConvert(layerParams.shaderParams.blockDimBits);
			CoupleParticlesParameters.IsVTR = layerParams.shaderParams.isVTR.x;

			CoupleParticlesParameters.AccelRate = dt / FlowGridSceneProxy->FlowGridProperties->ParticleToGridAccelTimeConstant;
			CoupleParticlesParameters.DecelRate = dt / FlowGridSceneProxy->FlowGridProperties->ParticleToGridDecelTimeConstant;
			CoupleParticlesParameters.Threshold = FlowGridSceneProxy->FlowGridProperties->ParticleToGridThresholdMultiplier;

			CoupleParticlesParameters.InvVelocityScale = 1.0f / scale;

			FShaderResourceViewRHIRef DataInSRV = Data1SRV;
			FUnorderedAccessViewRHIRef DataOutUAV = Data0UAV;

			for (int32 i = 0; i < m_particleParamsArray.Num(); ++i)
			{
				const ParticleSimulationParamsNvFlow& ParticleParams = m_particleParamsArray[i];
				if (ParticleParams.ParticleCount > 0)
				{
					CoupleParticlesParameters.ParticleCount = ParticleParams.ParticleCount;
					CoupleParticlesParameters.TextureSizeX = ParticleParams.TextureSizeX;
					CoupleParticlesParameters.TextureSizeY = ParticleParams.TextureSizeY;
					FNvFlowCoupleParticlesUniformBufferRef UniformBuffer = FNvFlowCoupleParticlesUniformBufferRef::CreateUniformBufferImmediate(CoupleParticlesParameters, UniformBuffer_SingleFrame);

					uint32 GroupCount = (ParticleParams.ParticleCount + COUPLE_PARTICLES_THREAD_COUNT - 1) / COUPLE_PARTICLES_THREAD_COUNT;

					CoupleParticlesCS->SetOutput(RHICmdCtx, DataOutUAV);
					CoupleParticlesCS->SetParameters(RHICmdCtx, UniformBuffer, ParticleParams.VertexBufferSRV, ParticleParams.PositionTextureRHI, ParticleParams.VelocityTextureRHI, BlockTableSRV, DataInSRV);
					//DispatchComputeShader(RHICmdCtx, *CoupleParticlesCS, GroupCount, 1, 1);
					RHICmdCtx->RHIDispatchComputeShader(GroupCount, 1, 1);
					CoupleParticlesCS->UnbindBuffers(RHICmdCtx);
				}
			}
		}

		if (bHasDistanceFieldCollision)
		{
			applyDistanceField(RHICmdCtx, *dataFrontIdx, layerParams, GlobalDistanceFieldParameterData, dt,
				Data0SRV, Data1UAV, BlockListSRV, BlockTableSRV,
				FlowGridSceneProxy->FlowGridProperties->VelocitySlipFactor, FlowGridSceneProxy->FlowGridProperties->VelocitySlipThickness);
		}

		m_context->m_flowInterop->ReleaseResourceRW(*RHICmdCtx, pData1ResourceRW);
		m_context->m_flowInterop->ReleaseResourceRW(*RHICmdCtx, pData0ResourceRW);
	}

	if (bHasDistanceFieldCollision)
	{
		*dataFrontIdx ^= 1;
	}

	m_context->m_flowInterop->Push(*RHICmdCtx, m_renderContext);
}

void NvFlow::Scene::emitCustomEmitDensityCallback(IRHICommandContext* RHICmdCtx, NvFlowUint* dataFrontIdx, const NvFlowGridEmitCustomEmitParams* emitParams, const class FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData, float dt)
{
	bool bHasDistanceFieldCollision = FlowGridSceneProxy->FlowGridProperties->bDistanceFieldCollisionEnabled &&
		(GlobalDistanceFieldParameterData->Textures[0] != nullptr);

	if (emitParams->numLayers == 0 || !bHasDistanceFieldCollision)
	{
		return;
	}

	m_context->m_flowInterop->Pop(*RHICmdCtx, m_renderContext);

	for (uint32 layerId = 0; layerId < emitParams->numLayers; ++layerId)
	{
		NvFlowGridEmitCustomEmitLayerParams layerParams;
		NvFlowGridEmitCustomGetLayerParams(emitParams, layerId, &layerParams);

		FShaderResourceViewRHIRef Data0SRV;
		FUnorderedAccessViewRHIRef Data1UAV;

		FRHINvFlowResourceRW* pData0ResourceRW = m_context->m_flowInterop->CreateResourceRW(
			*RHICmdCtx, m_renderContext, layerParams.dataRW[*dataFrontIdx], &Data0SRV, nullptr);

		FRHINvFlowResourceRW* pData1ResourceRW = m_context->m_flowInterop->CreateResourceRW(
			*RHICmdCtx, m_renderContext, layerParams.dataRW[*dataFrontIdx ^ 1], nullptr, &Data1UAV);

		FShaderResourceViewRHIRef BlockListSRV = m_context->m_flowInterop->CreateSRV(*RHICmdCtx, m_renderContext, layerParams.blockList);
		FShaderResourceViewRHIRef BlockTableSRV = m_context->m_flowInterop->CreateSRV(*RHICmdCtx, m_renderContext, layerParams.blockTable);

		applyDistanceField(RHICmdCtx, *dataFrontIdx, layerParams, GlobalDistanceFieldParameterData, dt,
			Data0SRV, Data1UAV, BlockListSRV, BlockTableSRV);

		m_context->m_flowInterop->ReleaseResourceRW(*RHICmdCtx, pData1ResourceRW);
		m_context->m_flowInterop->ReleaseResourceRW(*RHICmdCtx, pData0ResourceRW);
	}

	*dataFrontIdx ^= 1;

	m_context->m_flowInterop->Push(*RHICmdCtx, m_renderContext);
}

#endif // WITH_NVFLOW_BACKEND

#endif
// NvFlow end
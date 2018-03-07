// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12Shaders.cpp: D3D shader RHI implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"

template <typename TShaderType>
static inline void ReadShaderOptionalData(FShaderCodeReader& InShaderCode, TShaderType& OutShader)
{
	auto PackedResourceCounts = InShaderCode.FindOptionalData<FShaderCodePackedResourceCounts>();
	check(PackedResourceCounts);
	OutShader.ResourceCounts = *PackedResourceCounts;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	OutShader.ShaderName = InShaderCode.FindOptionalData('n');

	int32 UniformBufferTableSize = 0;
	auto* UniformBufferData = InShaderCode.FindOptionalDataAndSize('u', UniformBufferTableSize);
#if 0
	//#todo-rco
	if (UniformBufferData && UniformBufferTableSize > 0)
	{
		FBufferReader UBReader((void*)UniformBufferData, UniformBufferTableSize, false);
		TArray<FString> Names;
		UBReader << Names;
		check(OutShader.UniformBuffers.Num() == 0);
		for (int32 Index = 0; Index < Names.Num(); ++Index)
		{
			OutShader.UniformBuffers.Add(FName(*Names[Index]));
		}
	}
#endif
#endif
}

FVertexShaderRHIRef FD3D12DynamicRHI::RHICreateVertexShader(const TArray<uint8>& Code)
{
	FShaderCodeReader ShaderCode(Code);
	FD3D12VertexShader* Shader = new FD3D12VertexShader;

	FMemoryReader Ar(Code, true);
	Ar << Shader->ShaderResourceTable;
	int32 Offset = Ar.Tell();
	const uint8* CodePtr = Code.GetData() + Offset;
	const SIZE_T CodeSize = ShaderCode.GetActualShaderCodeSize() - Offset;

	ReadShaderOptionalData(ShaderCode, *Shader);

	Shader->Code = Code;
	Shader->Offset = Offset;

	D3D12_SHADER_BYTECODE ShaderBytecode;
	ShaderBytecode.pShaderBytecode = Shader->Code.GetData() + Offset;
	ShaderBytecode.BytecodeLength = CodeSize;
	Shader->ShaderBytecode.SetShaderBytecode(ShaderBytecode);

	// NVCHANGE_BEGIN: Add VXGI
	Shader->NvidiaShaderExtensions = NvidiaShaderExtensions;
	NvidiaShaderExtensions.Reset();
	// NVCHANGE_END: Add VXGI

	return Shader;
}

FPixelShaderRHIRef FD3D12DynamicRHI::RHICreatePixelShader(const TArray<uint8>& Code)
{
	FShaderCodeReader ShaderCode(Code);

	FD3D12PixelShader* Shader = new FD3D12PixelShader;

	FMemoryReader Ar(Code, true);
	Ar << Shader->ShaderResourceTable;
	int32 Offset = Ar.Tell();
	const uint8* CodePtr = Code.GetData() + Offset;
	const SIZE_T CodeSize = ShaderCode.GetActualShaderCodeSize() - Offset;

	ReadShaderOptionalData(ShaderCode, *Shader);

	Shader->Code = Code;

	D3D12_SHADER_BYTECODE ShaderBytecode;
	ShaderBytecode.pShaderBytecode = Shader->Code.GetData() + Offset;
	ShaderBytecode.BytecodeLength = CodeSize;
	Shader->ShaderBytecode.SetShaderBytecode(ShaderBytecode);

	// NVCHANGE_BEGIN: Add VXGI
	Shader->NvidiaShaderExtensions = NvidiaShaderExtensions;
	NvidiaShaderExtensions.Reset();
	// NVCHANGE_END: Add VXGI

	return Shader;
}

FHullShaderRHIRef FD3D12DynamicRHI::RHICreateHullShader(const TArray<uint8>& Code)
{
	FShaderCodeReader ShaderCode(Code);

	FD3D12HullShader* Shader = new FD3D12HullShader;

	FMemoryReader Ar(Code, true);
	Ar << Shader->ShaderResourceTable;
	int32 Offset = Ar.Tell();
	const uint8* CodePtr = Code.GetData() + Offset;
	const SIZE_T CodeSize = ShaderCode.GetActualShaderCodeSize() - Offset;

	ReadShaderOptionalData(ShaderCode, *Shader);

	Shader->Code = Code;

	D3D12_SHADER_BYTECODE ShaderBytecode;
	ShaderBytecode.pShaderBytecode = Shader->Code.GetData() + Offset;
	ShaderBytecode.BytecodeLength = CodeSize;
	Shader->ShaderBytecode.SetShaderBytecode(ShaderBytecode);

	// NVCHANGE_BEGIN: Add VXGI
	Shader->NvidiaShaderExtensions = NvidiaShaderExtensions;
	NvidiaShaderExtensions.Reset();
	// NVCHANGE_END: Add VXGI

	return Shader;
}

FDomainShaderRHIRef FD3D12DynamicRHI::RHICreateDomainShader(const TArray<uint8>& Code)
{
	FShaderCodeReader ShaderCode(Code);

	FD3D12DomainShader* Shader = new FD3D12DomainShader;

	FMemoryReader Ar(Code, true);
	Ar << Shader->ShaderResourceTable;
	int32 Offset = Ar.Tell();
	const uint8* CodePtr = Code.GetData() + Offset;
	const SIZE_T CodeSize = ShaderCode.GetActualShaderCodeSize() - Offset;

	ReadShaderOptionalData(ShaderCode, *Shader);

	Shader->Code = Code;

	D3D12_SHADER_BYTECODE ShaderBytecode;
	ShaderBytecode.pShaderBytecode = Shader->Code.GetData() + Offset;
	ShaderBytecode.BytecodeLength = CodeSize;
	Shader->ShaderBytecode.SetShaderBytecode(ShaderBytecode);

	// NVCHANGE_BEGIN: Add VXGI
	Shader->NvidiaShaderExtensions = NvidiaShaderExtensions;
	NvidiaShaderExtensions.Reset();
	// NVCHANGE_END: Add VXGI

	return Shader;
}

FGeometryShaderRHIRef FD3D12DynamicRHI::RHICreateGeometryShader(const TArray<uint8>& Code)
{
	FShaderCodeReader ShaderCode(Code);

	FD3D12GeometryShader* Shader = new FD3D12GeometryShader;

	FMemoryReader Ar(Code, true);
	Ar << Shader->ShaderResourceTable;
	int32 Offset = Ar.Tell();
	const uint8* CodePtr = Code.GetData() + Offset;
	const SIZE_T CodeSize = ShaderCode.GetActualShaderCodeSize() - Offset;

	ReadShaderOptionalData(ShaderCode, *Shader);

	Shader->Code = Code;

	D3D12_SHADER_BYTECODE ShaderBytecode;
	ShaderBytecode.pShaderBytecode = Shader->Code.GetData() + Offset;
	ShaderBytecode.BytecodeLength = CodeSize;
	Shader->ShaderBytecode.SetShaderBytecode(ShaderBytecode);

	// NVCHANGE_BEGIN: Add VXGI
	Shader->NvidiaShaderExtensions = NvidiaShaderExtensions;
	NvidiaShaderExtensions.Reset();
	// NVCHANGE_END: Add VXGI

	return Shader;
}

FGeometryShaderRHIRef FD3D12DynamicRHI::RHICreateGeometryShaderWithStreamOutput(const TArray<uint8>& Code, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream)
{
	FShaderCodeReader ShaderCode(Code);

	FD3D12GeometryShader* Shader = new FD3D12GeometryShader;

	FMemoryReader Ar(Code, true);
	Ar << Shader->ShaderResourceTable;
	int32 Offset = Ar.Tell();
	const uint8* CodePtr = Code.GetData() + Offset;
	const SIZE_T CodeSize = ShaderCode.GetActualShaderCodeSize() - Offset;

	Shader->StreamOutput.RasterizedStream = RasterizedStream;
	if (RasterizedStream == -1)
	{
		Shader->StreamOutput.RasterizedStream = D3D12_SO_NO_RASTERIZED_STREAM;
	}

	int32 NumEntries = ElementList.Num();
	Shader->StreamOutput.NumEntries = NumEntries;
	Shader->pStreamOutEntries = new D3D12_SO_DECLARATION_ENTRY[NumEntries];
	Shader->StreamOutput.pSODeclaration = Shader->pStreamOutEntries;
	if (Shader->pStreamOutEntries == nullptr)
	{
		return nullptr;	// Out of memory
	}
	for (int32 EntryIndex = 0; EntryIndex < NumEntries; EntryIndex++)
	{
		Shader->pStreamOutEntries[EntryIndex].Stream = ElementList[EntryIndex].Stream;
		Shader->pStreamOutEntries[EntryIndex].SemanticName = ElementList[EntryIndex].SemanticName;
		Shader->pStreamOutEntries[EntryIndex].SemanticIndex = ElementList[EntryIndex].SemanticIndex;
		Shader->pStreamOutEntries[EntryIndex].StartComponent = ElementList[EntryIndex].StartComponent;
		Shader->pStreamOutEntries[EntryIndex].ComponentCount = ElementList[EntryIndex].ComponentCount;
		Shader->pStreamOutEntries[EntryIndex].OutputSlot = ElementList[EntryIndex].OutputSlot;
	}

	ReadShaderOptionalData(ShaderCode, *Shader);

	// Indicate this shader uses stream output
	Shader->bShaderNeedsStreamOutput = true;

	// Copy the strides
	Shader->StreamOutput.NumStrides = NumStrides;
	Shader->pStreamOutStrides = new uint32[NumStrides];
	Shader->StreamOutput.pBufferStrides = Shader->pStreamOutStrides;
	if (Shader->pStreamOutStrides == nullptr)
	{
		return nullptr;	// Out of memory
	}
	FMemory::Memcpy(Shader->pStreamOutStrides, Strides, sizeof(uint32) * NumStrides);

	Shader->Code = Code;

	D3D12_SHADER_BYTECODE ShaderBytecode;
	ShaderBytecode.pShaderBytecode = Shader->Code.GetData() + Offset;
	ShaderBytecode.BytecodeLength = CodeSize;
	Shader->ShaderBytecode.SetShaderBytecode(ShaderBytecode);

	// NVCHANGE_BEGIN: Add VXGI
	Shader->NvidiaShaderExtensions = NvidiaShaderExtensions;
	NvidiaShaderExtensions.Reset();
	// NVCHANGE_END: Add VXGI

	return Shader;
}

FComputeShaderRHIRef FD3D12DynamicRHI::RHICreateComputeShader(const TArray<uint8>& Code)
{
	FShaderCodeReader ShaderCode(Code);

	FD3D12ComputeShader* Shader = new FD3D12ComputeShader;

	FMemoryReader Ar(Code, true);
	Ar << Shader->ShaderResourceTable;
	int32 Offset = Ar.Tell();
	const uint8* CodePtr = Code.GetData() + Offset;
	const SIZE_T CodeSize = ShaderCode.GetActualShaderCodeSize() - Offset;

	ReadShaderOptionalData(ShaderCode, *Shader);

	Shader->Code = Code;

	D3D12_SHADER_BYTECODE ShaderBytecode;
	ShaderBytecode.pShaderBytecode = Shader->Code.GetData() + Offset;
	ShaderBytecode.BytecodeLength = CodeSize;
	Shader->ShaderBytecode.SetShaderBytecode(ShaderBytecode);

	FD3D12Adapter& Adapter = GetAdapter();

#if USE_STATIC_ROOT_SIGNATURE
	Shader->pRootSignature = Adapter.GetStaticComputeRootSignature();
#else
	const D3D12_RESOURCE_BINDING_TIER Tier = Adapter.GetResourceBindingTier();
	FD3D12QuantizedBoundShaderState QBSS;
	QuantizeBoundShaderState(Tier, Shader, QBSS);
	Shader->pRootSignature = Adapter.GetRootSignature(QBSS);
#endif

	// NVCHANGE_BEGIN: Add VXGI
	check(NvidiaShaderExtensions.Num() == 0);
	// NVCHANGE_END: Add VXGI

	return Shader;
}

void FD3D12CommandContext::RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data)
{
	// Structures are chosen to be directly mappable
	StateCache.SetViewports(Count, reinterpret_cast<const D3D12_VIEWPORT*>(Data));
}

static volatile uint64 BoundShaderStateID = 0;

FD3D12BoundShaderState::FD3D12BoundShaderState(
	FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
	FVertexShaderRHIParamRef InVertexShaderRHI,
	FPixelShaderRHIParamRef InPixelShaderRHI,
	FHullShaderRHIParamRef InHullShaderRHI,
	FDomainShaderRHIParamRef InDomainShaderRHI,
	FGeometryShaderRHIParamRef InGeometryShaderRHI,
	FD3D12Device* InDevice
	) :
	CacheLink(InVertexDeclarationRHI, InVertexShaderRHI, InPixelShaderRHI, InHullShaderRHI, InDomainShaderRHI, InGeometryShaderRHI, this),
	UniqueID(FPlatformAtomics::InterlockedIncrement(reinterpret_cast<volatile int64*>(&BoundShaderStateID))),
	FD3D12DeviceChild(InDevice)
{
	INC_DWORD_STAT(STAT_D3D12NumBoundShaderState);

	// Warning: Input layout desc contains padding which must be zero-initialized to prevent PSO cache misses
	FMemory::Memzero(&InputLayout, sizeof(InputLayout));

	FD3D12VertexDeclaration*  InVertexDeclaration = FD3D12DynamicRHI::ResourceCast(InVertexDeclarationRHI);
	FD3D12VertexShader*  InVertexShader = FD3D12DynamicRHI::ResourceCast(InVertexShaderRHI);
	FD3D12PixelShader*  InPixelShader = FD3D12DynamicRHI::ResourceCast(InPixelShaderRHI);
	FD3D12HullShader*  InHullShader = FD3D12DynamicRHI::ResourceCast(InHullShaderRHI);
	FD3D12DomainShader*  InDomainShader = FD3D12DynamicRHI::ResourceCast(InDomainShaderRHI);
	FD3D12GeometryShader*  InGeometryShader = FD3D12DynamicRHI::ResourceCast(InGeometryShaderRHI);

	// Create an input layout for this combination of vertex declaration and vertex shader.
	InputLayout.NumElements = (InVertexDeclaration ? InVertexDeclaration->VertexElements.Num() : 0);
	InputLayout.pInputElementDescs = (InVertexDeclaration ? InVertexDeclaration->VertexElements.GetData() : nullptr);

	bShaderNeedsGlobalConstantBuffer[SF_Vertex] = InVertexShader ? InVertexShader->ResourceCounts.bGlobalUniformBufferUsed : false;
	bShaderNeedsGlobalConstantBuffer[SF_Hull] = InHullShader ? InHullShader->ResourceCounts.bGlobalUniformBufferUsed : false;
	bShaderNeedsGlobalConstantBuffer[SF_Domain] = InDomainShader ? InDomainShader->ResourceCounts.bGlobalUniformBufferUsed : false;
	bShaderNeedsGlobalConstantBuffer[SF_Pixel] = InPixelShader ? InPixelShader->ResourceCounts.bGlobalUniformBufferUsed : false;
	bShaderNeedsGlobalConstantBuffer[SF_Geometry] = InGeometryShader ? InGeometryShader->ResourceCounts.bGlobalUniformBufferUsed : false;

	static_assert(ARRAY_COUNT(bShaderNeedsGlobalConstantBuffer) == SF_NumFrequencies, "EShaderFrequency size should match with array count of bShaderNeedsGlobalConstantBuffer.");

	if (InVertexDeclaration)
	{
		FMemory::Memcpy(StreamStrides, InVertexDeclaration->StreamStrides, sizeof(StreamStrides));
	}
	else
	{
		FMemory::Memzero(StreamStrides, sizeof(StreamStrides));
	}

	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();

#if USE_STATIC_ROOT_SIGNATURE
	pRootSignature = Adapter->GetStaticGraphicsRootSignature();
#else
	const D3D12_RESOURCE_BINDING_TIER Tier = Adapter->GetResourceBindingTier();
	FD3D12QuantizedBoundShaderState QuantizedBoundShaderState;
	QuantizeBoundShaderState(Tier, this, QuantizedBoundShaderState);
	pRootSignature = Adapter->GetRootSignature(QuantizedBoundShaderState);
#endif

#if D3D12_SUPPORTS_PARALLEL_RHI_EXECUTE
	CacheLink.AddToCache();
#endif
}

FD3D12BoundShaderState::~FD3D12BoundShaderState()
{
	DEC_DWORD_STAT(STAT_D3D12NumBoundShaderState);
#if D3D12_SUPPORTS_PARALLEL_RHI_EXECUTE
	CacheLink.RemoveFromCache();
#endif
}

/**
* Creates a bound shader state instance which encapsulates a decl, vertex shader, and pixel shader
* @param VertexDeclaration - existing vertex decl
* @param StreamStrides - optional stream strides
* @param VertexShader - existing vertex shader
* @param HullShader - existing hull shader
* @param DomainShader - existing domain shader
* @param PixelShader - existing pixel shader
* @param GeometryShader - existing geometry shader
*/
FBoundShaderStateRHIRef FD3D12DynamicRHI::RHICreateBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclarationRHI,
	FVertexShaderRHIParamRef VertexShaderRHI,
	FHullShaderRHIParamRef HullShaderRHI,
	FDomainShaderRHIParamRef DomainShaderRHI,
	FPixelShaderRHIParamRef PixelShaderRHI,
	FGeometryShaderRHIParamRef GeometryShaderRHI
	)
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12CreateBoundShaderStateTime);

	checkf(GIsRHIInitialized && GetRHIDevice()->GetCommandListManager().IsReady(), (TEXT("Bound shader state RHI resource was created without initializing Direct3D first")));

#if D3D12_SUPPORTS_PARALLEL_RHI_EXECUTE
	// Check for an existing bound shader state which matches the parameters
	FBoundShaderStateRHIRef CachedBoundShaderState = GetCachedBoundShaderState_Threadsafe(
		VertexDeclarationRHI,
		VertexShaderRHI,
		PixelShaderRHI,
		HullShaderRHI,
		DomainShaderRHI,
		GeometryShaderRHI
		);
	if (CachedBoundShaderState.GetReference())
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		return CachedBoundShaderState;
	}
#else
	check(IsInRenderingThread() || IsInRHIThread());
	// Check for an existing bound shader state which matches the parameters
	FCachedBoundShaderStateLink* CachedBoundShaderStateLink = GetCachedBoundShaderState(
		VertexDeclarationRHI,
		VertexShaderRHI,
		PixelShaderRHI,
		HullShaderRHI,
		DomainShaderRHI,
		GeometryShaderRHI
		);
	if (CachedBoundShaderStateLink)
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		return CachedBoundShaderStateLink->BoundShaderState;
	}
#endif
	else
	{
		SCOPE_CYCLE_COUNTER(STAT_D3D12NewBoundShaderStateTime);

		return new FD3D12BoundShaderState(VertexDeclarationRHI, VertexShaderRHI, PixelShaderRHI, HullShaderRHI, DomainShaderRHI, GeometryShaderRHI, GetRHIDevice());
	}
}

struct FD3D12PipelineStateWrapper : public FRHIComputePipelineState
{
	FD3D12PipelineStateWrapper(FD3D12PipelineState* InPipelineState, FD3D12ComputeShader* InComputeShader)
		: PipelineState(InPipelineState)
		, ComputeShader(InComputeShader)
	{
	}

	FD3D12PipelineState* PipelineState;
	FD3D12ComputeShader* ComputeShader;
};

TRefCountPtr<FRHIComputePipelineState> FD3D12DynamicRHI::RHICreateComputePipelineState(FRHIComputeShader* ComputeShaderRHI)
{
	check(ComputeShaderRHI);
	FD3D12ComputeShader* ComputeShader = FD3D12DynamicRHI::ResourceCast(ComputeShaderRHI);
	FD3D12ComputePipelineStateDesc PSODesc;
	FMemory::Memzero(&PSODesc, sizeof(PSODesc));
	PSODesc.pRootSignature = ComputeShader->pRootSignature;
	PSODesc.Desc.pRootSignature = PSODesc.pRootSignature->GetRootSignature();
	PSODesc.Desc.CS = ComputeShader->ShaderBytecode.GetShaderBytecode();
	PSODesc.CSHash = ComputeShader->ShaderBytecode.GetHash();

	FD3D12PipelineStateCache& PSOCache = GetRHIDevice()->GetParentAdapter()->GetPSOCache();

	// Actual creation happens here
	FD3D12PipelineState* const PSO = PSOCache.FindCompute(&PSODesc);
	check(PSO != nullptr);

	return new FD3D12PipelineStateWrapper(PSO, ComputeShader);
}

void FD3D12CommandContext::RHISetComputePipelineState(FRHIComputePipelineState* ComputePipelineState)
{
	if (ComputePipelineState)
	{
		FD3D12PipelineStateWrapper* Wrapper = static_cast<FD3D12PipelineStateWrapper*>(ComputePipelineState);
		StateCache.SetComputeShader(Wrapper->ComputeShader);
	}
}

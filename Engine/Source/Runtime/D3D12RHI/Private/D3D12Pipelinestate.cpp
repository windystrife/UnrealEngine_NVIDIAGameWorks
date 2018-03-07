// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Implementation of D3D12 Pipelinestate related functions

//-----------------------------------------------------------------------------
//	Include Files
//-----------------------------------------------------------------------------
#include "D3D12RHIPrivate.h"

/// @cond DOXYGEN_WARNINGS

void FD3D12HighLevelGraphicsPipelineStateDesc::GetLowLevelDesc(FD3D12LowLevelGraphicsPipelineStateDesc& Desc)
{
	FMemory::Memzero(&Desc, sizeof(Desc));

	Desc.pRootSignature = BoundShaderState->pRootSignature;
	Desc.Desc.pRootSignature = Desc.pRootSignature->GetRootSignature();
	Desc.Desc.SampleMask = SampleMask;
	Desc.Desc.PrimitiveTopologyType = PrimitiveTopologyType;
	Desc.Desc.NumRenderTargets = NumRenderTargets;
	FMemory::Memcpy(Desc.Desc.RTVFormats, &RTVFormats[0], sizeof(RTVFormats[0]) * NumRenderTargets);
	Desc.Desc.DSVFormat = DSVFormat;
	Desc.Desc.SampleDesc = SampleDesc;

	Desc.Desc.InputLayout = BoundShaderState->InputLayout;

	if (BoundShaderState->GetGeometryShader())
	{
		Desc.Desc.StreamOutput = BoundShaderState->GetGeometryShader()->StreamOutput;
	}

	// NVCHANGE_BEGIN: Add VXGI
	auto addNvidiaExtensions = [&Desc](const TArray<const void*>& Extensions) {
		check(Desc.NumNvidiaShaderExtensions + Extensions.Num() < _countof(Desc.NvidiaShaderExtensions));
		FMemory::Memcpy(Desc.NvidiaShaderExtensions + Desc.NumNvidiaShaderExtensions, Extensions.GetData(), Extensions.Num() * sizeof(void*));
		Desc.NumNvidiaShaderExtensions += Extensions.Num();
	};

#define COPY_SHADER(Initial, Name) \
	if (FD3D12##Name##Shader* Shader = BoundShaderState->Get##Name##Shader()) \
	{ \
		Desc.Desc.Initial##S = Shader->ShaderBytecode.GetShaderBytecode(); \
		Desc.Initial##SHash = Shader->ShaderBytecode.GetHash(); \
		addNvidiaExtensions(Shader->NvidiaShaderExtensions); \
	}
	COPY_SHADER(V, Vertex);
	COPY_SHADER(P, Pixel);
	COPY_SHADER(D, Domain);
	COPY_SHADER(H, Hull);
	COPY_SHADER(G, Geometry);
#undef COPY_SHADER
	// NVCHANGE_END: Add VXGI

	Desc.Desc.BlendState = BlendState ? *BlendState : CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	Desc.Desc.RasterizerState = RasterizerState ? *RasterizerState : CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	Desc.Desc.DepthStencilState = DepthStencilState ? *DepthStencilState : CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
}

/// @endcond

FORCEINLINE uint32 SSE4_CRC32(void* Data, SIZE_T NumBytes)
{
	check(GCPUSupportsSSE4);
	uint32 Hash = 0;
#if defined(_WIN64)
	static const SIZE_T Alignment = 8;//64 Bit
#elif defined(_WIN32)
	static const SIZE_T Alignment = 4;//32 Bit
#else
	check(0);
	return 0;
#endif

	const SIZE_T RoundingIterations = (NumBytes & (Alignment - 1));
	uint8* UnalignedData = (uint8*)Data;
	for (SIZE_T i = 0; i < RoundingIterations; i++)
	{
		Hash = _mm_crc32_u8(Hash, UnalignedData[i]);
	}
	UnalignedData += RoundingIterations;
	NumBytes -= RoundingIterations;

	SIZE_T* AlignedData = (SIZE_T*)UnalignedData;
	check((NumBytes % Alignment) == 0);
	const SIZE_T NumIterations = (NumBytes / Alignment);
	for (SIZE_T i = 0; i < NumIterations; i++)
	{
#ifdef _WIN64
		Hash = _mm_crc32_u64(Hash, AlignedData[i]);
#else
		Hash = _mm_crc32_u32(Hash, AlignedData[i]);
#endif
	}

	return Hash;
}

SIZE_T FD3D12PipelineStateCacheBase::HashData(void* Data, SIZE_T NumBytes)
{
	if (GCPUSupportsSSE4)
	{
		return SIZE_T(SSE4_CRC32(Data, NumBytes));
	}
	else
	{
		return SIZE_T(FCrc::MemCrc32(Data, NumBytes));
	}
}

//TODO: optimize by pre-hashing these things at set time
SIZE_T FD3D12PipelineStateCacheBase::HashPSODesc(const FD3D12LowLevelGraphicsPipelineStateDesc& Desc)
{
	__declspec(align(32)) FD3D12LowLevelGraphicsPipelineStateDesc Hash;
	FMemory::Memcpy(&Hash, &Desc, sizeof(Hash)); // Memcpy to avoid introducing garbage due to alignment

	Hash.Desc.VS.pShaderBytecode = nullptr; // null out pointers so stale ones don't ruin the hash
	Hash.Desc.PS.pShaderBytecode = nullptr;
	Hash.Desc.HS.pShaderBytecode = nullptr;
	Hash.Desc.DS.pShaderBytecode = nullptr;
	Hash.Desc.GS.pShaderBytecode = nullptr;
	Hash.Desc.InputLayout.pInputElementDescs = nullptr;
	Hash.Desc.StreamOutput.pBufferStrides = nullptr;
	Hash.Desc.StreamOutput.pSODeclaration = nullptr;
	Hash.Desc.CachedPSO.pCachedBlob = nullptr;
	Hash.Desc.CachedPSO.CachedBlobSizeInBytes = 0;
	Hash.CombinedHash = 0;
	Hash.Desc.pRootSignature = nullptr;
	Hash.pRootSignature = nullptr;

	return HashData(&Hash, sizeof(Hash));
}

SIZE_T FD3D12PipelineStateCacheBase::HashPSODesc(const FD3D12HighLevelGraphicsPipelineStateDesc& Desc)
{
	__declspec(align(32)) FD3D12HighLevelGraphicsPipelineStateDesc Hash;
	FMemory::Memcpy(&Hash, &Desc, sizeof(Hash)); // Memcpy to avoid introducing garbage due to alignment

	Hash.CombinedHash = 0;

	return HashData(&Hash, sizeof(Hash));
}

SIZE_T FD3D12PipelineStateCacheBase::HashPSODesc(const FD3D12ComputePipelineStateDesc& Desc)
{
	__declspec(align(32)) FD3D12ComputePipelineStateDesc Hash;
	FMemory::Memcpy(&Hash, &Desc, sizeof(Hash)); // Memcpy to avoid introducing garbage due to alignment

	Hash.Desc.CS.pShaderBytecode = nullptr;  // null out pointers so stale ones don't ruin the hash
	Hash.Desc.CachedPSO.pCachedBlob = nullptr;
	Hash.Desc.CachedPSO.CachedBlobSizeInBytes = 0;
	Hash.CombinedHash = 0;
	Hash.Desc.pRootSignature = nullptr;
	Hash.pRootSignature = nullptr;

	return HashData(&Hash, sizeof(Hash));
}

#define SSE4_2     0x100000 
#define SSE4_CPUID_ARRAY_INDEX 2

FD3D12PipelineStateCacheBase::FD3D12PipelineStateCacheBase(FD3D12Adapter* InParent) :
	FD3D12AdapterChild(InParent)
{
#if UE_BUILD_DEBUG
	GraphicsCacheRequestCount = 0;
	HighLevelCacheFulfillCount = 0;
	HighLevelCacheStaleCount = 0;
	HighLevelCacheMissCount = 0;
#endif

	// Check for SSE4 support see: https://msdn.microsoft.com/en-us/library/vstudio/hskdteyh(v=vs.100).aspx
	{
		int32 cpui[4];
		__cpuidex(cpui, 1, 0);
		GCPUSupportsSSE4 = !!(cpui[SSE4_CPUID_ARRAY_INDEX] & SSE4_2);
	}
}

FD3D12PipelineStateCacheBase::~FD3D12PipelineStateCacheBase()
{
	CleanupPipelineStateCaches();
}


FD3D12PipelineState::FD3D12PipelineState(FD3D12Adapter* Parent)
	: Worker(nullptr)
	, FD3D12AdapterChild(Parent)
	, FD3D12MultiNodeGPUObject(Parent->ActiveGPUMask(), Parent->ActiveGPUMask()) //Create on all, visible on all
{
	INC_DWORD_STAT(STAT_D3D12NumPSOs);
}

FD3D12PipelineState::~FD3D12PipelineState()
{
	if (Worker)
	{
		Worker->EnsureCompletion(true);
		delete Worker;
		Worker = nullptr;
	}

	DEC_DWORD_STAT(STAT_D3D12NumPSOs);
}

ID3D12PipelineState* FD3D12PipelineState::GetPipelineState()
{
	if (Worker)
	{
		Worker->EnsureCompletion(true);

		check(Worker->IsWorkDone());
		PipelineState = Worker->GetTask().PSO;

		delete Worker;
		Worker = nullptr;
	}

	return PipelineState.GetReference();
}

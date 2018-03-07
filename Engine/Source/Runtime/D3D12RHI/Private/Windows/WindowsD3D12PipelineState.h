// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Implementation of D3D12 Pipelinestate related functions

#pragma once

class FD3D12RootSignature; // forward-declare
class FD3D12BoundShaderState; // forward-declare

struct FD3D12LowLevelGraphicsPipelineStateDesc
{
	const FD3D12RootSignature *pRootSignature;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc;
	ShaderBytecodeHash VSHash;
	ShaderBytecodeHash HSHash;
	ShaderBytecodeHash DSHash;
	ShaderBytecodeHash GSHash;
	ShaderBytecodeHash PSHash;

	SIZE_T CombinedHash;

	FORCEINLINE FString GetName() const { return FString::Printf(TEXT("%llu"), CombinedHash); }

	// NVCHANGE_BEGIN: Add VXGI
	const void* NvidiaShaderExtensions[4];
	uint32 NumNvidiaShaderExtensions;
	// NVCHANGE_END: Add VXGI
};

struct FD3D12HighLevelGraphicsPipelineStateDesc
{
	FD3D12BoundShaderState* BoundShaderState;
	D3D12_BLEND_DESC* BlendState;
	D3D12_DEPTH_STENCIL_DESC* DepthStencilState;
	D3D12_RASTERIZER_DESC* RasterizerState;
	// IBStripCutValue unused
	uint32 SampleMask;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType;
	uint32 NumRenderTargets;
	TRenderTargetFormatsArray RTVFormats;
	DXGI_FORMAT DSVFormat;
	DXGI_SAMPLE_DESC SampleDesc;

	SIZE_T CombinedHash; // Pre-computed hash

	void GetLowLevelDesc(FD3D12LowLevelGraphicsPipelineStateDesc& psoDesc);
};

struct FD3D12ComputePipelineStateDesc
{
	const FD3D12RootSignature* pRootSignature;
	D3D12_COMPUTE_PIPELINE_STATE_DESC Desc;
	ShaderBytecodeHash CSHash;

	SIZE_T CombinedHash;

	FORCEINLINE FString GetName() const { return FString::Printf(TEXT("%llu"), CombinedHash); }
};

struct ComputePipelineCreationArgs_POD
{
	const FD3D12ComputePipelineStateDesc* Desc;
	ID3D12PipelineLibrary* Library;

	void Init(ComputePipelineCreationArgs_POD InArgs)
	{
		Desc = InArgs.Desc;
		Library = InArgs.Library;
	}
};

struct ComputePipelineCreationArgs
{
	ComputePipelineCreationArgs()
	{
		Args.Desc = nullptr;
		Args.Library = nullptr;
	}

	ComputePipelineCreationArgs(const FD3D12ComputePipelineStateDesc* InDesc, ID3D12PipelineLibrary* InLibrary)
	{
		Args.Desc = InDesc;
		Args.Library = InLibrary;
	}

	ComputePipelineCreationArgs(const ComputePipelineCreationArgs& InArgs)
		: ComputePipelineCreationArgs(InArgs.Args.Desc, InArgs.Args.Library)
	{}

	ComputePipelineCreationArgs& operator=(const ComputePipelineCreationArgs& Other)
	{
		if (this != &Other)
		{
			Args.Desc = Other.Args.Desc;
			Args.Library = Other.Args.Library;
		}

		return *this;
	}

	ComputePipelineCreationArgs_POD Args;
};

struct GraphicsPipelineCreationArgs_POD
{
	const FD3D12LowLevelGraphicsPipelineStateDesc* Desc;
	ID3D12PipelineLibrary* Library;

	void Init(GraphicsPipelineCreationArgs_POD InArgs)
	{
		Desc = InArgs.Desc;
		Library = InArgs.Library;
	}
};

struct GraphicsPipelineCreationArgs
{
	GraphicsPipelineCreationArgs()
	{
		Args.Desc = nullptr;
		Args.Library = nullptr;
	}

	GraphicsPipelineCreationArgs(const FD3D12LowLevelGraphicsPipelineStateDesc* InDesc, ID3D12PipelineLibrary* InLibrary)
	{
		Args.Desc = InDesc;
		Args.Library = InLibrary;
	}

	GraphicsPipelineCreationArgs(const GraphicsPipelineCreationArgs& InArgs)
		: GraphicsPipelineCreationArgs(InArgs.Args.Desc, InArgs.Args.Library)
	{}

	GraphicsPipelineCreationArgs& operator=(const GraphicsPipelineCreationArgs& Other)
	{
		if (this != &Other)
		{
			Args.Desc = Other.Args.Desc;
			Args.Library = Other.Args.Library;
		}

		return *this;
	}

	GraphicsPipelineCreationArgs_POD Args;
};

template <typename TDesc> struct TPSOFunctionMap;
template<> struct TPSOFunctionMap < D3D12_GRAPHICS_PIPELINE_STATE_DESC >
{
	static decltype(&ID3D12Device::CreateGraphicsPipelineState) GetCreatePipelineState() { return &ID3D12Device::CreateGraphicsPipelineState; }
	static decltype(&ID3D12PipelineLibrary::LoadGraphicsPipeline) GetLoadPipeline() { return &ID3D12PipelineLibrary::LoadGraphicsPipeline; }
};
template<> struct TPSOFunctionMap < D3D12_COMPUTE_PIPELINE_STATE_DESC >
{
	static decltype(&ID3D12Device::CreateComputePipelineState) GetCreatePipelineState() { return &ID3D12Device::CreateComputePipelineState; }
	static decltype(&ID3D12PipelineLibrary::LoadComputePipeline) GetLoadPipeline() { return &ID3D12PipelineLibrary::LoadComputePipeline; }
};


#include "D3D12PipelineState.h"

class FD3D12PipelineStateCache : public FD3D12PipelineStateCacheBase
{
private:
	FDiskCacheInterface DiskBinaryCache;
	TRefCountPtr<ID3D12PipelineLibrary> PipelineLibrary;

	FD3D12PipelineState* Add(const GraphicsPipelineCreationArgs& Args);
	FD3D12PipelineState* Add(const ComputePipelineCreationArgs& Args);

	FD3D12PipelineState* FindGraphicsLowLevel(FD3D12LowLevelGraphicsPipelineStateDesc* Desc);

	void WriteOutShaderBlob(PSO_CACHE_TYPE Cache, ID3D12PipelineState* APIPso);

	template<typename PipelineStateDescType>
	void ReadBackShaderBlob(PipelineStateDescType& Desc, PSO_CACHE_TYPE Cache)
	{
		SIZE_T* cachedBlobOffset = nullptr;
		DiskCaches[Cache].SetPointerAndAdvanceFilePosition((void**)&cachedBlobOffset, sizeof(SIZE_T));

		SIZE_T* cachedBlobSize = nullptr;
		DiskCaches[Cache].SetPointerAndAdvanceFilePosition((void**)&cachedBlobSize, sizeof(SIZE_T));

		check(cachedBlobOffset);
		check(cachedBlobSize);

		if (UseCachedBlobs())
		{
			check(*cachedBlobSize);
			Desc.CachedPSO.CachedBlobSizeInBytes = *cachedBlobSize;
			Desc.CachedPSO.pCachedBlob = DiskBinaryCache.GetDataAt(*cachedBlobOffset);
		}
		else
		{
			Desc.CachedPSO.CachedBlobSizeInBytes = 0;
			Desc.CachedPSO.pCachedBlob = nullptr;
		}
	}

	bool UsePipelineLibrary() const
	{
		return bUseAPILibaries && PipelineLibrary != nullptr;
	}

	bool UseCachedBlobs() const
	{
		return bUseAPILibaries && bUseCachedBlobs && !UsePipelineLibrary();
	}

public:
	void RebuildFromDiskCache(ID3D12RootSignature* GraphicsRootSignature, ID3D12RootSignature* ComputeRootSignature);

	FD3D12PipelineState* FindGraphics(FD3D12HighLevelGraphicsPipelineStateDesc* Desc);
	FD3D12PipelineState* FindCompute(FD3D12ComputePipelineStateDesc* Desc);

	void Close();

	void Init(FString &GraphicsCacheFilename, FString &ComputeCacheFilename, FString &DriverBlobFilename);
	bool IsInErrorState() const;

	FD3D12PipelineStateCache(FD3D12Adapter* InParent);
	~FD3D12PipelineStateCache();

	static const bool bUseAPILibaries = true;
	static const bool bUseCachedBlobs = false;
	uint32 DriverShaderBlobs;
};
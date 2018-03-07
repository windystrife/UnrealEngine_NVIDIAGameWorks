// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Implementation of D3D12 Pipelinestate related functions

#pragma once

static bool GCPUSupportsSSE4;

#define PSO_IF_NOT_EQUAL_RETURN_FALSE( value ) if(lhs.##value != rhs.##value){ return false; }

#define PSO_IF_MEMCMP_FAILS_RETURN_FALSE( value ) if(FMemory::Memcmp(&lhs.##value, &rhs.##value, sizeof(rhs.##value)) != 0){ return false; }

#define PSO_IF_STRING_COMPARE_FAILS_RETURN_FALSE( value ) \
	const char* const lhString = lhs.##value##; \
	const char* const rhString = rhs.##value##; \
	if (lhString != rhString) \
	{ \
		if (strcmp(lhString, rhString) != 0) \
		{ \
			return false; \
		} \
	}

template <typename TDesc> struct equality_pipeline_state_desc;
template <> struct equality_pipeline_state_desc<FD3D12HighLevelGraphicsPipelineStateDesc>
{
	bool operator()(const FD3D12HighLevelGraphicsPipelineStateDesc& lhs, const FD3D12HighLevelGraphicsPipelineStateDesc& rhs)
	{
		PSO_IF_NOT_EQUAL_RETURN_FALSE(BoundShaderState);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(BlendState);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(DepthStencilState);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(RasterizerState);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(SampleMask);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(PrimitiveTopologyType);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(NumRenderTargets);
		for (SIZE_T i = 0; i < lhs.NumRenderTargets; i++)
		{
			PSO_IF_NOT_EQUAL_RETURN_FALSE(RTVFormats[i]);
		}
		PSO_IF_NOT_EQUAL_RETURN_FALSE(DSVFormat);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(SampleDesc.Count);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(SampleDesc.Quality);
		return true;
	}
};

template <> struct equality_pipeline_state_desc<FD3D12LowLevelGraphicsPipelineStateDesc>
{
	bool operator()(const FD3D12LowLevelGraphicsPipelineStateDesc& lhs, const FD3D12LowLevelGraphicsPipelineStateDesc& rhs)
	{
		// Order from most likely to change to least
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.PS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.VS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.GS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.DS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.HS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.NumElements)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.NumRenderTargets)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.DSVFormat)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.PrimitiveTopologyType)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.Flags)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.pRootSignature)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.SampleMask)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.IBStripCutValue)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.NodeMask)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.RasterizedStream)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.NumEntries)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.NumStrides)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.SampleDesc.Count)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.SampleDesc.Quality)

		PSO_IF_MEMCMP_FAILS_RETURN_FALSE(Desc.BlendState)
		PSO_IF_MEMCMP_FAILS_RETURN_FALSE(Desc.RasterizerState)
		PSO_IF_MEMCMP_FAILS_RETURN_FALSE(Desc.DepthStencilState)
		for (SIZE_T i = 0; i < lhs.Desc.NumRenderTargets; i++)
		{
			PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.RTVFormats[i]);
		}

		// Shader byte code is hashed with SHA1 (160 bit) so the chances of collision
		// should be tiny i.e if there were 1 quadrillion shaders the chance of a 
		// collision is ~ 1 in 10^18. so only do a full check on debug builds
		PSO_IF_NOT_EQUAL_RETURN_FALSE(VSHash)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(PSHash)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(GSHash)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(HSHash)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(DSHash)

		if (lhs.Desc.StreamOutput.pSODeclaration != rhs.Desc.StreamOutput.pSODeclaration &&
			lhs.Desc.StreamOutput.NumEntries)
		{
			for (SIZE_T i = 0; i < lhs.Desc.StreamOutput.NumEntries; i++)
			{
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].Stream)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].SemanticIndex)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].StartComponent)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].ComponentCount)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].OutputSlot)
				PSO_IF_STRING_COMPARE_FAILS_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].SemanticName)
			}
		}

		if (lhs.Desc.StreamOutput.pBufferStrides != rhs.Desc.StreamOutput.pBufferStrides &&
			lhs.Desc.StreamOutput.NumStrides)
		{
			for (SIZE_T i = 0; i < lhs.Desc.StreamOutput.NumStrides; i++)
			{
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pBufferStrides[i])
			}
		}

		if (lhs.Desc.InputLayout.pInputElementDescs != rhs.Desc.InputLayout.pInputElementDescs &&
			lhs.Desc.InputLayout.NumElements)
		{
			for (SIZE_T i = 0; i < lhs.Desc.InputLayout.NumElements; i++)
			{
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].SemanticIndex)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].Format)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].InputSlot)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].AlignedByteOffset)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].InputSlotClass)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].InstanceDataStepRate)
				PSO_IF_STRING_COMPARE_FAILS_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].SemanticName)
			}
		}
		return true;
	}
};

template <> struct equality_pipeline_state_desc<FD3D12ComputePipelineStateDesc>
{
	bool operator()(const FD3D12ComputePipelineStateDesc& lhs, const FD3D12ComputePipelineStateDesc& rhs)
	{
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.CS.BytecodeLength)
#if !PLATFORM_XBOXONE
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.Flags)
#endif
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.pRootSignature)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.NodeMask)

		// Shader byte code is hashed with SHA1 (160 bit) so the chances of collision
		// should be tiny i.e if there were 1 quadrillion shaders the chance of a 
		// collision is ~ 1 in 10^18. so only do a full check on debug builds
		PSO_IF_NOT_EQUAL_RETURN_FALSE(CSHash)

#if UE_BUILD_DEBUG
		if (lhs.Desc.CS.pShaderBytecode != rhs.Desc.CS.pShaderBytecode &&
			lhs.Desc.CS.pShaderBytecode != nullptr &&
			lhs.Desc.CS.BytecodeLength)
		{
			if (FMemory::Memcmp(lhs.Desc.CS.pShaderBytecode, rhs.Desc.CS.pShaderBytecode, lhs.Desc.CS.BytecodeLength) != 0)
			{
				return false;
			}
		}
#endif

		return true;
	}
};

struct ComputePipelineCreationArgs;
struct GraphicsPipelineCreationArgs;

struct FD3D12PipelineStateWorker : public FD3D12AdapterChild, public FNonAbandonableTask
{
	FD3D12PipelineStateWorker(FD3D12Adapter* Adapter, const ComputePipelineCreationArgs& InArgs)
		: bIsGraphics(false)
		, FD3D12AdapterChild(Adapter)
	{
		CreationArgs.ComputeArgs.Init(InArgs.Args);
	};

	FD3D12PipelineStateWorker(FD3D12Adapter* Adapter, const GraphicsPipelineCreationArgs& InArgs)
		: bIsGraphics(true)
		, FD3D12AdapterChild(Adapter)
	{
		CreationArgs.GraphicsArgs.Init(InArgs.Args);
	};

	void DoWork();

	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FD3D12PipelineStateWorker, STATGROUP_ThreadPoolAsyncTasks); }

	union PipelineCreationArgs
	{
		ComputePipelineCreationArgs_POD ComputeArgs;
		GraphicsPipelineCreationArgs_POD GraphicsArgs;
	} CreationArgs;

	const bool bIsGraphics;
	TRefCountPtr<ID3D12PipelineState> PSO;
};

struct FD3D12PipelineState : public FD3D12AdapterChild, public FD3D12MultiNodeGPUObject, public FNoncopyable
{
public:
	explicit FD3D12PipelineState(FD3D12Adapter* Parent);
	~FD3D12PipelineState();

	void Create(const ComputePipelineCreationArgs& InCreationArgs);
	void CreateAsync(const ComputePipelineCreationArgs& InCreationArgs);

	void Create(const GraphicsPipelineCreationArgs& InCreationArgs);
	void CreateAsync(const GraphicsPipelineCreationArgs& InCreationArgs);

	ID3D12PipelineState* GetPipelineState();

	FD3D12PipelineState& operator=(const FD3D12PipelineState& other)
	{
		checkSlow(m_NodeMask == other.m_NodeMask);
		checkSlow(m_VisibilityMask == other.m_VisibilityMask);

		PipelineState = other.PipelineState;
		Worker = other.Worker;

		return *this;
	}

protected:
	TRefCountPtr<ID3D12PipelineState> PipelineState;
	FAsyncTask<FD3D12PipelineStateWorker>* Worker;
};

struct FD3D12GraphicsPipelineState : public FRHIGraphicsPipelineState
{
	FD3D12GraphicsPipelineState()
		: PipelineState(nullptr)
	{
	}

	FD3D12GraphicsPipelineState(
		const FGraphicsPipelineStateInitializer& Initializer,
		FD3D12PipelineState* InPipelineState
	)
		: PipelineStateInitializer(Initializer)
		, PipelineState(InPipelineState)
	{
	}

	FGraphicsPipelineStateInitializer PipelineStateInitializer;
	FD3D12PipelineState* PipelineState;
};

class FD3D12PipelineStateCacheBase : public FD3D12AdapterChild
{
protected:
	enum PSO_CACHE_TYPE
	{
		PSO_CACHE_GRAPHICS,
		PSO_CACHE_COMPUTE,
		NUM_PSO_CACHE_TYPES
	};

	template <typename TDesc, typename TValue>
	struct TStateCacheKeyFuncs : BaseKeyFuncs<TPair<TDesc, TValue>, TDesc, false>
	{
		typedef typename TTypeTraits<TDesc>::ConstPointerType KeyInitType;
		typedef const typename TPairInitializer<typename TTypeTraits<TDesc>::ConstInitType, typename TTypeTraits<TValue>::ConstInitType>& ElementInitType;

		static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element)
		{
			return Element.Key;
		}
		static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
		{
			equality_pipeline_state_desc<TDesc> equal;
			return equal(A, B);
		}
		static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
		{
			return Key.CombinedHash;
		}
	};

	template <typename TDesc, typename TValue = FD3D12PipelineState*>
	using TPipelineCache = TMap<TDesc, TValue, FDefaultSetAllocator, TStateCacheKeyFuncs<TDesc, TValue>>;

	TPipelineCache<FD3D12HighLevelGraphicsPipelineStateDesc, TPair<FD3D12PipelineState*, uint64>> HighLevelGraphicsPipelineStateCache;
	TPipelineCache<FD3D12LowLevelGraphicsPipelineStateDesc> LowLevelGraphicsPipelineStateCache;
	TPipelineCache<FD3D12ComputePipelineStateDesc> ComputePipelineStateCache;

	FCriticalSection CS;
	FDiskCacheInterface DiskCaches[NUM_PSO_CACHE_TYPES];

#if UE_BUILD_DEBUG
	uint64 GraphicsCacheRequestCount;
	uint64 HighLevelCacheFulfillCount;
	uint64 HighLevelCacheStaleCount;
	uint64 HighLevelCacheMissCount;
#endif

	void CleanupPipelineStateCaches()
	{
		// The high level graphics cache doesn't manage lifetime, we can just empty it.
		HighLevelGraphicsPipelineStateCache.Empty();

		// The low level graphics and compute maps manage the lifetime of their PSOs.
		// We need to delete each element before we empty it.
		for (auto Iter = LowLevelGraphicsPipelineStateCache.CreateConstIterator(); Iter; ++Iter)
		{
			const FD3D12PipelineState* const PipelineState = Iter.Value();
			delete PipelineState;
		}
		LowLevelGraphicsPipelineStateCache.Empty();

		for (auto Iter = ComputePipelineStateCache.CreateConstIterator(); Iter; ++Iter)
		{
			const FD3D12PipelineState* const PipelineState = Iter.Value();
			delete PipelineState;
		}
		ComputePipelineStateCache.Empty();
	}

public:
	static SIZE_T HashPSODesc(const FD3D12HighLevelGraphicsPipelineStateDesc& Desc);
	static SIZE_T HashPSODesc(const FD3D12LowLevelGraphicsPipelineStateDesc& Desc);
	static SIZE_T HashPSODesc(const FD3D12ComputePipelineStateDesc& Desc);

	static SIZE_T HashData(void* Data, SIZE_T NumBytes);

	FD3D12PipelineStateCacheBase(FD3D12Adapter* InParent);
	~FD3D12PipelineStateCacheBase();
};
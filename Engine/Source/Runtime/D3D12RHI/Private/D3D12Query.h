// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Query.h: Implementation of D3D12 Query
=============================================================================*/
#pragma once

/** D3D12 Render query */
class FD3D12RenderQuery : public FRHIRenderQuery, public FD3D12DeviceChild, public FD3D12LinkedAdapterObject<FD3D12RenderQuery>
{
public:

	/** The query heap resource. */
	const TRefCountPtr<ID3D12QueryHeap> QueryHeap;
	uint32 HeapIndex;

	/** CPU-visible buffer to store the query result **/
	const TRefCountPtr<ID3D12Resource> ResultBuffer;

	/** The cached query result. */
	uint64 Result;

	/** true if the query's result is cached. */
	bool bResultIsCached : 1;

	// todo: memory optimize
	const ERenderQueryType Type;

	// Context that the query was ended'd on.
	class FD3D12CommandContext* OwningContext;

	// When the query result is ready on the GPU.
	FD3D12CLSyncPoint CLSyncPoint;

	/** Initialization constructor. */
	FD3D12RenderQuery(FD3D12Device* Parent, ID3D12QueryHeap* InQueryHeap, ID3D12Resource* InQueryResultBuffer, ERenderQueryType InQueryType) :
		QueryHeap(InQueryHeap),
		ResultBuffer(InQueryResultBuffer),
		Result(0),
		Type(InQueryType),
		FD3D12DeviceChild(Parent)
	{
		Reset();
	}

	void Reset()
	{
		HeapIndex = -1;
		bResultIsCached = false;
		OwningContext = nullptr;
	}
};

template<>
struct TD3D12ResourceTraits<FRHIRenderQuery>
{
	typedef FD3D12RenderQuery TConcreteType;
};

// This class handles query heaps
class FD3D12QueryHeap : public FD3D12DeviceChild, public FD3D12SingleNodeGPUObject
{
private:
	struct QueryBatch
	{
	private:
		int64 BatchID;            // The unique ID for the batch

		int64 GenerateID()
		{
#if WINVER >= 0x0600 // Interlock...64 functions are only available from Vista onwards
			static int64 ID = 0;
#else
			static int32 ID = 0;
#endif
			return FPlatformAtomics::InterlockedIncrement(&ID);
		}

	public:
		uint32 StartElement;    // The first element in the batch (inclusive)
		uint32 EndElement;      // The last element in the batch (inclusive)
		uint32 ElementCount;    // The number of elements in the batch
		bool bOpen;             // Is the batch still open for more begin/end queries?

		void Clear()
		{
			StartElement = 0;
			EndElement = 0;
			ElementCount = 0;
			BatchID = GenerateID();
			bOpen = false;
		}

		int64 GetBatchID() const { return BatchID; }

		bool IsValidElement(uint32 Element) const
		{
			return ((Element >= StartElement) && (Element <= EndElement));
		}
	};

public:
	FD3D12QueryHeap(class FD3D12Device* InParent, const D3D12_QUERY_HEAP_TYPE &InQueryHeapType, uint32 InQueryHeapCount);
	~FD3D12QueryHeap();

	void Init();

	void Destroy();

	void StartQueryBatch(FD3D12CommandContext& CmdContext); // Start tracking a new batch of begin/end query calls that will be resolved together
	void EndQueryBatchAndResolveQueryData(FD3D12CommandContext& CmdContext, D3D12_QUERY_TYPE InQueryType); // Stop tracking the current batch of begin/end query calls that will be resolved together

	uint32 BeginQuery(FD3D12CommandContext& CmdContext, D3D12_QUERY_TYPE InQueryType); // Obtain a query from the store of available queries
	void EndQuery(FD3D12CommandContext& CmdContext, D3D12_QUERY_TYPE InQueryType, uint32 InElement);

	uint32 GetQueryHeapCount() const { return QueryHeapDesc.Count; }
	uint32 GetResultSize() const { return ResultSize; }

	inline FD3D12Resource* GetResultBuffer() { return ResultBuffer.GetReference(); }

private:
	uint32 GetNextElement(uint32 InElement); // Get the next element, after the specified element. Handles overflow.
	uint32 GetPreviousElement(uint32 InElement); // Get the previous element, before the specified element. Handles underflow.
	bool IsHeapFull();
	bool IsHeapEmpty() const { return ActiveAllocatedElementCount == 0; }

	uint32 GetNextBatchElement(uint32 InBatchElement);
	uint32 GetPreviousBatchElement(uint32 InBatchElement);

	uint32 AllocQuery(FD3D12CommandContext& CmdContext, D3D12_QUERY_TYPE InQueryType);
	void CreateQueryHeap();
	void CreateResultBuffer();

	uint64 GetResultBufferOffsetForElement(uint32 InElement) const { return ResultSize * InElement; };

private:
	QueryBatch CurrentQueryBatch;                       // The current recording batch.

	TArray<QueryBatch> ActiveQueryBatches;              // List of active query batches. The data for these is in use.

	static const uint32 MAX_ACTIVE_BATCHES = 5;         // The max number of query batches that will be held.
	uint32 LastBatch;                                   // The index of the newest batch.

	uint32 HeadActiveElement;                   // The oldest element that is in use (Active). The data for this element is being used.
	uint32 TailActiveElement;                   // The most recent element that is in use (Active). The data for this element is being used.
	uint32 ActiveAllocatedElementCount;         // The number of elements that are in use (Active). Between the head and the tail.

	uint32 LastAllocatedElement;                // The last element that was allocated for BeginQuery
	const uint32 ResultSize;                    // The byte size of a result for a single query
	D3D12_QUERY_HEAP_DESC QueryHeapDesc;        // The description of the current query heap
	D3D12_QUERY_TYPE QueryType;
	TRefCountPtr<ID3D12QueryHeap> QueryHeap;    // The query heap where all elements reside
	TRefCountPtr<FD3D12Resource> ResultBuffer;  // The buffer where all query results are stored
	void* pResultData;
};
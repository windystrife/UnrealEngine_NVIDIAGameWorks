// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12Query.cpp: D3D query RHI implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"

namespace D3D12RHI
{
	/**
	* RHI console variables used by queries.
	*/
	namespace RHIConsoleVariables
	{
		int32 bStablePowerState = 0;
		static FAutoConsoleVariableRef CVarStablePowerState(
			TEXT("D3D12.StablePowerState"),
			bStablePowerState,
			TEXT("If true, enable stable power state. This increases GPU timing measurement accuracy but may decrease overall GPU clock rate."),
			ECVF_Default
			);
	}
}
using namespace D3D12RHI;

FRenderQueryRHIRef FD3D12DynamicRHI::RHICreateRenderQuery_RenderThread(FRHICommandListImmediate& RHICmdList, ERenderQueryType QueryType)
{
	return RHICreateRenderQuery(QueryType);
}

FRenderQueryRHIRef FD3D12DynamicRHI::RHICreateRenderQuery(ERenderQueryType QueryType)
{
	FD3D12Adapter* Adapter = &GetAdapter();

	check(QueryType == RQT_Occlusion || QueryType == RQT_AbsoluteTime);

	return Adapter->CreateLinkedObject<FD3D12RenderQuery>([QueryType](FD3D12Device* Device)
	{
		FD3D12RenderQuery* NewQuery = nullptr;
		if (QueryType == RQT_AbsoluteTime)
		{
			ID3D12QueryHeap* pQueryHeap = nullptr;
			ID3D12Resource* pQueryResultBuffer = nullptr;

			D3D12_QUERY_HEAP_DESC QueryHeapDesc = {};
			QueryHeapDesc.Count = 1;
			QueryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
			QueryHeapDesc.NodeMask = Device->GetNodeMask();
			VERIFYD3D12RESULT(Device->GetDevice()->CreateQueryHeap(&QueryHeapDesc, IID_PPV_ARGS(&pQueryHeap)));

			const D3D12_HEAP_PROPERTIES HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
			const D3D12_RESOURCE_DESC HeapDesc = CD3DX12_RESOURCE_DESC::Buffer(8);
			VERIFYD3D12RESULT(
				Device->GetDevice()->CreateCommittedResource(
					&HeapProperties,
					D3D12_HEAP_FLAG_NONE,
					&HeapDesc,
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&pQueryResultBuffer)
					)
				);

			NewQuery = new FD3D12RenderQuery(Device, pQueryHeap, pQueryResultBuffer, QueryType);
			NewQuery->HeapIndex = 0;
		}
		else
		{
			// Occlusion query heap and result buffer will be assigned later.
			NewQuery = new FD3D12RenderQuery(Device, nullptr, nullptr, QueryType);
		}

		return NewQuery;
	});
}

bool FD3D12DynamicRHI::RHIGetRenderQueryResult(FRenderQueryRHIParamRef QueryRHI, uint64& OutResult, bool bWait)
{
	check(IsInRenderingThread());
	FD3D12Adapter& Adapter = GetAdapter();

	int32 Index = (GFrameNumberRenderThread - 1) % Adapter.GetNumGPUNodes();
	FD3D12CommandContext& DefaultContext = Adapter.GetDeviceByIndex(Index)->GetDefaultCommandContext();
	FD3D12RenderQuery* Query = DefaultContext.RetrieveObject<FD3D12RenderQuery>(QueryRHI);

	if (Query->HeapIndex == INDEX_NONE)
	{
		// This query hasn't been submitted before
		return false;
	}

	bool bSuccess = true;
	if (!Query->bResultIsCached)
	{
		SCOPE_CYCLE_COUNTER(STAT_RenderQueryResultTime);
		bSuccess = Query->GetParentDevice()->GetQueryData(*Query, bWait);


		Query->bResultIsCached = bSuccess;
	}

	if (Query->Type == RQT_AbsoluteTime)
	{
		// GetTimingFrequency is the number of ticks per second
		uint64 Div = FMath::Max(1llu, FGPUTiming::GetTimingFrequency() / (1000 * 1000));

		// convert from GPU specific timestamp to micro sec (1 / 1 000 000 s) which seems a reasonable resolution
		OutResult = Query->Result / Div;
	}
	else
	{
		OutResult = Query->Result;
	}
	return bSuccess;
}

bool FD3D12Device::GetQueryData(FD3D12RenderQuery& Query, bool bWait)
{
	// Wait for the query result to be ready (if requested).
	const FD3D12CLSyncPoint& SyncPoint = Query.CLSyncPoint;
	if (!SyncPoint.IsComplete())
	{
		if (!bWait)
		{
			return false;
		}

		SCOPE_CYCLE_COUNTER(STAT_RenderQueryResultTime);

		const uint32 IdleStart = FPlatformTime::Cycles();

		if (SyncPoint.IsOpen())
		{
			// The query is on a command list that hasn't been submitted yet.
			// We need to flush, but the RHI thread may be using the default command list...so stall it first.
			check(IsInRenderingThread());
			check(Query.OwningContext->IsDefaultContext());
			FScopedRHIThreadStaller StallRHIThread(FRHICommandListExecutor::GetImmediateCommandList());
			Query.OwningContext->FlushCommands();	// Don't wait yet, since we're stalling the RHI thread.
		}

		SyncPoint.WaitForCompletion();

		GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery] += FPlatformTime::Cycles() - IdleStart;
		GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUQuery]++;
	}

	// Read the data from the query's buffer.
	static const CD3DX12_RANGE EmptyRange(0, 0);
	if (Query.Type == RQT_Occlusion)
	{
		const uint32 BeginOffset = Query.HeapIndex * sizeof(Query.Result);
		const CD3DX12_RANGE ReadRange(BeginOffset, BeginOffset + sizeof(Query.Result));
		const FD3D12ScopeMap<uint64> MappedData(OcclusionQueryHeap.GetResultBuffer(), 0, &ReadRange, &EmptyRange /* Not writing any data */);
		Query.Result = MappedData[Query.HeapIndex];
		return true;
	}
	else
	{
		static const CD3DX12_RANGE ReadRange(0, sizeof(Query.Result));
		const FD3D12ScopeMap<uint64> MappedData(Query.ResultBuffer.GetReference(), 0, &ReadRange, &EmptyRange /* Not writing any data */);
		Query.Result = MappedData[0];
		return true;
	}
}

void FD3D12CommandContext::RHIBeginOcclusionQueryBatch()
{
	GetParentDevice()->GetQueryHeap()->StartQueryBatch(*this);
}

void FD3D12CommandContext::RHIEndOcclusionQueryBatch()
{
	GetParentDevice()->GetQueryHeap()->EndQueryBatchAndResolveQueryData(*this, D3D12_QUERY_TYPE_OCCLUSION);

	// Note: We want to execute this ASAP. The Engine will call RHISubmitCommandHint after this.
	// We'll break up the command list there so that the wait on the previous frame's results don't block.
}

/*=============================================================================
* class FD3D12QueryHeap
*=============================================================================*/

FD3D12QueryHeap::FD3D12QueryHeap(FD3D12Device* InParent, const D3D12_QUERY_HEAP_TYPE &InQueryHeapType, uint32 InQueryHeapCount)
	:HeadActiveElement(0)
	, TailActiveElement(0)
	, ActiveAllocatedElementCount(0)
	, LastAllocatedElement(InQueryHeapCount - 1)
	, ResultSize(8)
	, pResultData(nullptr)
	, LastBatch(MAX_ACTIVE_BATCHES - 1)
	, FD3D12DeviceChild(InParent)
	, FD3D12SingleNodeGPUObject(InParent->GetNodeMask())
{
	if (InQueryHeapType == D3D12_QUERY_HEAP_TYPE_OCCLUSION)
	{
		QueryType = D3D12_QUERY_TYPE_OCCLUSION;
	}
	else if (InQueryHeapType == D3D12_QUERY_HEAP_TYPE_TIMESTAMP)
	{
		QueryType = D3D12_QUERY_TYPE_TIMESTAMP;
	}
	else
	{
		check(false);
	}

	// Setup the query heap desc
	QueryHeapDesc = {};
	QueryHeapDesc.Type = InQueryHeapType;
	QueryHeapDesc.Count = InQueryHeapCount;
	QueryHeapDesc.NodeMask = GetNodeMask();

	CurrentQueryBatch.Clear();

	ActiveQueryBatches.Reserve(MAX_ACTIVE_BATCHES);
	ActiveQueryBatches.AddZeroed(MAX_ACTIVE_BATCHES);

	// Don't Init() until the RHI has created the device
}

FD3D12QueryHeap::~FD3D12QueryHeap()
{
	// Unmap the result buffer
	if (pResultData)
	{
		ResultBuffer->GetResource()->Unmap(0, nullptr);
		pResultData = nullptr;
	}
}

void FD3D12QueryHeap::Init()
{
	check(GetParentDevice());
	check(GetParentDevice()->GetDevice());

	// Create the query heap
	CreateQueryHeap();

	// Create the result buffer
	CreateResultBuffer();
}

void FD3D12QueryHeap::Destroy()
{
	if (pResultData)
	{
		ResultBuffer->GetResource()->Unmap(0, nullptr);
		pResultData = nullptr;
	}

	QueryHeap = nullptr;
	ResultBuffer = nullptr;
}

uint32 FD3D12QueryHeap::GetNextElement(uint32 InElement)
{
	// Increment the provided element
	InElement++;

	// See if we need to wrap around to the begining of the heap
	if (InElement >= GetQueryHeapCount())
	{
		InElement = 0;
	}

	return InElement;
}

uint32 FD3D12QueryHeap::GetPreviousElement(uint32 InElement)
{
	// Decrement the provided element
	InElement--;

	// See if we need to wrap around to the end of the heap
	if (InElement == UINT_MAX)
	{
		InElement = GetQueryHeapCount() - 1;
	}

	return InElement;
}

uint32 FD3D12QueryHeap::GetNextBatchElement(uint32 InBatchElement)
{
	// Increment the provided element
	InBatchElement++;

	// See if we need to wrap around to the begining of the heap
	if (InBatchElement >= MAX_ACTIVE_BATCHES)
	{
		InBatchElement = 0;
	}

	return InBatchElement;
}

uint32 FD3D12QueryHeap::GetPreviousBatchElement(uint32 InBatchElement)
{
	// Decrement the provided element
	InBatchElement--;

	// See if we need to wrap around to the end of the heap
	if (InBatchElement == UINT_MAX)
	{
		InBatchElement = MAX_ACTIVE_BATCHES - 1;
	}

	return InBatchElement;
}

bool FD3D12QueryHeap::IsHeapFull()
{
	// Find the next element after the active tail and compare with the head
	return (GetNextElement(TailActiveElement) == HeadActiveElement);
}

uint32 FD3D12QueryHeap::AllocQuery(FD3D12CommandContext& CmdContext, D3D12_QUERY_TYPE InQueryType)
{
	check(CurrentQueryBatch.bOpen);

	// Get the element for this allocation
	const uint32 CurrentElement = GetNextElement(LastAllocatedElement);

	if (CurrentQueryBatch.StartElement > CurrentElement)
	{
		// We're in the middle of a batch, but we're at the end of the heap
		// We need to split the batch in two and resolve the first piece
		EndQueryBatchAndResolveQueryData(CmdContext, InQueryType);

		StartQueryBatch(CmdContext);
	}

	// Increment the count for the current batch
	CurrentQueryBatch.ElementCount++;

	LastAllocatedElement = CurrentElement;
	check(CurrentElement < GetQueryHeapCount());
	return CurrentElement;
}

void FD3D12QueryHeap::StartQueryBatch(FD3D12CommandContext& CmdContext)
{
	check(&CmdContext == &GetParentDevice()->GetDefaultCommandContext());
	check(!CurrentQueryBatch.bOpen);

	// Clear the current batch
	CurrentQueryBatch.Clear();

	// Start a new batch
	CurrentQueryBatch.StartElement = GetNextElement(LastAllocatedElement);
	CurrentQueryBatch.bOpen = true;
}

void FD3D12QueryHeap::EndQueryBatchAndResolveQueryData(FD3D12CommandContext& CmdContext, D3D12_QUERY_TYPE InQueryType)
{
	check(CurrentQueryBatch.bOpen);
	check(&CmdContext == &GetParentDevice()->GetDefaultCommandContext());

	// Close the current batch
	CurrentQueryBatch.bOpen = false;

	// Discard empty batches
	if (CurrentQueryBatch.ElementCount == 0)
	{
		return;
	}

	// Update the end element
	CurrentQueryBatch.EndElement = CurrentQueryBatch.StartElement + CurrentQueryBatch.ElementCount - 1;

	// Update the tail
	TailActiveElement = CurrentQueryBatch.EndElement;
	check(TailActiveElement < GetQueryHeapCount());

	// Increment the active element count
	ActiveAllocatedElementCount += CurrentQueryBatch.ElementCount;
	checkf(ActiveAllocatedElementCount <= GetQueryHeapCount(), TEXT("The query heap is too small. Either increase the heap count (larger resource) or decrease MAX_ACTIVE_BATCHES."));

	// Track the current active batches (application is using the data)
	LastBatch = GetNextBatchElement(LastBatch);
	ActiveQueryBatches[LastBatch] = CurrentQueryBatch;

	// Update the head
	QueryBatch& OldestBatch = ActiveQueryBatches[GetNextBatchElement(LastBatch)];
	HeadActiveElement = OldestBatch.StartElement;
	ActiveAllocatedElementCount -= OldestBatch.ElementCount;

	CmdContext.otherWorkCounter++;
	CmdContext.CommandListHandle->ResolveQueryData(
		QueryHeap, InQueryType, CurrentQueryBatch.StartElement, CurrentQueryBatch.ElementCount,
		ResultBuffer->GetResource(), GetResultBufferOffsetForElement(CurrentQueryBatch.StartElement));

	CmdContext.CommandListHandle.UpdateResidency(ResultBuffer);
}

uint32 FD3D12QueryHeap::BeginQuery(FD3D12CommandContext& CmdContext, D3D12_QUERY_TYPE InQueryType)
{
	const uint32 Element = AllocQuery(CmdContext, InQueryType);
	CmdContext.otherWorkCounter++;
	CmdContext.CommandListHandle->BeginQuery(QueryHeap, InQueryType, Element);

	CmdContext.CommandListHandle.UpdateResidency(ResultBuffer);

	return Element;
}

void FD3D12QueryHeap::EndQuery(FD3D12CommandContext& CmdContext, D3D12_QUERY_TYPE InQueryType, uint32 InElement)
{
	CmdContext.otherWorkCounter++;
	CmdContext.CommandListHandle->EndQuery(QueryHeap, InQueryType, InElement);

	CmdContext.CommandListHandle.UpdateResidency(ResultBuffer);
}

void FD3D12QueryHeap::CreateQueryHeap()
{
	// Create the upload heap
	VERIFYD3D12RESULT(GetParentDevice()->GetDevice()->CreateQueryHeap(&QueryHeapDesc, IID_PPV_ARGS(QueryHeap.GetInitReference())));
}

void FD3D12QueryHeap::CreateResultBuffer()
{
	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();

	const D3D12_HEAP_PROPERTIES ResultBufferHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK, GetNodeMask(), GetVisibilityMask());
	const D3D12_RESOURCE_DESC ResultBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(ResultSize * QueryHeapDesc.Count); // Each query's result occupies ResultSize bytes.

	// Create the readback heap
	VERIFYD3D12RESULT(Adapter->CreateCommittedResource(
		ResultBufferDesc,
		ResultBufferHeapProperties,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		ResultBuffer.GetInitReference()));

	// Map the result buffer (and keep it mapped)
	VERIFYD3D12RESULT(ResultBuffer->GetResource()->Map(0, nullptr, &pResultData));
}

/*=============================================================================
 * class FD3D12BufferedGPUTiming
 *=============================================================================*/

 /**
  * Constructor.
  *
  * @param InD3DRHI			RHI interface
  * @param InBufferSize		Number of buffered measurements
  */
FD3D12BufferedGPUTiming::FD3D12BufferedGPUTiming(FD3D12Adapter* InParent, int32 InBufferSize)
	: BufferSize(InBufferSize)
	, CurrentTimestamp(-1)
	, NumIssuedTimestamps(0)
	, TimestampQueryHeap(nullptr)
	, TimestampQueryHeapBuffer(nullptr)
	, bIsTiming(false)
	, bStablePowerState(false)
	, FD3D12AdapterChild(InParent)
{
}

/**
 * Initializes the static variables, if necessary.
 */
void FD3D12BufferedGPUTiming::PlatformStaticInitialize(void* UserData)
{
	// Are the static variables initialized?
	check(!GAreGlobalsInitialized);

	GTimingFrequency = 0;
	FD3D12Adapter* ParentAdapter = (FD3D12Adapter*)UserData;
	VERIFYD3D12RESULT(ParentAdapter->GetDevice()->GetCommandListManager().GetTimestampFrequency(&GTimingFrequency));
}

/**
 * Initializes all D3D resources and if necessary, the static variables.
 */
void FD3D12BufferedGPUTiming::InitDynamicRHI()
{
	FD3D12Adapter* Adapter = GetParentAdapter();
	ID3D12Device* D3DDevice = Adapter->GetD3DDevice();
	const GPUNodeMask Node = Adapter->ActiveGPUMask();

	StaticInitialize(Adapter, PlatformStaticInitialize);

	CurrentTimestamp = 0;
	NumIssuedTimestamps = 0;
	bIsTiming = false;

	// Now initialize the queries and backing buffers for this timing object.
	if (GIsSupported)
	{
		D3D12_QUERY_HEAP_DESC QueryHeapDesc = {};
		QueryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		QueryHeapDesc.Count = BufferSize * 2;	// Space for each Start + End pair.

		TimestampQueryHeap = Adapter->CreateLinkedObject<QueryHeap>([&] (FD3D12Device* Device)
		{
			QueryHeap* NewHeap = new QueryHeap(Device);
			QueryHeapDesc.NodeMask = Device->GetNodeMask();
			VERIFYD3D12RESULT(D3DDevice->CreateQueryHeap(&QueryHeapDesc, IID_PPV_ARGS(NewHeap->Heap.GetInitReference())));
			SetName(NewHeap->Heap, L"FD3D12BufferedGPUTiming: Timestamp Query Heap");

			return NewHeap;
		});

		const uint64 Size = 8 * QueryHeapDesc.Count; // Each timestamp query occupies 8 bytes.
		Adapter->CreateBuffer(D3D12_HEAP_TYPE_READBACK, GDefaultGPUMask, Node, Size, TimestampQueryHeapBuffer.GetInitReference());
		SetName(TimestampQueryHeapBuffer, L"FD3D12BufferedGPUTiming: Timestamp Query Result Buffer");

		TimestampListHandles.AddZeroed(QueryHeapDesc.Count);
	}
}

/**
 * Releases all D3D resources.
 */
void FD3D12BufferedGPUTiming::ReleaseDynamicRHI()
{
	delete(TimestampQueryHeap);
	TimestampQueryHeap = nullptr;
	TimestampQueryHeapBuffer = nullptr;
}

/**
 * Start a GPU timing measurement.
 */
void FD3D12BufferedGPUTiming::StartTiming()
{
	FD3D12Adapter* Adapter = GetParentAdapter();
	ID3D12Device* D3DDevice = Adapter->GetD3DDevice();

	// Issue a timestamp query for the 'start' time.
	if (GIsSupported && !bIsTiming)
	{
		// Check to see if stable power state cvar has changed
		const bool bStablePowerStateCVar = RHIConsoleVariables::bStablePowerState != 0;
		if (bStablePowerState != bStablePowerStateCVar)
		{
			if (SUCCEEDED(D3DDevice->SetStablePowerState(bStablePowerStateCVar)))
			{
				// SetStablePowerState succeeded. Update timing frequency.
				VERIFYD3D12RESULT(Adapter->GetDevice()->GetCommandListManager().GetTimestampFrequency(&GTimingFrequency));
				bStablePowerState = bStablePowerStateCVar;
			}
			else
			{
				// SetStablePowerState failed. This can occur if SDKLayers is not present on the system.
				RHIConsoleVariables::CVarStablePowerState->Set(0, ECVF_SetByConsole);
			}
		}

		CurrentTimestamp = (CurrentTimestamp + 1) % BufferSize;

		const uint32 QueryStartIndex = GetStartTimestampIndex(CurrentTimestamp);
		FD3D12CommandContext& CmdContext = Adapter->GetCurrentDevice()->GetDefaultCommandContext();
		CmdContext.otherWorkCounter++;

		QueryHeap* CurrentQH = CmdContext.RetrieveObject<QueryHeap>(TimestampQueryHeap);
		CmdContext.CommandListHandle->EndQuery(CurrentQH->Heap, D3D12_QUERY_TYPE_TIMESTAMP, QueryStartIndex);
		CmdContext.CommandListHandle.UpdateResidency(TimestampQueryHeapBuffer.GetReference());

		TimestampListHandles[QueryStartIndex] = CmdContext.CommandListHandle;
		bIsTiming = true;
	}
}

/**
 * End a GPU timing measurement.
 * The timing for this particular measurement will be resolved at a later time by the GPU.
 */
void FD3D12BufferedGPUTiming::EndTiming()
{
	// Issue a timestamp query for the 'end' time.
	if (GIsSupported && bIsTiming)
	{
		check(CurrentTimestamp >= 0 && CurrentTimestamp < BufferSize);
		const uint32 QueryStartIndex = GetStartTimestampIndex(CurrentTimestamp);
		const uint32 QueryEndIndex = GetEndTimestampIndex(CurrentTimestamp);
		check(QueryEndIndex == QueryStartIndex + 1);	// Make sure they're adjacent indices.
		FD3D12CommandContext& CmdContext = GetParentAdapter()->GetCurrentDevice()->GetDefaultCommandContext();
		CmdContext.otherWorkCounter += 2;

		QueryHeap* CurrentQH = CmdContext.RetrieveObject<QueryHeap>(TimestampQueryHeap);

		CmdContext.CommandListHandle->EndQuery(CurrentQH->Heap, D3D12_QUERY_TYPE_TIMESTAMP, QueryEndIndex);
		CmdContext.CommandListHandle->ResolveQueryData(CurrentQH->Heap, D3D12_QUERY_TYPE_TIMESTAMP, QueryStartIndex, 2, TimestampQueryHeapBuffer->GetResource(), 8 * QueryStartIndex);
		CmdContext.CommandListHandle.UpdateResidency(TimestampQueryHeapBuffer.GetReference());

		TimestampListHandles[QueryEndIndex] = CmdContext.CommandListHandle;
		NumIssuedTimestamps = FMath::Min<int32>(NumIssuedTimestamps + 1, BufferSize);
		bIsTiming = false;
	}
}

/**
 * Retrieves the most recently resolved timing measurement.
 * The unit is the same as for FPlatformTime::Cycles(). Returns 0 if there are no resolved measurements.
 *
 * @return	Value of the most recently resolved timing, or 0 if no measurements have been resolved by the GPU yet.
 */
uint64 FD3D12BufferedGPUTiming::GetTiming(bool bGetCurrentResultsAndBlock)
{
	FD3D12Device* Device = GetParentAdapter()->GetCurrentDevice();

	if (GIsSupported)
	{
		check(CurrentTimestamp >= 0 && CurrentTimestamp < BufferSize);
		uint64 StartTime, EndTime;
		static const CD3DX12_RANGE EmptyRange(0, 0);

		FD3D12CommandListManager& CommandListManager = Device->GetCommandListManager();

		int32 TimestampIndex = CurrentTimestamp;
		if (!bGetCurrentResultsAndBlock)
		{
			// Quickly check the most recent measurements to see if any of them has been resolved.  Do not flush these queries.
			for (int32 IssueIndex = 1; IssueIndex < NumIssuedTimestamps; ++IssueIndex)
			{
				const uint32 QueryStartIndex = GetStartTimestampIndex(TimestampIndex);
				const uint32 QueryEndIndex = GetEndTimestampIndex(TimestampIndex);
				const FD3D12CLSyncPoint& StartQuerySyncPoint = TimestampListHandles[QueryStartIndex];
				const FD3D12CLSyncPoint& EndQuerySyncPoint = TimestampListHandles[QueryEndIndex];
				if (EndQuerySyncPoint.IsComplete() && StartQuerySyncPoint.IsComplete())
				{
					// Scope map the result range for read.
					const CD3DX12_RANGE ReadRange(QueryStartIndex * sizeof(uint64), (QueryEndIndex + 1) * sizeof(uint64));
					const FD3D12ScopeMap<uint64> MappedTimestampData(TimestampQueryHeapBuffer, 0, &ReadRange, &EmptyRange /* Not writing any data */);
					StartTime = MappedTimestampData[QueryStartIndex];
					EndTime = MappedTimestampData[QueryEndIndex];
					
					if (EndTime > StartTime)
					{
						return EndTime - StartTime;
					}
				}

				TimestampIndex = (TimestampIndex + BufferSize - 1) % BufferSize;
			}
		}

		if (NumIssuedTimestamps > 0 || bGetCurrentResultsAndBlock)
		{
			// None of the (NumIssuedTimestamps - 1) measurements were ready yet,
			// so check the oldest measurement more thoroughly.
			// This really only happens if occlusion and frame sync event queries are disabled, otherwise those will block until the GPU catches up to 1 frame behind

			const bool bBlocking = (NumIssuedTimestamps == BufferSize) || bGetCurrentResultsAndBlock;
			uint32 IdleStart = FPlatformTime::Cycles();
			double StartTimeoutTime = FPlatformTime::Seconds();

			SCOPE_CYCLE_COUNTER(STAT_RenderQueryResultTime);

			const uint32 QueryStartIndex = GetStartTimestampIndex(TimestampIndex);
			const uint32 QueryEndIndex = GetEndTimestampIndex(TimestampIndex);

			if (bBlocking)
			{
				const FD3D12CLSyncPoint& StartQuerySyncPoint = TimestampListHandles[QueryStartIndex];
				const FD3D12CLSyncPoint& EndQuerySyncPoint = TimestampListHandles[QueryEndIndex];
				if (EndQuerySyncPoint.IsOpen() || StartQuerySyncPoint.IsOpen())
				{
					// Need to submit the open command lists.
					Device->GetDefaultCommandContext().FlushCommands();
				}

				// CPU wait for query results to be ready.
				StartQuerySyncPoint.WaitForCompletion();
				EndQuerySyncPoint.WaitForCompletion();
			}

			GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery] += FPlatformTime::Cycles() - IdleStart;
			GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUQuery]++;

			// Scope map the result range for read.
			const CD3DX12_RANGE ReadRange(QueryStartIndex * sizeof(uint64), (QueryEndIndex + 1) * sizeof(uint64));
			const FD3D12ScopeMap<uint64> MappedTimestampData(TimestampQueryHeapBuffer, 0, &ReadRange, &EmptyRange /* Not writing any data */);
			StartTime = MappedTimestampData[QueryStartIndex];
			EndTime = MappedTimestampData[QueryEndIndex];

			if (EndTime > StartTime)
			{
				return EndTime - StartTime;
			}
		}
	}

	return 0;
}
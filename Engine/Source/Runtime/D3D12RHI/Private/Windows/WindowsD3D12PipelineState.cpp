// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Implementation of D3D12 Pipelinestate related functions

//-----------------------------------------------------------------------------
//	Include Files
//-----------------------------------------------------------------------------
#include "D3D12RHIPrivate.h"

#include <AllowWindowsPlatformTypes.h>
#include <nvapi.h>
#include <HideWindowsPlatformTypes.h>

void FD3D12PipelineStateCache::RebuildFromDiskCache(ID3D12RootSignature* GraphicsRootSignature, ID3D12RootSignature* ComputeRootSignature)
{
	FScopeLock Lock(&CS);

	UNREFERENCED_PARAMETER(GraphicsRootSignature);
	UNREFERENCED_PARAMETER(ComputeRootSignature);

	if (IsInErrorState())
	{
		// TODO: Make sure we clear the disk caches that are in error.
		return;
	}
	// The only time shader code is ever read back is on debug builds
	// when it checks for hash collisions in the PSO map. Therefore
	// there is no point backing the memory on release.
#if UE_BUILD_DEBUG
	static const bool bBackShadersWithSystemMemory = true;
#else
	static const bool bBackShadersWithSystemMemory = false;
#endif

	DiskCaches[PSO_CACHE_GRAPHICS].Reset(FDiskCacheInterface::RESET_TO_FIRST_OBJECT);
	DiskCaches[PSO_CACHE_COMPUTE].Reset(FDiskCacheInterface::RESET_TO_FIRST_OBJECT);
	DiskBinaryCache.Reset(FDiskCacheInterface::RESET_TO_AFTER_LAST_OBJECT); // Reset this one to the end as we always append

	FD3D12Adapter* Adapter = GetParentAdapter();

	const uint32 NumGraphicsPSOs = DiskCaches[PSO_CACHE_GRAPHICS].GetNumPSOs();
	for (uint32 i = 0; i < NumGraphicsPSOs; i++)
	{
		FD3D12LowLevelGraphicsPipelineStateDesc* Desc = nullptr;
		DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&Desc, sizeof(*Desc));
		D3D12_GRAPHICS_PIPELINE_STATE_DESC* PSODesc = &Desc->Desc;

		Desc->pRootSignature = nullptr;
		SIZE_T* RSBlobLength = nullptr;
		DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&RSBlobLength, sizeof(*RSBlobLength));
		if (RSBlobLength && *RSBlobLength > 0)
		{
			FD3D12QuantizedBoundShaderState* QBSS = nullptr;
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&QBSS, sizeof(*QBSS));
			
			FD3D12RootSignatureManager* const RootSignatureManager = GetParentAdapter()->GetRootSignatureManager();
			FD3D12RootSignature* const RootSignature = RootSignatureManager->GetRootSignature(*QBSS);
			PSODesc->pRootSignature = RootSignature->GetRootSignature();
			check(PSODesc->pRootSignature);
		}
		if (PSODesc->InputLayout.NumElements)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->InputLayout.pInputElementDescs, PSODesc->InputLayout.NumElements * sizeof(D3D12_INPUT_ELEMENT_DESC), true);
			for (uint32 j = 0; j < PSODesc->InputLayout.NumElements; j++)
			{
				// Get the Sematic name string
				uint32* stringLength = nullptr;
				DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&stringLength, sizeof(uint32));
				DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->InputLayout.pInputElementDescs[j].SemanticName, *stringLength, true);
			}
		}
		if (PSODesc->StreamOutput.NumEntries)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->StreamOutput.pSODeclaration, PSODesc->StreamOutput.NumEntries * sizeof(D3D12_SO_DECLARATION_ENTRY), true);
			for (uint32 j = 0; j < PSODesc->StreamOutput.NumEntries; j++)
			{
				//Get the Sematic name string
				uint32* stringLength = nullptr;
				DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&stringLength, sizeof(uint32));
				DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->StreamOutput.pSODeclaration[j].SemanticName, *stringLength, true);
			}
		}
		if (PSODesc->StreamOutput.NumStrides)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->StreamOutput.pBufferStrides, PSODesc->StreamOutput.NumStrides * sizeof(uint32), true);
		}
		if (PSODesc->VS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->VS.pShaderBytecode, PSODesc->VS.BytecodeLength, bBackShadersWithSystemMemory);
		}
		if (PSODesc->PS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->PS.pShaderBytecode, PSODesc->PS.BytecodeLength, bBackShadersWithSystemMemory);
		}
		if (PSODesc->DS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->DS.pShaderBytecode, PSODesc->DS.BytecodeLength, bBackShadersWithSystemMemory);
		}
		if (PSODesc->HS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->HS.pShaderBytecode, PSODesc->HS.BytecodeLength, bBackShadersWithSystemMemory);
		}
		if (PSODesc->GS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->GS.pShaderBytecode, PSODesc->GS.BytecodeLength, bBackShadersWithSystemMemory);
		}

		ReadBackShaderBlob(*PSODesc, PSO_CACHE_GRAPHICS);

		if (!DiskCaches[PSO_CACHE_GRAPHICS].IsInErrorState())
		{
			Desc->CombinedHash = FD3D12PipelineStateCache::HashPSODesc(*Desc);

			FD3D12PipelineState* const NewPipelineState = new FD3D12PipelineState(Adapter);
			LowLevelGraphicsPipelineStateCache.Add(*Desc, NewPipelineState);
			NewPipelineState->CreateAsync(GraphicsPipelineCreationArgs(Desc, PipelineLibrary));
		}
		else
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("PSO Cache read error!"));
			break;
		}
	}

	const uint32 NumComputePSOs = DiskCaches[PSO_CACHE_COMPUTE].GetNumPSOs();
	for (uint32 i = 0; i < NumComputePSOs; i++)
	{
		FD3D12ComputePipelineStateDesc* Desc = nullptr;
		DiskCaches[PSO_CACHE_COMPUTE].SetPointerAndAdvanceFilePosition((void**)&Desc, sizeof(*Desc));
		D3D12_COMPUTE_PIPELINE_STATE_DESC* PSODesc = &Desc->Desc;

		Desc->pRootSignature = nullptr;
		SIZE_T* RSBlobLength = nullptr;
		DiskCaches[PSO_CACHE_COMPUTE].SetPointerAndAdvanceFilePosition((void**)&RSBlobLength, sizeof(*RSBlobLength));
		if (RSBlobLength > 0)
		{
			FD3D12QuantizedBoundShaderState* QBSS = nullptr;
			DiskCaches[PSO_CACHE_COMPUTE].SetPointerAndAdvanceFilePosition((void**)&QBSS, sizeof(*QBSS));

			FD3D12RootSignatureManager* const RootSignatureManager = GetParentAdapter()->GetRootSignatureManager();
			FD3D12RootSignature* const RootSignature = RootSignatureManager->GetRootSignature(*QBSS);
			PSODesc->pRootSignature = RootSignature->GetRootSignature();
			check(PSODesc->pRootSignature);
		}
		if (PSODesc->CS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_COMPUTE].SetPointerAndAdvanceFilePosition((void**)&PSODesc->CS.pShaderBytecode, PSODesc->CS.BytecodeLength, bBackShadersWithSystemMemory);
		}

		ReadBackShaderBlob(*PSODesc, PSO_CACHE_COMPUTE);

		if (!DiskCaches[PSO_CACHE_COMPUTE].IsInErrorState())
		{
			Desc->CombinedHash = FD3D12PipelineStateCache::HashPSODesc(*Desc);

			FD3D12PipelineState* const NewPipelineState = new FD3D12PipelineState(Adapter);
			ComputePipelineStateCache.Add(*Desc, NewPipelineState);
			NewPipelineState->CreateAsync(ComputePipelineCreationArgs(Desc, PipelineLibrary));
		}
		else
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("PSO Cache read error!"));
			break;
		}
	}
}

FD3D12PipelineState* FD3D12PipelineStateCache::FindGraphics(FD3D12HighLevelGraphicsPipelineStateDesc* Desc)
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12ApplyStateFindPSOTime);

	FScopeLock Lock(&CS);

#if UE_BUILD_DEBUG
	++GraphicsCacheRequestCount;
#endif

	Desc->CombinedHash = FD3D12PipelineStateCache::HashPSODesc(*Desc);

	const uint64 BSSUniqueID = Desc->BoundShaderState ? Desc->BoundShaderState->UniqueID : 0;
	TPair<FD3D12PipelineState*, uint64>* HighLevelCacheEntry = HighLevelGraphicsPipelineStateCache.Find(*Desc);
	if (HighLevelCacheEntry && HighLevelCacheEntry->Value == BSSUniqueID)
	{
#if UE_BUILD_DEBUG
		++HighLevelCacheFulfillCount; // No low-level cache hit
#endif
		return HighLevelCacheEntry->Key;
	}

	FD3D12LowLevelGraphicsPipelineStateDesc LowLevelDesc;
	Desc->GetLowLevelDesc(LowLevelDesc);

	//TODO: for now PSOs will be created on every node of the LDA chain
	LowLevelDesc.Desc.NodeMask = GetParentAdapter()->ActiveGPUMask();

	FD3D12PipelineState* const PSO = FindGraphicsLowLevel(&LowLevelDesc);

	if (HighLevelCacheEntry)
	{
#if UE_BUILD_DEBUG
		++HighLevelCacheStaleCount; // High-level cache hit, but was stale due to BSS memory re-use
#endif
		HighLevelCacheEntry->Key = PSO;
		HighLevelCacheEntry->Value = BSSUniqueID;
	}
	else
	{
#if UE_BUILD_DEBUG
		++HighLevelCacheMissCount; // No high-level cache hit
#endif
		HighLevelCacheEntry = &HighLevelGraphicsPipelineStateCache.Add(
			*Desc,
			TPair<FD3D12PipelineState*, uint64>(PSO, BSSUniqueID)
			);
	}

	return HighLevelCacheEntry->Key;
}

FD3D12PipelineState* FD3D12PipelineStateCache::FindGraphicsLowLevel(FD3D12LowLevelGraphicsPipelineStateDesc* Desc)
{
	// Lock already taken by high level find
	Desc->CombinedHash = FD3D12PipelineStateCache::HashPSODesc(*Desc);

	FD3D12PipelineState** const PSO = LowLevelGraphicsPipelineStateCache.Find(*Desc);
	if (PSO)
	{
		ID3D12PipelineState* APIPso = (*PSO)->GetPipelineState();
		if (APIPso)
		{
			return *PSO;
		}
		else
		{
			check(false);
			UE_LOG(LogD3D12RHI, Warning, TEXT("PSO re-creation failed. Most likely on disk descriptor corruption."));
			for (uint32 i = 0; i < NUM_PSO_CACHE_TYPES; i++)
			{
				DiskCaches[i].ClearDiskCache();
			}
		}
	}

	return Add(GraphicsPipelineCreationArgs(Desc, PipelineLibrary));
}

FD3D12PipelineState* FD3D12PipelineStateCache::FindCompute(FD3D12ComputePipelineStateDesc* Desc)
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12ApplyStateFindPSOTime);

	FScopeLock Lock(&CS);

	//TODO: for now PSOs will be created on every node of the LDA chain
	Desc->Desc.NodeMask = GetParentAdapter()->ActiveGPUMask();

	Desc->CombinedHash = FD3D12PipelineStateCache::HashPSODesc(*Desc);

	FD3D12PipelineState** const PSO = ComputePipelineStateCache.Find(*Desc);
	if (PSO)
	{
		ID3D12PipelineState* APIPso = (*PSO)->GetPipelineState();
		if (APIPso)
		{
			return *PSO;
		}
		else
		{
			check(false);
			UE_LOG(LogD3D12RHI, Warning, TEXT("PSO re-creation failed. Most likely on disk descriptor corruption."));
			for (uint32 i = 0; i < NUM_PSO_CACHE_TYPES; i++)
			{
				DiskCaches[i].ClearDiskCache();
			}
		}
	}

	return Add(ComputePipelineCreationArgs(Desc, PipelineLibrary));
}

FD3D12PipelineState* FD3D12PipelineStateCache::Add(const GraphicsPipelineCreationArgs& Args)
{
	FScopeLock Lock(&CS);

#if UE_BUILD_DEBUG
	check(LowLevelGraphicsPipelineStateCache.Find(*Args.Args.Desc) == nullptr);
#endif

	FD3D12PipelineState* const NewPipelineState = new FD3D12PipelineState(GetParentAdapter());
	LowLevelGraphicsPipelineStateCache.Add(*Args.Args.Desc, NewPipelineState);
	NewPipelineState->Create(Args);

	ID3D12PipelineState* APIPso = NewPipelineState->GetPipelineState();
	if (APIPso == nullptr)
	{
		UE_LOG(LogD3D12RHI, Warning, TEXT("Runtime PSO creation failed."));
		return nullptr;
	}

	const D3D12_GRAPHICS_PIPELINE_STATE_DESC &PsoDesc = Args.Args.Desc->Desc;

	//TODO: Optimize by only storing unique pointers
	// NVCHANGE_BEGIN: Add VXGI
	// Do not write the PSOs with NV extensions because the extension descriptors are opaque, their size is generally unknown,
	// and they contain pointers inside. Writing them to disk requires a deep copy.
	if (!DiskCaches[PSO_CACHE_GRAPHICS].IsInErrorState() && Args.Args.Desc->NumNvidiaShaderExtensions == 0)
	// NVCHANGE_END: Add VXGI
	{
		DiskCaches[PSO_CACHE_GRAPHICS].BeginAppendPSO();
		DiskCaches[PSO_CACHE_GRAPHICS].AppendData(Args.Args.Desc, sizeof(*Args.Args.Desc));

		ID3DBlob* const pRSBlob = Args.Args.Desc->pRootSignature ? Args.Args.Desc->pRootSignature->GetRootSignatureBlob() : nullptr;
		const SIZE_T RSBlobLength = pRSBlob ? pRSBlob->GetBufferSize() : 0;
		DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)(&RSBlobLength), sizeof(RSBlobLength));
		if (RSBlobLength > 0)
		{
			// Save the quantized bound shader state so we can use the root signature manager to deduplicate and handle root signature creation.
			check(Args.Args.Desc->pRootSignature->GetRootSignature() == PsoDesc.pRootSignature);
			FD3D12RootSignatureManager* const RootSignatureManager = GetParentAdapter()->GetRootSignatureManager();
			const FD3D12QuantizedBoundShaderState QBSS = RootSignatureManager->GetQuantizedBoundShaderState(Args.Args.Desc->pRootSignature);
			DiskCaches[PSO_CACHE_GRAPHICS].AppendData(&QBSS, sizeof(QBSS));
		}
		if (PsoDesc.InputLayout.NumElements)
		{
			//Save the layout structs
			DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)PsoDesc.InputLayout.pInputElementDescs, PsoDesc.InputLayout.NumElements * sizeof(D3D12_INPUT_ELEMENT_DESC));
			for (uint32 i = 0; i < PsoDesc.InputLayout.NumElements; i++)
			{
				//Save the Sematic name string
				uint32 stringLength = (uint32)strnlen_s(PsoDesc.InputLayout.pInputElementDescs[i].SemanticName, IL_MAX_SEMANTIC_NAME);
				stringLength++; // include the NULL char
				DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)&stringLength, sizeof(stringLength));
				DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)PsoDesc.InputLayout.pInputElementDescs[i].SemanticName, stringLength);
			}
		}
		if (PsoDesc.StreamOutput.NumEntries)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)&PsoDesc.StreamOutput.pSODeclaration, PsoDesc.StreamOutput.NumEntries * sizeof(D3D12_SO_DECLARATION_ENTRY));
			for (uint32 i = 0; i < PsoDesc.StreamOutput.NumEntries; i++)
			{
				//Save the Sematic name string
				uint32 stringLength = (uint32)strnlen_s(PsoDesc.StreamOutput.pSODeclaration[i].SemanticName, IL_MAX_SEMANTIC_NAME);
				stringLength++; // include the NULL char
				DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)&stringLength, sizeof(stringLength));
				DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)PsoDesc.StreamOutput.pSODeclaration[i].SemanticName, stringLength);
			}
		}
		if (PsoDesc.StreamOutput.NumStrides)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)&PsoDesc.StreamOutput.pBufferStrides, PsoDesc.StreamOutput.NumStrides * sizeof(uint32));
		}
		if (PsoDesc.VS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)PsoDesc.VS.pShaderBytecode, PsoDesc.VS.BytecodeLength);
		}
		if (PsoDesc.PS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)PsoDesc.PS.pShaderBytecode, PsoDesc.PS.BytecodeLength);
		}
		if (PsoDesc.DS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)PsoDesc.DS.pShaderBytecode, PsoDesc.DS.BytecodeLength);
		}
		if (PsoDesc.HS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)PsoDesc.HS.pShaderBytecode, PsoDesc.HS.BytecodeLength);
		}
		if (PsoDesc.GS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)PsoDesc.GS.pShaderBytecode, PsoDesc.GS.BytecodeLength);
		}

		WriteOutShaderBlob(PSO_CACHE_GRAPHICS, APIPso);

		DiskCaches[PSO_CACHE_GRAPHICS].Flush();
	}

	return NewPipelineState;
}

FD3D12PipelineState* FD3D12PipelineStateCache::Add(const ComputePipelineCreationArgs& Args)
{
	FScopeLock Lock(&CS);

	FD3D12PipelineState* const NewPipelineState = new FD3D12PipelineState(GetParentAdapter());
	ComputePipelineStateCache.Add(*Args.Args.Desc, NewPipelineState);
	NewPipelineState->Create(Args);

	ID3D12PipelineState* APIPso = NewPipelineState->GetPipelineState();
	if (APIPso == nullptr)
	{
		UE_LOG(LogD3D12RHI, Warning, TEXT("Runtime PSO creation failed."));
		return nullptr;
	}

	const D3D12_COMPUTE_PIPELINE_STATE_DESC &PsoDesc = Args.Args.Desc->Desc;

	if (!DiskCaches[PSO_CACHE_COMPUTE].IsInErrorState())
	{
		DiskCaches[PSO_CACHE_COMPUTE].BeginAppendPSO();
		DiskCaches[PSO_CACHE_COMPUTE].AppendData(Args.Args.Desc, sizeof(*Args.Args.Desc));

		ID3DBlob* const pRSBlob = Args.Args.Desc->pRootSignature ? Args.Args.Desc->pRootSignature->GetRootSignatureBlob() : nullptr;
		const SIZE_T RSBlobLength = pRSBlob ? pRSBlob->GetBufferSize() : 0;
		DiskCaches[PSO_CACHE_COMPUTE].AppendData((void*)(&RSBlobLength), sizeof(RSBlobLength));
		if (RSBlobLength > 0)
		{
			// Save the quantized bound shader state so we can use the root signature manager to deduplicate and handle root signature creation.
			CA_SUPPRESS(6011);
			check(Args.Args.Desc->pRootSignature->GetRootSignature() == PsoDesc.pRootSignature);
			FD3D12RootSignatureManager* const RootSignatureManager = GetParentAdapter()->GetRootSignatureManager();
			const FD3D12QuantizedBoundShaderState QBSS = RootSignatureManager->GetQuantizedBoundShaderState(Args.Args.Desc->pRootSignature);
			DiskCaches[PSO_CACHE_COMPUTE].AppendData(&QBSS, sizeof(QBSS));
		}
		if (PsoDesc.CS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_COMPUTE].AppendData((void*)PsoDesc.CS.pShaderBytecode, PsoDesc.CS.BytecodeLength);
		}

		WriteOutShaderBlob(PSO_CACHE_COMPUTE, APIPso);

		DiskCaches[PSO_CACHE_COMPUTE].Flush();
	}

	return NewPipelineState;
}

void FD3D12PipelineStateCache::WriteOutShaderBlob(PSO_CACHE_TYPE Cache, ID3D12PipelineState* APIPso)
{
	if (UseCachedBlobs())
	{
		TRefCountPtr<ID3DBlob> cachedBlob;
		HRESULT result = APIPso->GetCachedBlob(cachedBlob.GetInitReference());
		VERIFYD3D12RESULT(result);
		if (SUCCEEDED(result))
		{
			SIZE_T bufferSize = cachedBlob->GetBufferSize();

			SIZE_T currentOffset = DiskBinaryCache.GetCurrentOffset();
			DiskBinaryCache.BeginAppendPSO();
			DiskBinaryCache.AppendData(cachedBlob->GetBufferPointer(), bufferSize);

			DiskCaches[Cache].AppendData(&currentOffset, sizeof(currentOffset));
			DiskCaches[Cache].AppendData(&bufferSize, sizeof(bufferSize));

			DriverShaderBlobs++;

			DiskBinaryCache.Flush();
		}
		else
		{
			check(false);
			SIZE_T bufferSize = 0;
			DiskCaches[Cache].AppendData(&bufferSize, sizeof(bufferSize));
			DiskCaches[Cache].AppendData(&bufferSize, sizeof(bufferSize));
		}
	}
	else
	{
		SIZE_T bufferSize = 0;
		DiskCaches[Cache].AppendData(&bufferSize, sizeof(bufferSize));
		DiskCaches[Cache].AppendData(&bufferSize, sizeof(bufferSize));
	}
}

void FD3D12PipelineStateCache::Close()
{
	FScopeLock Lock(&CS);

	DiskCaches[PSO_CACHE_GRAPHICS].Reset(FDiskCacheInterface::RESET_TO_FIRST_OBJECT);
	DiskCaches[PSO_CACHE_COMPUTE].Reset(FDiskCacheInterface::RESET_TO_FIRST_OBJECT);
	DiskBinaryCache.Reset(FDiskCacheInterface::RESET_TO_AFTER_LAST_OBJECT);


	DiskCaches[PSO_CACHE_GRAPHICS].Close();
	DiskCaches[PSO_CACHE_COMPUTE].Close();

	TArray<BYTE> LibraryData;
	const bool bOverwriteExistingPipelineLibrary = true;
	if (UsePipelineLibrary() && bOverwriteExistingPipelineLibrary)
	{
		// Serialize the Library.
		const SIZE_T LibrarySize = PipelineLibrary->GetSerializedSize();
		if (LibrarySize)
		{
			LibraryData.AddUninitialized(LibrarySize);
			check(LibraryData.Num() == LibrarySize);

			UE_LOG(LogD3D12RHI, Log, TEXT("Serializing Pipeline Library to disk (%llu KiB containing %u PSOs)"), LibrarySize / 1024ll, DriverShaderBlobs);
			VERIFYD3D12RESULT(PipelineLibrary->Serialize(LibraryData.GetData(), LibrarySize));

			// Write the Library to disk (overwrite existing data).
			DiskBinaryCache.Reset(FDiskCacheInterface::RESET_TO_FIRST_OBJECT);
			const bool bSuccess = DiskBinaryCache.AppendData(LibraryData.GetData(), LibrarySize);
			UE_CLOG(!bSuccess, LogD3D12RHI, Warning, TEXT("Failed to write Pipeline Library to disk."));
		}
	}

	DiskBinaryCache.Close();

	CleanupPipelineStateCaches();
}

void FD3D12PipelineStateCache::Init(FString &GraphicsCacheFilename, FString &ComputeCacheFilename, FString &DriverBlobFilename)
{
	FScopeLock Lock(&CS);

	DiskCaches[PSO_CACHE_GRAPHICS].Init(GraphicsCacheFilename);
	DiskCaches[PSO_CACHE_COMPUTE].Init(ComputeCacheFilename);
	DiskBinaryCache.Init(DriverBlobFilename);

	DiskCaches[PSO_CACHE_GRAPHICS].Reset(FDiskCacheInterface::RESET_TO_FIRST_OBJECT);
	DiskCaches[PSO_CACHE_COMPUTE].Reset(FDiskCacheInterface::RESET_TO_FIRST_OBJECT);
	DiskBinaryCache.Reset(FDiskCacheInterface::RESET_TO_AFTER_LAST_OBJECT);

	DriverShaderBlobs = DiskBinaryCache.GetNumPSOs();

	if (bUseAPILibaries)
	{
		// Create a pipeline library if the system supports it.
		ID3D12Device1* pDevice1 = GetParentAdapter()->GetD3DDevice1();
		if (pDevice1)
		{
			const SIZE_T LibrarySize = DiskBinaryCache.GetSizeInBytes();
			void* pLibraryBlob = LibrarySize ? DiskBinaryCache.GetDataAtStart() : nullptr;

			if (pLibraryBlob)
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("Creating Pipeline Library from existing disk cache (%llu KiB)."), LibrarySize / 1024ll);
			}
			else
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("Creating new Pipeline Library."));
			}

			const HRESULT HResult = pDevice1->CreatePipelineLibrary(pLibraryBlob, LibrarySize, IID_PPV_ARGS(PipelineLibrary.GetInitReference()));

			// E_INVALIDARG if the blob is corrupted or unrecognized. D3D12_ERROR_DRIVER_VERSION_MISMATCH if the provided data came from 
			// an old driver or runtime. D3D12_ERROR_ADAPTER_NOT_FOUND if the data came from different hardware.
			if (DXGI_ERROR_UNSUPPORTED == HResult)
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("The driver doesn't support Pipeline Libraries."));
			}
			else if (FAILED(HResult))
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("Create Pipeline Library failed. Perhaps the Library has stale PSOs for the current HW or driver. Clearing the disk cache and trying again..."));

				// TODO: In the case of D3D12_ERROR_ADAPTER_NOT_FOUND, we don't really need to clear the cache, we just need to try another one. We should really have a cache per adapter.
				DiskBinaryCache.ClearDiskCache();
				DiskBinaryCache.Init(DriverBlobFilename);
				check(DiskBinaryCache.GetSizeInBytes() == 0);

				VERIFYD3D12RESULT(pDevice1->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(PipelineLibrary.GetInitReference())));
			}

			SetName(PipelineLibrary, L"Pipeline Library");
		}
	}
}

bool FD3D12PipelineStateCache::IsInErrorState() const
{
	return (DiskCaches[PSO_CACHE_GRAPHICS].IsInErrorState() ||
		DiskCaches[PSO_CACHE_COMPUTE].IsInErrorState() ||
		(bUseAPILibaries && DiskBinaryCache.IsInErrorState()));
}

FD3D12PipelineStateCache::FD3D12PipelineStateCache(FD3D12Adapter* InParent) :
	DriverShaderBlobs(0)
	, FD3D12PipelineStateCacheBase(InParent)
{
}

FD3D12PipelineStateCache::~FD3D12PipelineStateCache()
{
}


// Thread-safe create graphics/compute pipeline state. Conditionally load/store the PSO using a Pipeline Library.
template <typename TDesc>
ID3D12PipelineState* CreatePipelineState(ID3D12Device* Device, const TDesc* Desc, ID3D12PipelineLibrary* Library, const TCHAR* const Name)
{
	ID3D12PipelineState* PSO = nullptr;
	if (Library)
	{
		// Try to load the PSO from the library.
		check(Name);
		HRESULT HResult = (Library->*TPSOFunctionMap<TDesc>::GetLoadPipeline())(Name, Desc, IID_PPV_ARGS(&PSO));
		if (E_INVALIDARG == HResult)
		{
			// The name doesn't exist or the input desc doesn't match the data in the library, just create the PSO.
			{
				SCOPE_CYCLE_COUNTER(STAT_D3D12PSOCreateTime);
				VERIFYD3D12RESULT((Device->*TPSOFunctionMap<TDesc>::GetCreatePipelineState())(Desc, IID_PPV_ARGS(&PSO)));
			}

			// Try to save the PSO to the library for another time.
			check(PSO);
			HResult = Library->StorePipeline(Name, PSO);
			if (E_INVALIDARG != HResult)
			{
				// E_INVALIDARG means the name already exists in the library. Since the name is based on the hash, this is a hash collision.
				// We ignore E_INVALIDARG because we just create PSO's if they don't exist in the library.
				VERIFYD3D12RESULT(HResult);
			}
		}
		else
		{
			VERIFYD3D12RESULT(HResult);
		}
	}
	else
	{
		SCOPE_CYCLE_COUNTER(STAT_D3D12PSOCreateTime);
		VERIFYD3D12RESULT((Device->*TPSOFunctionMap<TDesc>::GetCreatePipelineState())(Desc, IID_PPV_ARGS(&PSO)));
	}

	check(PSO);
	return PSO;
}

void FD3D12PipelineState::Create(const ComputePipelineCreationArgs& InCreationArgs)
{
	check(PipelineState.GetReference() == nullptr);
	PipelineState = CreatePipelineState(GetParentAdapter()->GetD3DDevice(), &InCreationArgs.Args.Desc->Desc, InCreationArgs.Args.Library, InCreationArgs.Args.Desc->GetName().GetCharArray().GetData());
}

void FD3D12PipelineState::CreateAsync(const ComputePipelineCreationArgs& InCreationArgs)
{
	// NVCHANGE_BEGIN: Add VXGI
	// Workaround for a stack overflow crash in nvwgf2umx.dll
	{
		Create(InCreationArgs);
		return;
	}
	// NVCHANGE_END: Add VXGI
	check(PipelineState.GetReference() == nullptr && Worker == nullptr);
	Worker = new FAsyncTask<FD3D12PipelineStateWorker>(GetParentAdapter(), InCreationArgs);
	if (Worker)
	{
		Worker->StartBackgroundTask();
	}
}

void FD3D12PipelineState::Create(const GraphicsPipelineCreationArgs& InCreationArgs)
{
	// NVCHANGE_BEGIN: Add VXGI
	if (InCreationArgs.Args.Desc->NumNvidiaShaderExtensions)
	{
		NvAPI_Status status = NvAPI_D3D12_CreateGraphicsPipelineState(
			GetParentAdapter()->GetD3DDevice(),
			&InCreationArgs.Args.Desc->Desc,
			InCreationArgs.Args.Desc->NumNvidiaShaderExtensions,
			(const NVAPI_D3D12_PSO_EXTENSION_DESC**)InCreationArgs.Args.Desc->NvidiaShaderExtensions,
			PipelineState.GetInitReference());

		check(status == NVAPI_OK);
		check(PipelineState);

		return;
	}
	// NVCHANGE_END: Add VXGI

	check(PipelineState.GetReference() == nullptr);
	PipelineState = CreatePipelineState(GetParentAdapter()->GetD3DDevice(), &InCreationArgs.Args.Desc->Desc, InCreationArgs.Args.Library, InCreationArgs.Args.Desc->GetName().GetCharArray().GetData());
}

void FD3D12PipelineState::CreateAsync(const GraphicsPipelineCreationArgs& InCreationArgs)
{
	// NVCHANGE_BEGIN: Add VXGI
	// The condition is commented out as a workaround for a stack overflow crash in nvwgf2umx.dll
	// Otherwise it is there just because workers do not implement the NV extensions yet.
	//if (Desc->NumNvidiaShaderExtensions)
	{
		Create(InCreationArgs);
		return;
	}
	// NVCHANGE_END: Add VXGI

	check(PipelineState.GetReference() == nullptr && Worker == nullptr);
	Worker = new FAsyncTask<FD3D12PipelineStateWorker>(GetParentAdapter(), InCreationArgs);
	if (Worker)
	{
		Worker->StartBackgroundTask();
	}
}

void FD3D12PipelineStateWorker::DoWork()
{
	if (bIsGraphics)
	{
		PSO = CreatePipelineState(GetParentAdapter()->GetD3DDevice(), &CreationArgs.GraphicsArgs.Desc->Desc, CreationArgs.GraphicsArgs.Library, CreationArgs.GraphicsArgs.Desc->GetName().GetCharArray().GetData());
	}
	else
	{
		PSO = CreatePipelineState(GetParentAdapter()->GetD3DDevice(), &CreationArgs.ComputeArgs.Desc->Desc, CreationArgs.ComputeArgs.Library, CreationArgs.ComputeArgs.Desc->GetName().GetCharArray().GetData());
	}
}
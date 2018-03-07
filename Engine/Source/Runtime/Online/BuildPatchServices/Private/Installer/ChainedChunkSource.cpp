// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Installer/ChainedChunkSource.h"

namespace BuildPatchServices
{
	class FChainedChunkSource : public IChainedChunkSource
	{
	public:
		FChainedChunkSource(TArray<IChunkSource*> InChunkSources);
		~FChainedChunkSource();

		// IChunkSource interface begin.
		virtual IChunkDataAccess* Get(const FGuid& DataId) override;
		virtual TSet<FGuid> AddRuntimeRequirements(TSet<FGuid> NewRequirements) override;
		virtual void SetUnavailableChunksCallback(TFunction<void(TSet<FGuid>)> Callback) override;
		// IChunkSource interface end.

	private:
		TSet<FGuid> CascadeRuntimeRequirements(TSet<FGuid> NewRequirements, int32 StartingIdx = 0);

	private:
		TArray<IChunkSource*> ChunkSources;
		TFunction<void(TSet<FGuid>)> UnavailableChunksCallback;
	};

	FChainedChunkSource::FChainedChunkSource(TArray<IChunkSource*> InChunkSources)
		: ChunkSources(MoveTemp(InChunkSources))
	{
		for (int32 ChunkSourceIdx = 0; ChunkSourceIdx < ChunkSources.Num() - 1; ++ChunkSourceIdx)
		{
			check(ChunkSources[ChunkSourceIdx] != nullptr);
			int32 NextChunkSourceIdx = ChunkSourceIdx + 1;
			ChunkSources[ChunkSourceIdx]->SetUnavailableChunksCallback([this, NextChunkSourceIdx](TSet<FGuid> NewRequirements)
			{
				NewRequirements = CascadeRuntimeRequirements(MoveTemp(NewRequirements), NextChunkSourceIdx);
				if (NewRequirements.Num() > 0 && UnavailableChunksCallback)
				{
					UnavailableChunksCallback(MoveTemp(NewRequirements));
				}
			});
		}
	}

	FChainedChunkSource::~FChainedChunkSource()
	{
	}

	IChunkDataAccess* FChainedChunkSource::Get(const FGuid& DataId)
	{
		IChunkDataAccess* ChunkData = nullptr;
		for (IChunkSource* ChunkSource : ChunkSources)
		{
			ChunkData = ChunkSource->Get(DataId);
			if (ChunkData != nullptr)
			{
				break;
			}
		}
		return ChunkData;
	}

	TSet<FGuid> FChainedChunkSource::AddRuntimeRequirements(TSet<FGuid> NewRequirements)
	{
		return CascadeRuntimeRequirements(MoveTemp(NewRequirements));
	}

	void FChainedChunkSource::SetUnavailableChunksCallback(TFunction<void(TSet<FGuid>)> Callback)
	{
		UnavailableChunksCallback = Callback;
		ChunkSources.Last()->SetUnavailableChunksCallback(Callback);
	}

	TSet<FGuid> FChainedChunkSource::CascadeRuntimeRequirements(TSet<FGuid> NewRequirements, int32 StartingIdx)
	{
		for (int32 ChunkSourceIdx = StartingIdx; ChunkSourceIdx < ChunkSources.Num(); ++ChunkSourceIdx)
		{
			if (NewRequirements.Num() > 0)
			{
				NewRequirements = ChunkSources[ChunkSourceIdx]->AddRuntimeRequirements(MoveTemp(NewRequirements));
			}
		}
		return MoveTemp(NewRequirements);
	}

	IChainedChunkSource* FChainedChunkSourceFactory::Create(TArray<IChunkSource*> ChunkSources)
	{
		check(ChunkSources.Num() > 0);
		return new FChainedChunkSource(MoveTemp(ChunkSources));
	}
}
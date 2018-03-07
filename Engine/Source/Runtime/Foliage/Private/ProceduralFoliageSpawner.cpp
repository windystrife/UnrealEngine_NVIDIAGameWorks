// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ProceduralFoliageSpawner.h"
#include "ProceduralFoliageTile.h"
#include "Misc/FeedbackContext.h"
#include "Serialization/CustomVersion.h"
#include "Async/Async.h"

#define LOCTEXT_NAMESPACE "ProceduralFoliage"

UProceduralFoliageSpawner::UProceduralFoliageSpawner(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TileSize = 10000;	//100 m
	MinimumQuadTreeSize = 100.f;
	NumUniqueTiles = 10;
	RandomSeed = 42;
}

#if WITH_EDITOR
void UProceduralFoliageSpawner::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	bNeedsSimulation = true;
}
#endif

UProceduralFoliageTile* UProceduralFoliageSpawner::CreateTempTile()
{
	UProceduralFoliageTile* TmpTile = NewObject<UProceduralFoliageTile>(this);
	TmpTile->InitSimulation(this, 0);

	return TmpTile;
}

void UProceduralFoliageSpawner::CreateProceduralFoliageInstances()
{
	for (FFoliageTypeObject& FoliageTypeObject : FoliageTypes)
	{
		// Refresh the instances contained in the type objects
		FoliageTypeObject.RefreshInstance();
	}
}

void UProceduralFoliageSpawner::SetClean()
{
	for (FFoliageTypeObject& FoliageTypeObject : FoliageTypes)
	{
		FoliageTypeObject.SetClean();
	}

	bNeedsSimulation = false;
}

// Custom serialization version for all packages containing ProceduralFoliage
struct FProceduralFoliageCustomVersion
{
	enum Type
	{
		// Before any version changes were made in the plugin
		BeforeCustomVersionWasAdded = 0,
		// We serialize the type clean change map to ensure validity
		SerializeTypeMap = 1,
		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FProceduralFoliageCustomVersion() {}
};

const FGuid FProceduralFoliageCustomVersion::GUID(0xaafe32bd, 0x53954c14, 0xb66a5e25, 0x1032d1dd);
// Register the custom version with core
FCustomVersionRegistration GRegisterProceduralFoliageCustomVersion(FProceduralFoliageCustomVersion::GUID, FProceduralFoliageCustomVersion::LatestVersion, TEXT("ProceduralFoliageVer"));

void UProceduralFoliageSpawner::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FProceduralFoliageCustomVersion::GUID);
}

bool UProceduralFoliageSpawner::AnyDirty() const
{
	bool bDirty = bNeedsSimulation;

	if (!bDirty)
	{
		for (const FFoliageTypeObject& FoliageTypeObject : FoliageTypes)
		{
			if (FoliageTypeObject.IsDirty())
			{
				bDirty = true;
				break;
			}
		}
	}
	
	return bDirty;
}

void UProceduralFoliageSpawner::SimulateIfNeeded()
{
	//if (AnyDirty())	@todo: for now we must force simulation every time since precomputed tiles are weak pointers
	{
		Simulate();
	}

}

const UProceduralFoliageTile* UProceduralFoliageSpawner::GetRandomTile(int32 X, int32 Y)
{
	if (PrecomputedTiles.Num())
	{
		// Random stream to use as a hash function
		FRandomStream HashStream;	
		
		HashStream.Initialize(X);
		const float XRand = HashStream.FRand();
		
		HashStream.Initialize(Y);
		const float YRand = HashStream.FRand();
		
		const int32 RandomNumber = (RAND_MAX * XRand / (YRand + 0.01f));
		const int32 Idx = FMath::Clamp(RandomNumber % PrecomputedTiles.Num(), 0, PrecomputedTiles.Num() - 1);
		return PrecomputedTiles[Idx].Get();
	}

	return nullptr;
}

void UProceduralFoliageSpawner::Simulate(int32 NumSteps)
{
	RandomStream.Initialize(RandomSeed);
	CreateProceduralFoliageInstances();

	LastCancel.Increment();

	PrecomputedTiles.Empty();
	TArray<TFuture< UProceduralFoliageTile* >> Futures;

	for (int i = 0; i < NumUniqueTiles; ++i)
	{
		UProceduralFoliageTile* NewTile = NewObject<UProceduralFoliageTile>(this);
		const int32 RandomNumber = GetRandomNumber();
		const int32 LastCancelInit = LastCancel.GetValue();

		Futures.Add(Async<UProceduralFoliageTile*>(EAsyncExecution::ThreadPool, [=]()
		{
			NewTile->Simulate(this, RandomNumber, NumSteps, LastCancelInit);
			return NewTile;
		}));
	}

	const FText StatusMessage = LOCTEXT("SimulateProceduralFoliage", "Simulate ProceduralFoliage...");
	GWarn->BeginSlowTask(StatusMessage, true, true);
	
	const int32 TotalTasks = Futures.Num();

	bool bCancelled = false;

	for (int32 FutureIdx = 0; FutureIdx < Futures.Num(); ++FutureIdx)
	{
		// Sleep for 100ms if not ready. Needed so cancel is responsive.
		while (Futures[FutureIdx].WaitFor(FTimespan::FromMilliseconds(100.0)) == false)
		{
			GWarn->StatusUpdate(FutureIdx, TotalTasks, LOCTEXT("SimulateProceduralFoliage", "Simulate ProceduralFoliage..."));

			if (GWarn->ReceivedUserCancel() && bCancelled == false)
			{
				// Increment the thread-safe counter. Tiles compare against the original count and cancel if different.
				LastCancel.Increment();
				bCancelled = true;
			}
		}

		// Even if canceled, block until all threads have exited safely. This ensures memory isn't GC'd.
		PrecomputedTiles.Add(Futures[FutureIdx].Get());		
	}

	GWarn->EndSlowTask();

	if (bCancelled)
	{
		PrecomputedTiles.Empty();
	}
	else
	{
		SetClean();
	}
}

int32 UProceduralFoliageSpawner::GetRandomNumber()
{
	return RandomStream.FRand() * RAND_MAX;
}

#undef LOCTEXT_NAMESPACE

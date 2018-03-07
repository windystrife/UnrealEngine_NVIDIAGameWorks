// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FoliageStatistics.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "InstancedFoliageActor.h"

//////////////////////////////////////////////////////////////////////////
// UFoliageStatics

UFoliageStatistics::UFoliageStatistics(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UFoliageStatistics::FoliageOverlappingSphereCount(UObject* WorldContextObject, const UStaticMesh* StaticMesh, FVector CenterPosition, float Radius)
{
	int32 Count = 0;
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		const FSphere Sphere(CenterPosition, Radius);

		for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
		{
			AInstancedFoliageActor* IFA = *It;
			if (!IFA->IsPendingKill())
			{
				TArray<const UFoliageType*> FoliageTypes;
				IFA->GetAllFoliageTypesForMesh(StaticMesh, FoliageTypes);

				for (const auto Type : FoliageTypes)
				{
					Count += IFA->GetOverlappingSphereCount(Type, Sphere);
				}
			}
		}
	}

	return Count;
}

int32 UFoliageStatistics::FoliageOverlappingBoxCount(UObject* WorldContextObject, const UStaticMesh* StaticMesh, FBox Box)
{
	int32 Count = 0;
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
		{
			AInstancedFoliageActor* IFA = *It;
			if (!IFA->IsPendingKill())
			{
				TArray<const UFoliageType*> FoliageTypes;
				IFA->GetAllFoliageTypesForMesh(StaticMesh, FoliageTypes);

				for (const auto Type : FoliageTypes)
				{
					Count += IFA->GetOverlappingBoxCount(Type, Box);
				}
			}
		}
	}

	return Count;
}

void UFoliageStatistics::FoliageOverlappingBoxTransforms(UObject* WorldContextObject, const UStaticMesh* StaticMesh, FBox Box, TArray<FTransform>& OutTransforms)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
		{
			AInstancedFoliageActor* IFA = *It;
			if (!IFA->IsPendingKill())
			{
				TArray<const UFoliageType*> FoliageTypes;
				IFA->GetAllFoliageTypesForMesh(StaticMesh, FoliageTypes);

				for (const auto Type : FoliageTypes)
				{
					IFA->GetOverlappingBoxTransforms(Type, Box, OutTransforms);
				}
			}
		}
	}
}

void UFoliageStatistics::FoliageOverlappingMeshCounts_Debug(UObject* WorldContextObject, FVector CenterPosition, float Radius, TMap<UStaticMesh*, int32>& OutMeshCounts)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		const FSphere Sphere(CenterPosition, Radius);

		OutMeshCounts.Reset();
		for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
		{
			AInstancedFoliageActor* IFA = *It;
			if (!IFA->IsPendingKill())
			{
				IFA->GetOverlappingMeshCounts(Sphere, OutMeshCounts);
			}
		}
	}
}

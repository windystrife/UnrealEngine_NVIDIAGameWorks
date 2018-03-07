// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LODCluster.h"
#include "Modules/ModuleManager.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Volume.h"
#include "Components/InstancedStaticMeshComponent.h"


#if WITH_EDITOR
#include "Engine/LODActor.h"
#include "GameFramework/WorldSettings.h"
#include "IHierarchicalLODUtilities.h"
#include "HierarchicalLODUtilitiesModule.h"
#endif // WITH_EDITOR



#define LOCTEXT_NAMESPACE "LODCluster"
#define CM_TO_METER		0.01f
#define METER_TO_CM		100.0f



/** Utility function to calculate overlap of two spheres */
const float CalculateOverlap(const FSphere& ASphere, const float AFillingFactor, const FSphere& BSphere, const float BFillingFactor)
{
	// if it doesn't intersect, return zero 
	if (!ASphere.Intersects(BSphere))
	{
		return 0.f;
	}

	if (ASphere.IsInside(BSphere))
	{
		return ASphere.GetVolume();
	}

	if(BSphere.IsInside(ASphere))
	{
		return BSphere.GetVolume();
	}

	if (ASphere.Equals(BSphere))
	{
		return ASphere.GetVolume();
	}

	float Distance = (ASphere.Center-BSphere.Center).Size();
	check (Distance > 0.f);

	float ARadius = ASphere.W;
	float BRadius = BSphere.W;

	float ACapHeight = (BRadius*BRadius - (ARadius - Distance)*(ARadius - Distance)) / (2*Distance);
	float BCapHeight = (ARadius*ARadius - (BRadius - Distance)*(BRadius - Distance)) / (2*Distance);

	if (ACapHeight<=0.f || BCapHeight<=0.f)
	{
		// it's possible to get cap height to be less than 0 
		// since when we do check intersect, we do have regular tolerance
		return 0.f;		
	}

	float OverlapRadius1 = ((ARadius + BRadius)*(ARadius + BRadius) - Distance*Distance) * (Distance*Distance - (ARadius - BRadius)*(ARadius - BRadius));
	float OverlapRadius2 = 2 * Distance;
	float OverlapRedius = FMath::Sqrt(OverlapRadius1) / OverlapRadius2;
	float OverlapRediusSq = OverlapRedius*OverlapRedius;

	check (OverlapRadius1 >= 0.f);

	float ConstPI = PI/6.0f;
	float AVolume = ConstPI*(3*OverlapRediusSq + ACapHeight*ACapHeight) * ACapHeight;
	float BVolume = ConstPI*(3*OverlapRediusSq + BCapHeight*BCapHeight) * BCapHeight;

	check (AVolume > 0.f &&  BVolume > 0.f);

	float TotalVolume = AFillingFactor*AVolume + BFillingFactor*BVolume;
	return TotalVolume;
}

/** Utility function that calculates filling factor */
const float CalculateFillingFactor(const FSphere& ASphere, const float AFillingFactor, const FSphere& BSphere, const float BFillingFactor)
{
	const float OverlapVolume = CalculateOverlap( ASphere, AFillingFactor, BSphere, BFillingFactor);
	FSphere UnionSphere = ASphere + BSphere;
	// it shouldn't be zero or it should be checked outside
	ensure(UnionSphere.W != 0.f);

	// http://deim.urv.cat/~rivi/pub/3d/icra04b.pdf
	// cost is calculated based on r^3 / filling factor
	// since it subtract by AFillingFactor * 1/2 overlap volume + BfillingFactor * 1/2 overlap volume
	return FMath::Max(0.0f, (AFillingFactor * ASphere.GetVolume() + BFillingFactor * BSphere.GetVolume() - OverlapVolume) / UnionSphere.GetVolume());
}

FLODCluster::FLODCluster(const FLODCluster& Other)
: Actors(Other.Actors)
, Bound(Other.Bound)
, FillingFactor(Other.FillingFactor)
, ClusterCost(Other.ClusterCost)
, bValid(Other.bValid)
{
	
}

FLODCluster::FLODCluster(AActor* Actor1)
: Bound(ForceInit)
, bValid(true)
{
	AddActor(Actor1);
	// calculate new filling factor
	FillingFactor = 1.f;	
	ClusterCost = (Bound.W * Bound.W * Bound.W);
}

FLODCluster::FLODCluster(AActor* Actor1, AActor* Actor2)
: Bound(ForceInit)
, bValid(true)
{
	FSphere Actor1Bound = AddActor(Actor1);
	FSphere Actor2Bound = AddActor(Actor2);
	
	// calculate new filling factor
	FillingFactor = CalculateFillingFactor(Actor1Bound, 1.f, Actor2Bound, 1.f);	
	ClusterCost = ( Bound.W * Bound.W * Bound.W ) / FillingFactor;
}

FLODCluster::FLODCluster()
: Bound(ForceInit)
, bValid(false)
{
	FillingFactor = 1.0f;
	ClusterCost = (Bound.W * Bound.W * Bound.W);
}

FSphere FLODCluster::AddActor(AActor* NewActor)
{
	bValid = true;
	ensure (Actors.Contains(NewActor) == false);
	Actors.Add(NewActor);
	FVector Origin, Extent;

	NewActor->GetActorBounds(false, Origin, Extent);

	// scale 0.01 (change to meter from centimeter)
	FSphere NewBound = FSphere(Origin*CM_TO_METER, Extent.Size()*CM_TO_METER);
	Bound += NewBound;

	return NewBound;
}

FLODCluster FLODCluster::operator+(const FLODCluster& Other) const
{
	FLODCluster UnionCluster(*this);
	UnionCluster.MergeClusters(Other);
	return UnionCluster;
}

FLODCluster FLODCluster::operator+=(const FLODCluster& Other)
{
	MergeClusters(Other);
	return *this;
}

FLODCluster FLODCluster::operator-(const FLODCluster& Other) const
{
	FLODCluster Cluster(*this);
	Cluster.SubtractCluster(Other);
	return *this;
}

FLODCluster FLODCluster::operator-=(const FLODCluster& Other)
{
	SubtractCluster(Other);
	return *this;
}

FLODCluster& FLODCluster::operator=(const FLODCluster& Other)
{
	this->bValid		= Other.bValid;
	this->Actors		= Other.Actors;
	this->Bound			= Other.Bound;
	this->FillingFactor = Other.FillingFactor;	
	this->ClusterCost = Other.ClusterCost;

	return *this;
}

void FLODCluster::MergeClusters(const FLODCluster& Other)
{
	// please note that when merge, we merge two boxes from each cluster, not exactly all actors' bound
	// have to recalculate filling factor and bound based on cluster data
	FillingFactor = CalculateFillingFactor(Bound, FillingFactor, Other.Bound, Other.FillingFactor);
	Bound += Other.Bound;	

	ClusterCost = ( Bound.W * Bound.W * Bound.W ) / FillingFactor;
	

	for (auto& Actor: Other.Actors)
	{
		Actors.AddUnique(Actor);
	}

	if (Actors.Num() > 0)
	{
		bValid = true;
	}
}

void FLODCluster::SubtractCluster(const FLODCluster& Other)
{
	for(int32 ActorId=0; ActorId<Actors.Num(); ++ActorId)
	{
		if (Other.Actors.Contains(Actors[ActorId]))
		{
			Actors.RemoveAt(ActorId);
			--ActorId;
		}
	}

	TArray<AActor*> NewActors = Actors;
	Actors.Empty();
	// need to recalculate parameter
	if (NewActors.Num() == 0)
	{
		Invalidate();
	}
	else if (NewActors.Num() == 1)
	{
		Bound = FSphere(ForceInitToZero);
		AddActor(NewActors[0]);
		FillingFactor = 1.f;
		ClusterCost = ( Bound.W * Bound.W * Bound.W ) / FillingFactor;
	}
	else if (NewActors.Num() >= 2)
	{
		Bound = FSphere(ForceInit);

		FSphere Actor1Bound = AddActor(NewActors[0]);
		FSphere Actor2Bound = AddActor(NewActors[1]);

		// calculate new filling factor
		FillingFactor = CalculateFillingFactor(Actor1Bound, 1.f, Actor2Bound, 1.f);

		// if more actors, we add them manually
		for (int32 ActorId=2; ActorId<NewActors.Num(); ++ActorId)
		{
			// if not contained, it shouldn't be
			check (!Actors.Contains(NewActors[ActorId]));

			FSphere NewBound = AddActor(NewActors[ActorId]);
			FillingFactor = CalculateFillingFactor(NewBound, 1.f, Bound, FillingFactor);
			Bound += NewBound;
		}

		ClusterCost = ( Bound.W * Bound.W * Bound.W ) / FillingFactor;
	}
}


ALODActor* FLODCluster::BuildActor(ULevel* InLevel, const int32 LODIdx, const bool bCreateMeshes)
{
	ALODActor* NewActor = nullptr;

	if (InLevel && InLevel->GetWorld())
	{
		// create asset using Actors
		const FHierarchicalSimplification& LODSetup = InLevel->GetWorld()->GetWorldSettings()->HierarchicalLODSetup[LODIdx];

		// Retrieve draw distance for current and next LOD level
		const int32 LODCount = InLevel->GetWorld()->GetWorldSettings()->HierarchicalLODSetup.Num();

		// Where generated assets will be stored
		FHierarchicalLODUtilitiesModule& Module = FModuleManager::LoadModuleChecked<FHierarchicalLODUtilitiesModule>("HierarchicalLODUtilities");
		IHierarchicalLODUtilities* Utilities = Module.GetUtilities();
 	
		TArray<UStaticMeshComponent*> AllComponents;
		for (auto& Actor : Actors)
		{
			TArray<UStaticMeshComponent*> Components;

			if (Actor->IsA<ALODActor>())
			{
				Utilities->ExtractStaticMeshComponentsFromLODActor(Actor, Components);
			}
			else
			{
				Actor->GetComponents<UStaticMeshComponent>(Components);
			}

			// TODO: support instanced static meshes
			Components.RemoveAll([](UStaticMeshComponent* Val){ return Val->IsA(UInstancedStaticMeshComponent::StaticClass()); });

			AllComponents.Append(Components);
		}

		if (AllComponents.Num())
		{
			// Create LOD Actor
			UWorld* LevelWorld = Cast<UWorld>(InLevel->GetOuter());
			check(LevelWorld);

			FTransform Transform;
			NewActor = LevelWorld->SpawnActor<ALODActor>(ALODActor::StaticClass(), Transform);
			NewActor->LODLevel = LODIdx + 1;
			NewActor->LODDrawDistance = 0.0f;

			// now set as parent
			for (auto& Actor : Actors)
			{
				NewActor->AddSubActor(Actor);
			}

			// Mark dirty according to whether or not this is a preview build
			NewActor->SetIsDirty(!bCreateMeshes);

			if (bCreateMeshes)
			{
				UPackage* AssetsOuter = Utilities->CreateOrRetrieveLevelHLODPackage(InLevel);
				checkf(AssetsOuter != nullptr, TEXT("Failed to created outer for generated HLOD assets"));
				Utilities->BuildStaticMeshForLODActor(NewActor, AssetsOuter, LODSetup);
			}
			NewActor->PostEditChange();
		}

		
	}

	return NewActor;
}


bool FLODCluster::Contains(FLODCluster& Other) const
{
	if (IsValid() && Other.IsValid())
	{
		for(auto& Actor: Other.Actors)
		{
			if(Actors.Contains(Actor))
			{
				return true;
			}
		}
	}

	return false;
}

FString FLODCluster::ToString() const
{
	FString ActorList;
	for (auto& Actor: Actors)
	{
		ActorList += Actor->GetActorLabel();
		ActorList += ", ";
	}

	return FString::Printf(TEXT("ActorNum(%d), Actor List (%s)"), Actors.Num(), *ActorList);
}

#undef LOCTEXT_NAMESPACE 

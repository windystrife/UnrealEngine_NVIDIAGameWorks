// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class ALODActor;
class ULevel;
class UStaticMeshComponent;

/**
 *
 *	This is a LOD cluster struct that holds list of actors with relevant information
 *
 *	http://deim.urv.cat/~rivi/pub/3d/icra04b.pdf
 *
 *	This is used by Hierarchical LOD Builder to generates list of clusters 
 *	that are together in vicinity and build as one actor
 *
 **/
struct FLODCluster
{
	// constructors
	FLODCluster(const FLODCluster& Other);
	FLODCluster(AActor* Actor1);
	FLODCluster(AActor* Actor1, AActor* Actor2);
	FLODCluster();

	/** Cluster operators */
	FLODCluster operator+( const FLODCluster& Other ) const;
	FLODCluster operator+=( const FLODCluster& Other );
	FLODCluster operator-(const FLODCluster& Other) const;
	FLODCluster operator-=(const FLODCluster& Other);
	FLODCluster& operator=(const FLODCluster & Other);

	/** Invalidates this cluster */
	inline void Invalidate() { bValid = false; }
	
	/** Returns whether or not this cluster is valid */
	inline bool IsValid() const { return bValid; }

	/** Return cost of the cluster, lower is better */
	inline const float GetCost() const
	{
		return ClusterCost;
	}

	/** Compare clusters and returns true when this contains any of Other's actors */
	bool Contains(FLODCluster& Other) const;
	
	/** Returns data/info for this Cluster as a string */
	FString ToString() const;
	
	// member variable
	/** List of Actors that this cluster contains */
	TArray<AActor*> Actors;
	/** Cluster bounds */
	FSphere	Bound;
	/** Filling factor for this cluster, determines how much of the cluster's bounds/area is occupied by the contained actors*/
	float FillingFactor;
	/** Cached cluster cost, FMath::Pow(Bound.W, 3) / FillingFactor */
	float ClusterCost;

	/**
	* if criteria matches, build new LODActor and replace current Actors with that. We don't need
	* this clears previous actors and sets to this new actor
	* this is required when new LOD is created from these actors, this will be replaced
	* to save memory and to reduce memory increase during this process, we discard previous actors and replace with this actor
	*
	* @param InLevel - Level for which currently the HLODs are being build
	* @param LODIdx - LOD index to build
	* @param bCreateMeshes - Whether or not the new StaticMeshes should be created
	* @return ALODActor*
	*/
	ALODActor* BuildActor(ULevel* InLevel, const int32 LODIdx, const bool bCreateMeshes);

	/**
	* Recursively retrieves StaticMeshComponents from a LODActor and its child LODActors 
	*
	* @param Actor - LODActor instance
	* @param InOutComponents - Will hold the StaticMeshComponents	
	*/
	void ExtractStaticMeshComponentsFromLODActor(AActor* Actor, TArray<UStaticMeshComponent*>& InOutComponents);

private:	
	/**
	* Merges this cluster with Other by combining the actor arrays and updating the bounds, filling factor and cluster cost
	*
	* @param Other - Other Cluster to merge with	
	*/
	void MergeClusters(const FLODCluster& Other);

	/**
	* Subtracts the Other cluster from this cluster by removing Others actors and updating the bounds, filling factor and cluster cost
	* This will invalidate the cluster if Actors.Num result to be 0
	*
	* @param Other - Other cluster to subtract
	*/
	void SubtractCluster(const FLODCluster& Other);

	/**
	* Adds a new actor to this cluster and updates the bounds accordingly, the filling factor is NOT updated
	*
	* @param NewActor - Actor to be added to the cluster	
	*/
	FSphere AddActor(AActor* NewActor);

	/** Bool flag whether or not this cluster is valid */
	bool bValid;
};

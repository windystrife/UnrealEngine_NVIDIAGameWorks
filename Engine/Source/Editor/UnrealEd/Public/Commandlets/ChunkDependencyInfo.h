// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ChunkDependencyInfo.generated.h"

/** A single dependency, read from ini file */
USTRUCT()
struct FChunkDependency
{
	GENERATED_USTRUCT_BODY()

	/** The child chunk */
	UPROPERTY(EditAnywhere, Category = ChunkInfo)
	int32 ChunkID;

	/** Parent chunk, anything in both Parent and Child is only placed into Parent */
	UPROPERTY(EditAnywhere, Category = ChunkInfo)
	int32 ParentChunkID;

	bool operator== (const FChunkDependency& RHS) 
	{
		return ChunkID == RHS.ChunkID;
	}
};

/** In memory structure used for dependency tree */
struct FChunkDependencyTreeNode
{
	FChunkDependencyTreeNode(int32 InChunkID=0)
		: ChunkID(InChunkID)
	{}

	int32 ChunkID;

	TArray<FChunkDependencyTreeNode> ChildNodes;
};

/** This is read out of config and defines a tree of chunk dependencies */
UCLASS(config=Engine, defaultconfig)
class UNREALED_API UChunkDependencyInfo : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Will return an existing dependency graph, or build if it necessary */
	const FChunkDependencyTreeNode* GetOrBuildChunkDependencyGraph(int32 HighestChunk = 0);

	/** Will create a dependency tree starting with RootTreeNode. If HighestChunk is == 0 it will only add dependencies on 0 for chunks already in the dependencies list */
	const FChunkDependencyTreeNode* BuildChunkDependencyGraph(int32 HighestChunk);

	/** Removes redundant chunks from a chunk list */
	void RemoveRedundantChunks(TArray<int32>& ChunkIDs) const;

	/** List of dependencies used to remove redundant chunks */
	UPROPERTY(config)
	TArray<FChunkDependency> DependencyArray;

private:
	/** Fills out the Dependency Tree starting with Node */
	void AddChildrenRecursive(FChunkDependencyTreeNode& Node, TArray<FChunkDependency>& DepInfo, TSet<int32> Parents);

	/** Root of tree, this will be valid after calling BuildChunkDependencyGraph */
	FChunkDependencyTreeNode RootTreeNode;

	/** Map of child chunks to all parent chunks, computed in BuildChunkDependencyGraph */
	TMap<int32, TSet<int32>> ChildToParentMap;

	/** Cached value of HighestChunk at time of building */
	int32 CachedHighestChunk;
};

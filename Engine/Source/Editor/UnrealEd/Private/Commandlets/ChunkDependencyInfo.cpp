// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Commandlets/ChunkDependencyInfo.h"

UChunkDependencyInfo::UChunkDependencyInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CachedHighestChunk = -1;
}

const FChunkDependencyTreeNode* UChunkDependencyInfo::GetOrBuildChunkDependencyGraph(int32 HighestChunk)
{
	if (HighestChunk > CachedHighestChunk)
	{
		return BuildChunkDependencyGraph(HighestChunk);
	}
	return &RootTreeNode;
}

const FChunkDependencyTreeNode* UChunkDependencyInfo::BuildChunkDependencyGraph(int32 HighestChunk)
{
	// Reset any current tree
	RootTreeNode.ChunkID = 0;
	RootTreeNode.ChildNodes.Reset(0);

	ChildToParentMap.Reset();
	CachedHighestChunk = HighestChunk;

	// Ensure the DependencyArray is OK to work with.
	for (int32 DepIndex = DependencyArray.Num() - 1; DepIndex >= 0; DepIndex --)
	{
		const FChunkDependency& Dep = DependencyArray[DepIndex];
		if (Dep.ChunkID > HighestChunk)
		{
			HighestChunk = Dep.ChunkID;
		}
		if (Dep.ParentChunkID > HighestChunk)
		{
			HighestChunk = Dep.ParentChunkID;
		}
		if (Dep.ChunkID == Dep.ParentChunkID)
		{
			// Remove cycles
			DependencyArray.RemoveAtSwap(DepIndex);
		}
	}
	// Add missing links (assumes they parent to chunk zero)
	for (int32 i = 1; i <= HighestChunk; ++i)
	{
		if (!DependencyArray.FindByPredicate([=](const FChunkDependency& RHS){ return i == RHS.ChunkID; }))
		{
			FChunkDependency Dep;
			Dep.ChunkID = i;
			Dep.ParentChunkID = 0;
			DependencyArray.Add(Dep);
		}
	}
	// Remove duplicates
	DependencyArray.StableSort([](const FChunkDependency& LHS, const FChunkDependency& RHS) { return LHS.ChunkID < RHS.ChunkID; });
	for (int32 i = 0; i < DependencyArray.Num() - 1;)
	{
		if (DependencyArray[i] == DependencyArray[i + 1])
		{
			DependencyArray.RemoveAt(i + 1);
		}
		else
		{
			++i;
		}
	}

	AddChildrenRecursive(RootTreeNode, DependencyArray, TSet<int32>());
	return &RootTreeNode;
}

void UChunkDependencyInfo::AddChildrenRecursive(FChunkDependencyTreeNode& Node, TArray<FChunkDependency>& DepInfo, TSet<int32> Parents)
{
	if (Parents.Num() > 0)
	{
		ChildToParentMap.FindOrAdd(Node.ChunkID).Append(Parents);
	}

	Parents.Add(Node.ChunkID);
	auto ChildNodeIndices = DepInfo.FilterByPredicate(
		[&](const FChunkDependency& RHS) 
		{
			return Node.ChunkID == RHS.ParentChunkID;
		});
	for (const auto& ChildIndex : ChildNodeIndices)
	{
		Node.ChildNodes.Add(FChunkDependencyTreeNode(ChildIndex.ChunkID));
	}
	for (auto& Child : Node.ChildNodes) 
	{
		AddChildrenRecursive(Child, DepInfo, Parents);
	}
}

void UChunkDependencyInfo::RemoveRedundantChunks(TArray<int32>& ChunkIDs) const
{
	for (int32 ChunkIndex = ChunkIDs.Num() - 1; ChunkIndex >= 0; ChunkIndex--)
	{
		const TSet<int32>* FoundParents = ChildToParentMap.Find(ChunkIDs[ChunkIndex]);

		if (FoundParents)
		{
			for (int32 ParentChunk : *FoundParents)
			{
				if (ChunkIDs.Contains(ParentChunk))
				{
					ChunkIDs.RemoveAt(ChunkIndex);
					break;
				}
			}
		}
	}
}
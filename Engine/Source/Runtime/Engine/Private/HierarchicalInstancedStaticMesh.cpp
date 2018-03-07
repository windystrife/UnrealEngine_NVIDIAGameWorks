// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	InstancedStaticMesh.cpp: Static mesh rendering code.
=============================================================================*/

#include "CoreMinimal.h"
#include "Templates/Greater.h"
#include "Math/RandomStream.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "UObject/ObjectMacros.h"
#include "Async/TaskGraphInterfaces.h"
#include "EngineStats.h"
#include "Async/AsyncWork.h"
#include "PrimitiveViewRelevance.h"
#include "ConvexVolume.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Engine/MapBuildDataRegistry.h"
#include "MaterialShared.h"
#include "UObject/UObjectIterator.h"
#include "MeshBatch.h"
#include "RendererInterface.h"
#include "Engine/StaticMesh.h"
#include "UnrealEngine.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "StaticMeshResources.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "InstancedStaticMesh.h"
#include "SceneManagement.h"
#include "HAL/LowLevelMemTracker.h"

static TAutoConsoleVariable<int32> CVarFoliageSplitFactor(
	TEXT("foliage.SplitFactor"),
	16,
	TEXT("This controls the branching factor of the foliage tree."));

static TAutoConsoleVariable<int32> CVarForceLOD(
	TEXT("foliage.ForceLOD"),
	-1,
	TEXT("If greater than or equal to zero, forces the foliage LOD to that level."));

static TAutoConsoleVariable<int32> CVarOnlyLOD(
	TEXT("foliage.OnlyLOD"),
	-1,
	TEXT("If greater than or equal to zero, only renders the foliage LOD at that level."));

static TAutoConsoleVariable<int32> CVarDisableCull(
	TEXT("foliage.DisableCull"),
	0,
	TEXT("If greater than zero, no culling occurs based on frustum."));

static TAutoConsoleVariable<int32> CVarCullAll(
	TEXT("foliage.CullAll"),
	0,
	TEXT("If greater than zero, everything is considered culled."));

static TAutoConsoleVariable<int32> CVarDitheredLOD(
	TEXT("foliage.DitheredLOD"),
	1,
	TEXT("If greater than zero, dithered LOD is used, otherwise popping LOD is used."));

static TAutoConsoleVariable<int32> CVarOverestimateLOD(
	TEXT("foliage.OverestimateLOD"),
	0,
	TEXT("If greater than zero and dithered LOD is not used, then we use an overestimate of LOD instead of an underestimate."));

static TAutoConsoleVariable<int32> CVarMaxTrianglesToRender(
	TEXT("foliage.MaxTrianglesToRender"),
	100000000,
	TEXT("This is an absolute limit on the number of foliage triangles to render in one traversal. This is used to prevent a silly LOD parameter mistake from causing the OS to kill the GPU."));

TAutoConsoleVariable<float> CVarFoliageMinimumScreenSize(
	TEXT("foliage.MinimumScreenSize"),
	0.000005f,
	TEXT("This controls the screen size at which we cull foliage instances entirely."),
	ECVF_Scalability);

TAutoConsoleVariable<float> CVarFoliageLODDistanceScale(
	TEXT("foliage.LODDistanceScale"),
	1.0f,
	TEXT("Scale factor for the distance used in computing LOD for foliage."));

TAutoConsoleVariable<float> CVarRandomLODRange(
	TEXT("foliage.RandomLODRange"),
	0.0f,
	TEXT("Random distance added to each instance distance to compute LOD."));

static TAutoConsoleVariable<int32> CVarMinVertsToSplitNode(
	TEXT("foliage.MinVertsToSplitNode"),
	16384,
	TEXT("Controls the accuracy between culling and LOD accuracy and culling and CPU performance."));

static TAutoConsoleVariable<int32> CVarMaxOcclusionQueriesPerComponent(
	TEXT("foliage.MaxOcclusionQueriesPerComponent"),
	16,
	TEXT("Controls the granularity of occlusion culling. 16-128 is a reasonable range."));

static TAutoConsoleVariable<int32> CVarMinOcclusionQueriesPerComponent(
	TEXT("foliage.MinOcclusionQueriesPerComponent"),
	6,
	TEXT("Controls the granularity of occlusion culling. 2 should be the Min."));

static TAutoConsoleVariable<int32> CVarMinInstancesPerOcclusionQuery(
	TEXT("foliage.MinInstancesPerOcclusionQuery"),
	256,
	TEXT("Controls the granualrity of occlusion culling. 1024 to 65536 is a reasonable range. This is not exact, actual minimum might be off by a factor of two."));

static TAutoConsoleVariable<float> CVarFoliageDensityScale(
	TEXT("foliage.DensityScale"),
	1.0,
	TEXT("Controls the amount of foliage to render. Foliage must opt-in to density scaling through the foliage type."),
	ECVF_Scalability);

DECLARE_CYCLE_STAT(TEXT("Traversal Time"),STAT_FoliageTraversalTime,STATGROUP_Foliage);
DECLARE_CYCLE_STAT(TEXT("Build Time"), STAT_FoliageBuildTime, STATGROUP_Foliage);
DECLARE_CYCLE_STAT(TEXT("Batch Time"),STAT_FoliageBatchTime,STATGROUP_Foliage);
DECLARE_CYCLE_STAT(TEXT("Foliage Create Proxy"), STAT_FoliageCreateProxy, STATGROUP_Foliage);
DECLARE_CYCLE_STAT(TEXT("Foliage Post Load"), STAT_FoliagePostLoad, STATGROUP_Foliage);

DECLARE_DWORD_COUNTER_STAT(TEXT("Runs"), STAT_FoliageRuns, STATGROUP_Foliage);
DECLARE_DWORD_COUNTER_STAT(TEXT("Mesh Batches"), STAT_FoliageMeshBatches, STATGROUP_Foliage);
DECLARE_DWORD_COUNTER_STAT(TEXT("Triangles"), STAT_FoliageTriangles, STATGROUP_Foliage);
DECLARE_DWORD_COUNTER_STAT(TEXT("Instances"), STAT_FoliageInstances, STATGROUP_Foliage);
DECLARE_DWORD_COUNTER_STAT(TEXT("Occlusion Culled Instances"), STAT_OcclusionCulledFoliageInstances, STATGROUP_Foliage);
DECLARE_DWORD_COUNTER_STAT(TEXT("Traversals"),STAT_FoliageTraversals,STATGROUP_Foliage);
DECLARE_MEMORY_STAT(TEXT("Instance Buffers"),STAT_FoliageInstanceBuffers,STATGROUP_Foliage);

static void FoliageCVarSinkFunction()
{
	static float CachedFoliageDensityScale = 1.0f;
	float FoliageDensityScale = CVarFoliageDensityScale.GetValueOnGameThread();

	if (FoliageDensityScale != CachedFoliageDensityScale)
	{
		CachedFoliageDensityScale = FoliageDensityScale;

		for (auto* Component : TObjectRange<UHierarchicalInstancedStaticMeshComponent>(RF_ClassDefaultObject | RF_ArchetypeObject, true, EInternalObjectFlags::PendingKill))
		{
			if (Component->bEnableDensityScaling && Component->GetWorld() && Component->GetWorld()->IsGameWorld())
			{
				if (FoliageDensityScale == 0)
				{
					// exclude all instances
					Component->ExcludedDueToDensityScaling.Init(true, Component->PerInstanceSMData.Num());
				}
				else if (FoliageDensityScale > 0.0f && FoliageDensityScale < 1.0f)
				{
					FRandomStream Rand(Component->InstancingRandomSeed);
					if (Component->ExcludedDueToDensityScaling.Num() == 0)
					{
						// TBitArray<> doesn't have SetNumUninitialized...
						Component->ExcludedDueToDensityScaling.Init(false, Component->PerInstanceSMData.Num());
					}

					for (int32 i = 0; i < Component->ExcludedDueToDensityScaling.Num(); ++i)
					{
						Component->ExcludedDueToDensityScaling[i] = (Rand.FRand() > FoliageDensityScale);
					}
				}
				else
				{
					// Show all instances
					Component->ExcludedDueToDensityScaling.Empty();
				}

				Component->BuildTreeIfOutdated(false, true);
			}
		}
	}
}

static FAutoConsoleVariableSink CVarFoliageSink(FConsoleCommandDelegate::CreateStatic(&FoliageCVarSinkFunction));

struct FClusterTree
{
	TArray<FClusterNode> Nodes;
	TArray<int32> SortedInstances;
	TArray<int32> InstanceReorderTable;
	int32 OutOcclusionLayerNum;
};


class FClusterBuilder
{
	int32 OriginalNum;
	int32 Num;
	FBox InstBox;
	int32 BranchingFactor;
	int32 InternalNodeBranchingFactor;
	int32 OcclusionLayerTarget;
	int32 MaxInstancesPerLeaf;
	int32 NumRoots;
	TArray<int32> SortIndex;
	TArray<FVector> SortPoints;
	TArray<FMatrix> Transforms;

	struct FRunPair
	{
		int32 Start;
		int32 Num;

		FRunPair(int32 InStart, int32 InNum)
			: Start(InStart)
			, Num(InNum)
		{
		}

		bool operator< (const FRunPair& Other) const
		{
			return Start < Other.Start;
		}
	};
	TArray<FRunPair> Clusters;

	struct FSortPair
	{
		float d;
		int32 Index;

		bool operator< (const FSortPair& Other) const
		{
			return d < Other.d;
		}
	};
	TArray<FSortPair> SortPairs;

	void Split(int32 InNum)
	{
		checkSlow(InNum);
		Clusters.Reset();
		Split(0, InNum - 1);
		Clusters.Sort();
		checkSlow(Clusters.Num() > 0);
		int32 At = 0;
		for (auto& Cluster : Clusters)
		{
			checkSlow(At == Cluster.Start);
			At += Cluster.Num;
		}
		checkSlow(At == InNum);
	}

	void Split(int32 Start, int32 End)
	{
		int32 NumRange = 1 + End - Start;
		FBox ClusterBounds(ForceInit);
		for (int32 Index = Start; Index <= End; Index++)
		{
			ClusterBounds += SortPoints[SortIndex[Index]];
		}
		if (NumRange <= BranchingFactor)
		{
			Clusters.Add(FRunPair(Start, NumRange));
			return;
		}
		checkSlow(NumRange >= 2);
		SortPairs.Reset();
		int32 BestAxis = -1;
		float BestAxisValue = -1.0f;
		for (int32 Axis = 0; Axis < 3; Axis++)
		{
			float ThisAxisValue = ClusterBounds.Max[Axis] - ClusterBounds.Min[Axis];
			if (!Axis || ThisAxisValue > BestAxisValue)
			{
				BestAxis = Axis;
				BestAxisValue = ThisAxisValue;
			}
		}
		for (int32 Index = Start; Index <= End; Index++)
		{
			FSortPair Pair;

			Pair.Index = SortIndex[Index];
			Pair.d = SortPoints[Pair.Index][BestAxis];
			SortPairs.Add(Pair);
		}
		SortPairs.Sort();
		for (int32 Index = Start; Index <= End; Index++)
		{
			SortIndex[Index] = SortPairs[Index - Start].Index;
		}

		int32 Half = NumRange / 2;

		int32 EndLeft = Start + Half - 1;
		int32 StartRight = 1 + End - Half;

		if (NumRange & 1)
		{
			if (SortPairs[Half].d - SortPairs[Half - 1].d < SortPairs[Half + 1].d - SortPairs[Half].d)
			{
				EndLeft++;
			}
			else
			{
				StartRight--;
			}
		}
		checkSlow(EndLeft + 1 == StartRight);
		checkSlow(EndLeft >= Start);
		checkSlow(End >= StartRight);

		Split(Start, EndLeft);
		Split(StartRight, End);
	}
public:
	FClusterTree* Result;

	FClusterBuilder(TArray<FMatrix> InTransforms, const FBox& InInstBox, int32 InMaxInstancesPerLeaf, TBitArray<> ExcludedDueToDensityScaling = TBitArray<>())
		: OriginalNum(InTransforms.Num())
		, InstBox(InInstBox)
		, Transforms(MoveTemp(InTransforms))
		, Result(nullptr)
	{
		SortPoints.AddUninitialized(OriginalNum);
		for (int32 Index = 0; Index < OriginalNum; Index++)
		{
			SortPoints[Index] = Transforms[Index].GetOrigin();
		}

		for (int32 Index = 0; Index < ExcludedDueToDensityScaling.Num(); ++Index)
		{
			if (!ExcludedDueToDensityScaling[Index])
			{
				SortIndex.Add(Index);
			}
		}
		for (int32 Index = ExcludedDueToDensityScaling.Num(); Index < OriginalNum; ++Index)
		{
			SortIndex.Add(Index);
		}

		Num = SortIndex.Num();

		OcclusionLayerTarget = CVarMaxOcclusionQueriesPerComponent.GetValueOnAnyThread();
		int32 MinInstancesPerOcclusionQuery = CVarMinInstancesPerOcclusionQuery.GetValueOnAnyThread();

		if (Num / MinInstancesPerOcclusionQuery < OcclusionLayerTarget)
		{
			OcclusionLayerTarget = Num / MinInstancesPerOcclusionQuery;
			if (OcclusionLayerTarget < CVarMinOcclusionQueriesPerComponent.GetValueOnAnyThread())
			{
				OcclusionLayerTarget = 0;
			}
		}
		InternalNodeBranchingFactor = CVarFoliageSplitFactor.GetValueOnAnyThread();
		MaxInstancesPerLeaf = InMaxInstancesPerLeaf;
	}

	void BuildAsync(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Build();
		//FPlatformProcess::Sleep(5);
	}

	void Build()
	{
		Result = new FClusterTree;
		Result->OutOcclusionLayerNum = 0;

		if (Num == 0)
		{
			// Can happen if all instances are excluded due to scalability
			// It doesn't only happen with a scalability factor of 0 - 
			// even with a scalability factor of 0.99, if there's only one instance of this type you can end up with Num == 0 if you're unlucky
			Result->InstanceReorderTable.Init(INDEX_NONE, OriginalNum);
			return;
		}

		bool bIsOcclusionLayer = false;
		BranchingFactor = MaxInstancesPerLeaf;
		if (BranchingFactor > 2 && OcclusionLayerTarget && Num / BranchingFactor <= OcclusionLayerTarget)
		{
			BranchingFactor = FMath::Max<int32>(2, (Num + OcclusionLayerTarget - 1) / OcclusionLayerTarget);
			OcclusionLayerTarget = 0;
			bIsOcclusionLayer = true;
		}
		Split(Num);
		if (bIsOcclusionLayer)
		{
			Result->OutOcclusionLayerNum = Clusters.Num();
			bIsOcclusionLayer = false;
		}

		TArray<int32>& SortedInstances = Result->SortedInstances;
		SortedInstances.Append(SortIndex);
		
		NumRoots = Clusters.Num();
		Result->Nodes.Init(FClusterNode(), Clusters.Num());

		for (int32 Index = 0; Index < NumRoots; Index++)
		{
			FClusterNode& Node = Result->Nodes[Index];
			Node.FirstInstance = Clusters[Index].Start;
			Node.LastInstance = Clusters[Index].Start + Clusters[Index].Num - 1;
			FBox NodeBox(ForceInit);
			for (int32 InstanceIndex = Node.FirstInstance; InstanceIndex <= Node.LastInstance; InstanceIndex++)
			{
				const FMatrix& ThisInstTrans = Transforms[SortedInstances[InstanceIndex]];
				FBox ThisInstBox = InstBox.TransformBy(ThisInstTrans);
				NodeBox += ThisInstBox;
			}
			Node.BoundMin = NodeBox.Min;
			Node.BoundMax = NodeBox.Max;
		}
		TArray<int32> NodesPerLevel;
		NodesPerLevel.Add(NumRoots);
		int32 LOD = 0;

		TArray<int32> InverseSortIndex;
		TArray<int32> RemapSortIndex;
		TArray<int32> InverseInstanceIndex;
		TArray<int32> OldInstanceIndex;
		TArray<int32> LevelStarts;
		TArray<int32> InverseChildIndex;
		TArray<FClusterNode> OldNodes;

		while (NumRoots > 1)
		{
			SortIndex.Reset();
			SortPoints.Reset();
			SortIndex.AddUninitialized(NumRoots);
			SortPoints.AddUninitialized(NumRoots);
			for (int32 Index = 0; Index < NumRoots; Index++)
			{
				SortIndex[Index] = Index;
				FClusterNode& Node = Result->Nodes[Index];
				SortPoints[Index] = (Node.BoundMin + Node.BoundMax) * 0.5f;
			}
			BranchingFactor = InternalNodeBranchingFactor;
			if (BranchingFactor > 2 && OcclusionLayerTarget && NumRoots / BranchingFactor <= OcclusionLayerTarget)
			{
				BranchingFactor = FMath::Max<int32>(2, (NumRoots + OcclusionLayerTarget - 1) / OcclusionLayerTarget);
				OcclusionLayerTarget = 0;
				bIsOcclusionLayer = true;
			}
			Split(NumRoots);
			if (bIsOcclusionLayer)
			{
				Result->OutOcclusionLayerNum = Clusters.Num();
				bIsOcclusionLayer = false;
			}

			InverseSortIndex.Reset();
			InverseSortIndex.AddUninitialized(NumRoots);
			for (int32 Index = 0; Index < NumRoots; Index++)
			{
				InverseSortIndex[SortIndex[Index]] = Index;
			}

			{
				// rearrange the instances to match the new order of the old roots
				RemapSortIndex.Reset();
				RemapSortIndex.AddUninitialized(Num);
				int32 OutIndex = 0;
				for (int32 Index = 0; Index < NumRoots; Index++)
				{
					FClusterNode& Node = Result->Nodes[SortIndex[Index]];
					for (int32 InstanceIndex = Node.FirstInstance; InstanceIndex <= Node.LastInstance; InstanceIndex++)
					{
						RemapSortIndex[OutIndex++] = InstanceIndex;
					}
				}
				InverseInstanceIndex.Reset();
				InverseInstanceIndex.AddUninitialized(Num);
				for (int32 Index = 0; Index < Num; Index++)
				{
					InverseInstanceIndex[RemapSortIndex[Index]] = Index;
				}
				for (int32 Index = 0; Index < Result->Nodes.Num(); Index++)
				{
					FClusterNode& Node = Result->Nodes[Index];
					Node.FirstInstance = InverseInstanceIndex[Node.FirstInstance];
					Node.LastInstance = InverseInstanceIndex[Node.LastInstance];
				}
				OldInstanceIndex.Reset();
				Swap(OldInstanceIndex, SortedInstances);
				SortedInstances.AddUninitialized(Num);
				for (int32 Index = 0; Index < Num; Index++)
				{
					SortedInstances[Index] = OldInstanceIndex[RemapSortIndex[Index]];
				}
			}
			{
				// rearrange the nodes to match the new order of the old roots
				RemapSortIndex.Reset();
				int32 NewNum = Result->Nodes.Num() + Clusters.Num();
				// RemapSortIndex[new index] == old index
				RemapSortIndex.AddUninitialized(NewNum);
				LevelStarts.Reset();
				LevelStarts.Add(Clusters.Num());
				for (int32 Index = 0; Index < NodesPerLevel.Num() - 1; Index++)
				{
					LevelStarts.Add(LevelStarts[Index] + NodesPerLevel[Index]);
				}

				for (int32 Index = 0; Index < NumRoots; Index++)
				{
					FClusterNode& Node = Result->Nodes[SortIndex[Index]];
					RemapSortIndex[LevelStarts[0]++] = SortIndex[Index];

					int32 LeftIndex = Node.FirstChild;
					int32 RightIndex = Node.LastChild;
					int32 LevelIndex = 1;
					while (RightIndex >= 0)
					{
						int32 NextLeftIndex = MAX_int32;
						int32 NextRightIndex = -1;
						for (int32 ChildIndex = LeftIndex; ChildIndex <= RightIndex; ChildIndex++)
						{
							RemapSortIndex[LevelStarts[LevelIndex]++] = ChildIndex;
							int32 LeftChild = Result->Nodes[ChildIndex].FirstChild;
							int32 RightChild = Result->Nodes[ChildIndex].LastChild;
							if (LeftChild >= 0 && LeftChild <  NextLeftIndex)
							{
								NextLeftIndex = LeftChild;
							}
							if (RightChild >= 0 && RightChild >  NextRightIndex)
							{
								NextRightIndex = RightChild;
							}
						}
						LeftIndex = NextLeftIndex;
						RightIndex = NextRightIndex;
						LevelIndex++;
					}
				}
				checkSlow(LevelStarts[LevelStarts.Num() - 1] == NewNum);
				InverseChildIndex.Reset();
				// InverseChildIndex[old index] == new index
				InverseChildIndex.AddUninitialized(NewNum);
				for (int32 Index = Clusters.Num(); Index < NewNum; Index++)
				{
					InverseChildIndex[RemapSortIndex[Index]] = Index;
				}
				for (int32 Index = 0; Index < Result->Nodes.Num(); Index++)
				{
					FClusterNode& Node = Result->Nodes[Index];
					if (Node.FirstChild >= 0)
					{
						Node.FirstChild = InverseChildIndex[Node.FirstChild];
						Node.LastChild = InverseChildIndex[Node.LastChild];
					}
				}
				{
					Swap(OldNodes, Result->Nodes);
					Result->Nodes.Empty(NewNum);
					for (int32 Index = 0; Index < Clusters.Num(); Index++)
					{
						Result->Nodes.Add(FClusterNode());
					}
					Result->Nodes.AddUninitialized(OldNodes.Num());
					for (int32 Index = 0; Index < OldNodes.Num(); Index++)
					{
						Result->Nodes[InverseChildIndex[Index]] = OldNodes[Index];
					}
				}
				int32 OldIndex = Clusters.Num();
				int32 InstanceTracker = 0;
				for (int32 Index = 0; Index < Clusters.Num(); Index++)
				{
					FClusterNode& Node = Result->Nodes[Index];
					Node.FirstChild = OldIndex;
					OldIndex += Clusters[Index].Num;
					Node.LastChild = OldIndex - 1;
					Node.FirstInstance = Result->Nodes[Node.FirstChild].FirstInstance;
					checkSlow(Node.FirstInstance == InstanceTracker);
					Node.LastInstance = Result->Nodes[Node.LastChild].LastInstance;
					InstanceTracker = Node.LastInstance + 1;
					checkSlow(InstanceTracker <= Num);
					FBox NodeBox(ForceInit);
					for (int32 ChildIndex = Node.FirstChild; ChildIndex <= Node.LastChild; ChildIndex++)
					{
						FClusterNode& ChildNode = Result->Nodes[ChildIndex];
						NodeBox += ChildNode.BoundMin;
						NodeBox += ChildNode.BoundMax;
					}
					Node.BoundMin = NodeBox.Min;
					Node.BoundMax = NodeBox.Max;
				}
				NumRoots = Clusters.Num();
				NodesPerLevel.Insert(NumRoots, 0);
			}
		}

		// Save inverse map
		Result->InstanceReorderTable.Init(INDEX_NONE, OriginalNum);
		for (int32 Index = 0; Index < Num; Index++)
		{
			Result->InstanceReorderTable[SortedInstances[Index]] = Index;
		}
	}

};


static bool PrintLevel(const FClusterTree& Tree, int32 NodeIndex, int32 Level, int32 CurrentLevel, int32 Parent)
{
	const FClusterNode& Node = Tree.Nodes[NodeIndex];
	if (Level == CurrentLevel)
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("Level %2d  Parent %3d"),
			Level,
			Parent
			);
		FVector Extent = Node.BoundMax - Node.BoundMin;
		UE_LOG(LogConsoleResponse, Display, TEXT("    Bound (%5.1f, %5.1f, %5.1f) [(%5.1f, %5.1f, %5.1f) - (%5.1f, %5.1f, %5.1f)]"),
			Extent.X, Extent.Y, Extent.Z, 
			Node.BoundMin.X, Node.BoundMin.Y, Node.BoundMin.Z, 
			Node.BoundMax.X, Node.BoundMax.Y, Node.BoundMax.Z
			);
		UE_LOG(LogConsoleResponse, Display, TEXT("    children %3d [%3d,%3d]   instances %3d [%3d,%3d]"),
			(Node.FirstChild < 0) ? 0 : 1 + Node.LastChild - Node.FirstChild, Node.FirstChild, Node.LastChild,
			1 + Node.LastInstance - Node.FirstInstance, Node.FirstInstance, Node.LastInstance
			);
		return true;
	}
	else if (Node.FirstChild < 0)
	{
		return false;
	}
	bool Ret = false;
	for (int32 Child = Node.FirstChild; Child <= Node.LastChild; Child++)
	{
		Ret = PrintLevel(Tree, Child, Level, CurrentLevel + 1, NodeIndex) || Ret;
	}
	return Ret;
}

static void TestFoliage(const TArray<FString>& Args)
{
	UE_LOG(LogConsoleResponse, Display, TEXT("Running Foliage test."));
	TArray<FInstancedStaticMeshInstanceData> Instances;

	FMatrix Temp;
	Temp.SetIdentity();
	FRandomStream RandomStream(0x238946);
	for (int32 i = 0; i < 1000; i++)
	{
		Instances.Add(FInstancedStaticMeshInstanceData());
		Temp.SetOrigin(FVector(RandomStream.FRandRange(0.0f, 1.0f), RandomStream.FRandRange(0.0f, 1.0f), 0.0f) * 10000.0f);
		Instances[i].Transform = Temp;
	}

	FBox TempBox(ForceInit);
	TempBox += FVector(-100.0f, -100.0f, -100.0f);
	TempBox += FVector(100.0f, 100.0f, 100.0f);

	TArray<FMatrix> InstanceTransforms;
	InstanceTransforms.AddUninitialized(Instances.Num());
	for (int32 Index = 0; Index < Instances.Num(); Index++)
	{
		InstanceTransforms[Index] = Instances[Index].Transform;
	}
	FClusterBuilder Builder(InstanceTransforms, TempBox, 16);
	Builder.Build();

	int32 Level = 0;

	UE_LOG(LogConsoleResponse, Display, TEXT("-----"));

	while(PrintLevel(*Builder.Result, 0, Level++, 0, -1))
	{
	}

	delete Builder.Result;
}

static FAutoConsoleCommand TestFoliageCmd(
	TEXT("foliage.Test"),
	TEXT("Useful for debugging."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&TestFoliage)
	);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	static uint32 GDebugTag = 1;
	static uint32 GCaptureDebugRuns = 0;
#endif

static void FreezeFoliageCulling(const TArray<FString>& Args)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UE_LOG(LogConsoleResponse, Display, TEXT("Freezing Foliage Culling."));
	GDebugTag++;
	GCaptureDebugRuns = GDebugTag;
#endif
}

static FAutoConsoleCommand FreezeFoliageCullingCmd(
	TEXT("foliage.Freeze"),
	TEXT("Useful for debugging. Freezes the foliage culling and LOD."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FreezeFoliageCulling)
	);

static void UnFreezeFoliageCulling(const TArray<FString>& Args)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UE_LOG(LogConsoleResponse, Display, TEXT("Unfreezing Foliage Culling."));
	GDebugTag++;
	GCaptureDebugRuns = 0;
#endif
}

static FAutoConsoleCommand UnFreezeFoliageCullingCmd(
	TEXT("foliage.UnFreeze"),
	TEXT("Useful for debugging. Freezes the foliage culling and LOD."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&UnFreezeFoliageCulling)
	);

void ToggleFreezeFoliageCulling()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	TArray<FString> Args;

	if (GCaptureDebugRuns == 0)
	{
		FreezeFoliageCulling(Args);
	}
	else
	{
		UnFreezeFoliageCulling(Args);
	}
#endif
}


struct FFoliageOcclusionResults
{
	TArray<bool> Results; // we keep a copy from the View as the view will get destroyed too often
	int32 ResultsStart;
	int32 NumResults;
	uint32 FrameNumberRenderThread;

	FFoliageOcclusionResults(TArray<bool>* InResults, int32 InResultsStart, int32 InNumResults)
		: Results(*InResults)
		, ResultsStart(InResultsStart)
		, NumResults(InNumResults)
		, FrameNumberRenderThread(GFrameNumberRenderThread)
	{

	}
};

struct FFoliageElementParams;
struct FFoliageRenderInstanceParams;
struct FFoliageCullInstanceParams;

class FHierarchicalStaticMeshSceneProxy : public FInstancedStaticMeshSceneProxy
{
	TSharedRef<TArray<FClusterNode>, ESPMode::ThreadSafe> ClusterTreePtr;
	const TArray<FClusterNode>& ClusterTree;

	TArray<FBox> UnbuiltBounds;
	int32 FirstUnbuiltIndex;
	int32 UnbuiltInstanceCount;

	int32 FirstOcclusionNode;
	int32 LastOcclusionNode;
	TArray<FBoxSphereBounds> OcclusionBounds;
	TMap<uint32, FFoliageOcclusionResults> OcclusionResults;
	bool bIsGrass;
	uint32 SceneProxyCreatedFrameNumberRenderThread;
	bool bDitheredLODTransitions;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	mutable TArray<uint32> SingleDebugRuns[MAX_STATIC_MESH_LODS];
	mutable int32 SingleDebugTotalInstances[MAX_STATIC_MESH_LODS];
	mutable TArray<uint32> MultipleDebugRuns[MAX_STATIC_MESH_LODS];
	mutable int32 MultipleDebugTotalInstances[MAX_STATIC_MESH_LODS];
	mutable int32 CaptureTag;
#endif

public:

	FHierarchicalStaticMeshSceneProxy(bool bInIsGrass, UHierarchicalInstancedStaticMeshComponent* InComponent, ERHIFeatureLevel::Type InFeatureLevel)
		: FInstancedStaticMeshSceneProxy(InComponent, InFeatureLevel)
		, ClusterTreePtr(InComponent->ClusterTreePtr.ToSharedRef())
		, ClusterTree(*InComponent->ClusterTreePtr)
		, UnbuiltBounds(InComponent->UnbuiltInstanceBoundsList)
		, FirstUnbuiltIndex(InComponent->NumBuiltRenderInstances)
		, UnbuiltInstanceCount(InComponent->UnbuiltInstanceIndexList.Num())
		, bIsGrass(bInIsGrass)
		, SceneProxyCreatedFrameNumberRenderThread(UINT32_MAX)
		, bDitheredLODTransitions(InComponent->SupportsDitheredLODTransitions())
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		, CaptureTag(0)
#endif
	{
		SetupOcclusion(InComponent);
	}

	FHierarchicalStaticMeshSceneProxy(bool bInIsGrass, UHierarchicalInstancedStaticMeshComponent* InComponent, ERHIFeatureLevel::Type InFeatureLevel, FStaticMeshInstanceData& Other)
		: FInstancedStaticMeshSceneProxy(InComponent, InFeatureLevel, Other)
		, ClusterTreePtr(InComponent->ClusterTreePtr.ToSharedRef())
		, ClusterTree(*InComponent->ClusterTreePtr)
		, UnbuiltBounds(InComponent->UnbuiltInstanceBoundsList)
		, FirstUnbuiltIndex(InComponent->NumBuiltRenderInstances)
		, UnbuiltInstanceCount(InComponent->UnbuiltInstanceIndexList.Num())
		, bIsGrass(bInIsGrass)
		, SceneProxyCreatedFrameNumberRenderThread(UINT32_MAX)
		, bDitheredLODTransitions(InComponent->SupportsDitheredLODTransitions())
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		, CaptureTag(0)
#endif
	{
		check(!bInIsGrass || (!InComponent->InstanceReorderTable.Num() && !InComponent->PerInstanceSMData.Num()));
		SetupOcclusion(InComponent);
	}

	void SetupOcclusion(UHierarchicalInstancedStaticMeshComponent* InComponent)
	{
		FirstOcclusionNode = 0;
		LastOcclusionNode = 0;
		if (ClusterTree.Num() && InComponent->OcclusionLayerNumNodes)
		{
			while (true)
			{
				int32 NextFirstOcclusionNode = ClusterTree[FirstOcclusionNode].FirstChild;
				int32 NextLastOcclusionNode = ClusterTree[LastOcclusionNode].LastChild;

				if (NextFirstOcclusionNode < 0 || NextLastOcclusionNode < 0)
				{
					break;
				}
				int32 NumNodes = 1 + NextLastOcclusionNode - NextFirstOcclusionNode;
				if (NumNodes > InComponent->OcclusionLayerNumNodes)
				{
					break;
				}
				FirstOcclusionNode = NextFirstOcclusionNode;
				LastOcclusionNode = NextLastOcclusionNode;
			}
		}
		int32 NumNodes = 1 + LastOcclusionNode - FirstOcclusionNode;
		if (NumNodes < 2)
		{
			FirstOcclusionNode = -1;
			LastOcclusionNode = -1;
			NumNodes = 0;
			if (ClusterTree.Num())
			{
				//UE_LOG(LogTemp, Display, TEXT("No SubOcclusion %d inst"), 1 + ClusterTree[0].LastInstance - ClusterTree[0].FirstInstance);
			}
		}
		else
		{
			//int32 NumPerNode = (1 + ClusterTree[0].LastInstance - ClusterTree[0].FirstInstance) / NumNodes;
			//UE_LOG(LogTemp, Display, TEXT("Occlusion level %d   %d inst / node"), NumNodes, NumPerNode);
			OcclusionBounds.Reserve(NumNodes);
			FMatrix XForm = InComponent->GetComponentTransform().ToMatrixWithScale();
			for (int32 Index = FirstOcclusionNode; Index <= LastOcclusionNode; Index++)
			{
				OcclusionBounds.Add(FBoxSphereBounds(FBox(ClusterTree[Index].BoundMin, ClusterTree[Index].BoundMax).TransformBy(XForm)));
			}
		}
	}

	// FPrimitiveSceneProxy interface.
	
	virtual void CreateRenderThreadResources() override
	{
		FInstancedStaticMeshSceneProxy::CreateRenderThreadResources();
		SceneProxyCreatedFrameNumberRenderThread = GFrameNumberRenderThread;
	}
	
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		if (bIsGrass ? View->Family->EngineShowFlags.InstancedGrass : View->Family->EngineShowFlags.InstancedFoliage)
		{
			Result = FStaticMeshSceneProxy::GetViewRelevance(View);
			Result.bDynamicRelevance = true;
			Result.bStaticRelevance = false;
		}
		return Result;
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual const TArray<FBoxSphereBounds>* GetOcclusionQueries(const FSceneView* View) const override;
	virtual void AcceptOcclusionResults(const FSceneView* View, TArray<bool>* Results, int32 ResultsStart, int32 NumResults) override;
	virtual bool HasSubprimitiveOcclusionQueries() const override
	{
		return FirstOcclusionNode > 0;
	}


	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override
	{
	}

	virtual void ApplyWorldOffset(FVector InOffset) override
	{
		FInstancedStaticMeshSceneProxy::ApplyWorldOffset(InOffset);
		
		for (FBoxSphereBounds& Item : OcclusionBounds)
		{
			Item.Origin+= InOffset;
		}
	}

	void FillDynamicMeshElements(FMeshElementCollector& Collector, const FFoliageElementParams& ElementParams, const FFoliageRenderInstanceParams& Instances) const;

	template<bool TUseVector>
	void Traverse(const FFoliageCullInstanceParams& Params, int32 Index, int32 MinLOD, int32 MaxLOD, bool bFullyContained = false) const;
};

struct FFoliageRenderInstanceParams
{
	bool bNeedsSingleLODRuns;
	bool bNeedsMultipleLODRuns;
	bool bOverestimate;
	mutable TArray<uint32, SceneRenderingAllocator> MultipleLODRuns[MAX_STATIC_MESH_LODS];
	mutable TArray<uint32, SceneRenderingAllocator> SingleLODRuns[MAX_STATIC_MESH_LODS];
	mutable int32 TotalSingleLODInstances[MAX_STATIC_MESH_LODS];
	mutable int32 TotalMultipleLODInstances[MAX_STATIC_MESH_LODS];

	FFoliageRenderInstanceParams(bool InbNeedsSingleLODRuns, bool InbNeedsMultipleLODRuns, bool InbOverestimate)
		: bNeedsSingleLODRuns(InbNeedsSingleLODRuns)
		, bNeedsMultipleLODRuns(InbNeedsMultipleLODRuns)
		, bOverestimate(InbOverestimate)
	{
		for (int32 Index = 0; Index < MAX_STATIC_MESH_LODS; Index++)
		{
			TotalSingleLODInstances[Index] = 0;
			TotalMultipleLODInstances[Index] = 0;
		}
	}
	static FORCEINLINE_DEBUGGABLE void AddRun(TArray<uint32, SceneRenderingAllocator>& Array, int32 FirstInstance, int32 LastInstance)
	{
		if (Array.Num() && Array.Last() + 1 == FirstInstance)
		{
			Array.Last() = (uint32)LastInstance;
		}
		else
		{
			Array.Add((uint32)FirstInstance);
			Array.Add((uint32)LastInstance);
		}
	}
	FORCEINLINE_DEBUGGABLE void AddRun(int32 MinLod, int32 MaxLod, int32 FirstInstance, int32 LastInstance) const
	{
		if (bNeedsSingleLODRuns)
		{
			AddRun(SingleLODRuns[bOverestimate ? MaxLod : MinLod], FirstInstance, LastInstance);
			TotalSingleLODInstances[bOverestimate ? MaxLod : MinLod] += 1 + LastInstance - FirstInstance;
		}
		if (bNeedsMultipleLODRuns)
		{
			for (int32 Lod = MinLod; Lod <= MaxLod; Lod++)
			{
				TotalMultipleLODInstances[Lod] += 1 + LastInstance - FirstInstance;
				AddRun(MultipleLODRuns[Lod], FirstInstance, LastInstance);
			}
		}
	}

	FORCEINLINE_DEBUGGABLE void AddRun(int32 MinLod, int32 MaxLod, const FClusterNode& Node) const
	{
		AddRun(MinLod, MaxLod, Node.FirstInstance, Node.LastInstance);
	}
};

struct FFoliageCullInstanceParams : public FFoliageRenderInstanceParams
{
	FConvexVolume ViewFrustumLocal;
	int32 MinInstancesToSplit[MAX_STATIC_MESH_LODS];
	const TArray<FClusterNode>& Tree;
	const FSceneView* View;
	FVector ViewOriginInLocalZero;
	FVector ViewOriginInLocalOne;
	int32 LODs;
	float LODPlanesMax[MAX_STATIC_MESH_LODS];
	float LODPlanesMin[MAX_STATIC_MESH_LODS];
	int32 FirstOcclusionNode;
	int32 LastOcclusionNode;
	const TArray<bool>* OcclusionResults;
	int32 OcclusionResultsStart;



	FFoliageCullInstanceParams(bool InbNeedsSingleLODRuns, bool InbNeedsMultipleLODRuns, bool InbOverestimate, const TArray<FClusterNode>& InTree)
	:	FFoliageRenderInstanceParams(InbNeedsSingleLODRuns, InbNeedsMultipleLODRuns, InbOverestimate)
	,	Tree(InTree)
	,	FirstOcclusionNode(-1)
	,	LastOcclusionNode(-1)
	,	OcclusionResults(nullptr)
	,	OcclusionResultsStart(0)
	{
	}
};

static bool GUseVectorCull = true;

static void ToggleUseVectorCull(const TArray<FString>& Args)
{
	GUseVectorCull = !GUseVectorCull;
}

static FAutoConsoleCommand ToggleUseVectorCullCmd(
	TEXT("foliage.ToggleVectorCull"),
	TEXT("Useful for debugging. Toggles the optimized cull."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&ToggleUseVectorCull)
	);

static uint32 GFrameNumberRenderThread_CaptureFoliageRuns = MAX_uint32;

static void LogFoliageFrame(const TArray<FString>& Args)
{
	GFrameNumberRenderThread_CaptureFoliageRuns = GFrameNumberRenderThread + 2;
}

static FAutoConsoleCommand LogFoliageFrameCmd(
	TEXT("foliage.LogFoliageFrame"),
	TEXT("Useful for debugging. Logs all foliage rendered in a frame."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&LogFoliageFrame)
	);

const VectorRegister		VECTOR_HALF_HALF_HALF_ZERO				= DECLARE_VECTOR_REGISTER(0.5f, 0.5f, 0.5f, 0.0f);

template<bool TUseVector>
static FORCEINLINE_DEBUGGABLE bool CullNode(const FFoliageCullInstanceParams& Params, const FClusterNode& Node, bool& bOutFullyContained)
{
	if (TUseVector)
	{
		checkSlow(Params.ViewFrustumLocal.PermutedPlanes.Num() == 4);

		//@todo, once we have more than one mesh per tree, these should be aligned
		VectorRegister BoxMin = VectorLoadFloat3(&Node.BoundMin);
		VectorRegister BoxMax = VectorLoadFloat3(&Node.BoundMax);

		VectorRegister BoxDiff = VectorSubtract(BoxMax,BoxMin);
		VectorRegister BoxSum = VectorAdd(BoxMax,BoxMin);

		// Load the origin & extent
		VectorRegister Orig = VectorMultiply(VECTOR_HALF_HALF_HALF_ZERO, BoxSum);
		VectorRegister Ext = VectorMultiply(VECTOR_HALF_HALF_HALF_ZERO, BoxDiff);
		// Splat origin into 3 vectors
		VectorRegister OrigX = VectorReplicate(Orig, 0);
		VectorRegister OrigY = VectorReplicate(Orig, 1);
		VectorRegister OrigZ = VectorReplicate(Orig, 2);
		// Splat the abs for the pushout calculation
		VectorRegister AbsExtentX = VectorReplicate(Ext, 0);
		VectorRegister AbsExtentY = VectorReplicate(Ext, 1);
		VectorRegister AbsExtentZ = VectorReplicate(Ext, 2);
		// Since we are moving straight through get a pointer to the data
		const FPlane* RESTRICT PermutedPlanePtr = (FPlane*)Params.ViewFrustumLocal.PermutedPlanes.GetData();
		// Process four planes at a time until we have < 4 left
		// Load 4 planes that are already all Xs, Ys, ...
		VectorRegister PlanesX = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesY = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesZ = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesW = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		// Calculate the distance (x * x) + (y * y) + (z * z) - w
		VectorRegister DistX = VectorMultiply(OrigX,PlanesX);
		VectorRegister DistY = VectorMultiplyAdd(OrigY,PlanesY,DistX);
		VectorRegister DistZ = VectorMultiplyAdd(OrigZ,PlanesZ,DistY);
		VectorRegister Distance = VectorSubtract(DistZ,PlanesW);
		// Now do the push out FMath::Abs(x * x) + FMath::Abs(y * y) + FMath::Abs(z * z)
		VectorRegister PushX = VectorMultiply(AbsExtentX,VectorAbs(PlanesX));
		VectorRegister PushY = VectorMultiplyAdd(AbsExtentY,VectorAbs(PlanesY),PushX);
		VectorRegister PushOut = VectorMultiplyAdd(AbsExtentZ,VectorAbs(PlanesZ),PushY);
		VectorRegister PushOutNegative = VectorNegate(PushOut);

		bOutFullyContained = !VectorAnyGreaterThan(Distance,PushOutNegative);
		// Check for completely outside
		return !!VectorAnyGreaterThan(Distance,PushOut);
	}
	FVector Center = (Node.BoundMin + Node.BoundMax) * 0.5f;
	FVector Extent = (Node.BoundMax - Node.BoundMin) * 0.5f;
	if (!Params.ViewFrustumLocal.IntersectBox(Center, Extent, bOutFullyContained)) 
	{
		return true;
	}
	return false;
}

inline void CalcLOD(int32& InOutMinLOD, int32& InOutMaxLOD, const FVector& BoundMin, const FVector& BoundMax, const FVector& ViewOriginInLocalZero, const FVector& ViewOriginInLocalOne, const float LODPlanesMin[], const float LODPlanesMax[])
{
	if (InOutMinLOD != InOutMaxLOD)
	{
		const FVector Center = (BoundMax + BoundMin) * 0.5f;
		const float DistCenterZero = FVector::Dist(Center, ViewOriginInLocalZero);
		const float DistCenterOne = FVector::Dist(Center, ViewOriginInLocalOne);
		const float HalfWidth = FVector::Dist(BoundMax, BoundMin) * 0.5f;
		const float NearDot = FMath::Min(DistCenterZero, DistCenterOne) - HalfWidth;
		const float FarDot = FMath::Max(DistCenterZero, DistCenterOne) + HalfWidth;

		while (InOutMaxLOD > InOutMinLOD && NearDot > LODPlanesMax[InOutMinLOD])
		{
			InOutMinLOD++;
		}
		while (InOutMaxLOD > InOutMinLOD && FarDot < LODPlanesMin[InOutMaxLOD - 1])
		{
			InOutMaxLOD--;
		}
	}
}

inline bool CanGroup(const FVector& BoundMin, const FVector& BoundMax, const FVector& ViewOriginInLocalZero, const FVector& ViewOriginInLocalOne, float MaxDrawDist)
{
	const FVector Center = (BoundMax + BoundMin) * 0.5f;
	const float DistCenterZero = FVector::Dist(Center, ViewOriginInLocalZero);
	const float DistCenterOne = FVector::Dist(Center, ViewOriginInLocalOne);
	const float HalfWidth = FVector::Dist(BoundMax, BoundMin) * 0.5f;
	const float FarDot = FMath::Max(DistCenterZero, DistCenterOne) + HalfWidth;

	// We are sure that everything in the bound won't be distance culled
	return FarDot < MaxDrawDist;
}

template<bool TUseVector>
void FHierarchicalStaticMeshSceneProxy::Traverse(const FFoliageCullInstanceParams& Params, int32 Index, int32 MinLOD, int32 MaxLOD, bool bFullyContained) const
{
	const FClusterNode& Node = Params.Tree[Index];
	if (!bFullyContained)
	{
		if (CullNode<TUseVector>(Params, Node, bFullyContained))
		{
			return;
		}
	}

	if (MinLOD != MaxLOD)
	{
		CalcLOD(MinLOD, MaxLOD, Node.BoundMin, Node.BoundMax, Params.ViewOriginInLocalZero, Params.ViewOriginInLocalOne, Params.LODPlanesMin, Params.LODPlanesMax);

		if (MinLOD >= Params.LODs)
		{
			return;
		}
	}

	if (Index >= Params.FirstOcclusionNode && Index <= Params.LastOcclusionNode)
	{
		check(Params.OcclusionResults != NULL);
		const TArray<bool>& OcclusionResultsArray = *Params.OcclusionResults;
		if (OcclusionResultsArray[Params.OcclusionResultsStart + Index - Params.FirstOcclusionNode])
		{
			INC_DWORD_STAT_BY(STAT_OcclusionCulledFoliageInstances, 1 + Node.LastInstance - Node.FirstInstance);
			return;
		}
	}

	bool bShouldGroup = Node.FirstChild < 0
		|| ((Node.LastInstance - Node.FirstInstance + 1) < Params.MinInstancesToSplit[MinLOD]
			&& CanGroup(Node.BoundMin, Node.BoundMax, Params.ViewOriginInLocalZero, Params.ViewOriginInLocalOne, Params.LODPlanesMax[Params.LODs - 1]));
	bool bSplit = (!bFullyContained || MinLOD < MaxLOD || Index < Params.FirstOcclusionNode)
		&& !bShouldGroup;

	if (!bSplit)
	{
		MaxLOD = FMath::Min(MaxLOD, Params.LODs - 1);
		Params.AddRun(MinLOD, MaxLOD, Node);
		return;
	}
	for (int32 ChildIndex = Node.FirstChild; ChildIndex <= Node.LastChild; ChildIndex++)
	{
		Traverse<TUseVector>(Params, ChildIndex, MinLOD, MaxLOD, bFullyContained);
	}
}

struct FFoliageElementParams
{
	const FInstancingUserData* PassUserData[2];
	int32 NumSelectionGroups;
	const FSceneView* View;
	int32 ViewIndex;
	bool bSelectionRenderEnabled;
	bool BatchRenderSelection[2];
	bool bIsWireframe;
	bool bUseHoveredMaterial;
	bool bInstanced;
	bool bBlendLODs;
	ERHIFeatureLevel::Type FeatureLevel;
	bool ShadowFrustum;
	float FinalCullDistance;
};

void FHierarchicalStaticMeshSceneProxy::FillDynamicMeshElements(FMeshElementCollector& Collector, const FFoliageElementParams& ElementParams, const FFoliageRenderInstanceParams& Params) const
{
	SCOPE_CYCLE_COUNTER(STAT_FoliageBatchTime);
	int64 TotalTriangles = 0;

	int32 OnlyLOD = FMath::Min<int32>(CVarOnlyLOD.GetValueOnRenderThread(),InstancedRenderData.VertexFactories.Num() - 1);
	int32 FirstLOD = (OnlyLOD < 0) ? 0 : OnlyLOD;
	int32 LastLODPlusOne = (OnlyLOD < 0) ? InstancedRenderData.VertexFactories.Num() : (OnlyLOD+1);

	for (int32 LODIndex = FirstLOD; LODIndex < LastLODPlusOne; LODIndex++)
	{
		const FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[LODIndex];

		for (int32 SelectionGroupIndex = 0; SelectionGroupIndex < ElementParams.NumSelectionGroups; SelectionGroupIndex++)
		{
			for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
			{
				const FLODInfo& ProxyLODInfo = LODs[LODIndex];
				UMaterialInterface* Material = ProxyLODInfo.Sections[SectionIndex].Material;
				const bool bDitherLODEnabled = ElementParams.bBlendLODs;

				TArray<uint32, SceneRenderingAllocator>& RunArray = bDitherLODEnabled ? Params.MultipleLODRuns[LODIndex] : Params.SingleLODRuns[LODIndex];

				if (!RunArray.Num())
				{
					continue;
				}

				int32 NumBatches = 1;
				int32 CurrentRun = 0;
				int32 CurrentInstance = 0;
				int32 RemainingInstances = bDitherLODEnabled ? Params.TotalMultipleLODInstances[LODIndex] : Params.TotalSingleLODInstances[LODIndex];

				if (!ElementParams.bInstanced)
				{
					NumBatches = FMath::DivideAndRoundUp(RemainingInstances, (int32)FInstancedStaticMeshVertexFactory::NumBitsForVisibilityMask());
					if (NumBatches)
					{
						check(RunArray.Num());
						CurrentInstance = RunArray[CurrentRun];
					}
				}

#if STATS
				INC_DWORD_STAT_BY(STAT_FoliageInstances, RemainingInstances);
				if (!ElementParams.bInstanced)
				{
					INC_DWORD_STAT_BY(STAT_FoliageRuns, NumBatches);
				}
#endif
				bool bDidStats = false;
				for (int32 BatchIndex = 0; BatchIndex < NumBatches; BatchIndex++)
				{
					FMeshBatch& MeshElement = Collector.AllocateMesh();
					INC_DWORD_STAT(STAT_FoliageMeshBatches);

					if (!FStaticMeshSceneProxy::GetMeshElement(LODIndex, 0, SectionIndex, GetDepthPriorityGroup(ElementParams.View), ElementParams.BatchRenderSelection[SelectionGroupIndex], ElementParams.bUseHoveredMaterial, true, MeshElement))
					{
						continue;
					}
					checkSlow(MeshElement.GetNumPrimitives() > 0);

					MeshElement.VertexFactory = &InstancedRenderData.VertexFactories[LODIndex];
					FMeshBatchElement& BatchElement0 = MeshElement.Elements[0];

					BatchElement0.UserData = ElementParams.PassUserData[SelectionGroupIndex];
					BatchElement0.bUserDataIsColorVertexBuffer = false;
					BatchElement0.MaxScreenSize = 1.0;
					BatchElement0.MinScreenSize = 0.0;
					BatchElement0.InstancedLODIndex = LODIndex;
					BatchElement0.InstancedLODRange = bDitherLODEnabled ? 1 : 0;
					BatchElement0.bIsInstancedMesh = true;
					MeshElement.bCanApplyViewModeOverrides = true;
					MeshElement.bUseSelectionOutline = ElementParams.BatchRenderSelection[SelectionGroupIndex];
					MeshElement.bUseWireframeSelectionColoring = ElementParams.BatchRenderSelection[SelectionGroupIndex];
					MeshElement.bUseAsOccluder = ShouldUseAsOccluder();

					if (!bDidStats)
					{
						bDidStats = true;
						int64 Tris = int64(RemainingInstances) * int64(BatchElement0.NumPrimitives);
						TotalTriangles += Tris;
#if STATS
						if (GFrameNumberRenderThread_CaptureFoliageRuns == GFrameNumberRenderThread)
						{
							if (ElementParams.FinalCullDistance > 9.9E8)
							{
								UE_LOG(LogStaticMesh, Display, TEXT("lod:%1d/%1d   sel:%1d   section:%1d/%1d   runs:%4d   inst:%8d   tris:%9lld   cast shadow:%1d   cull:-NONE!!-   shadow:%1d     %s %s"), 
									LODIndex, InstancedRenderData.VertexFactories.Num(), SelectionGroupIndex, SectionIndex, LODModel.Sections.Num(), RunArray.Num() / 2, 
									RemainingInstances, Tris, (int)MeshElement.CastShadow, ElementParams.ShadowFrustum,
									*StaticMesh->GetPathName(),
									*MeshElement.MaterialRenderProxy->GetMaterial(ElementParams.FeatureLevel)->GetFriendlyName());
							}
							else
							{
								UE_LOG(LogStaticMesh, Display, TEXT("lod:%1d/%1d   sel:%1d   section:%1d/%1d   runs:%4d   inst:%8d   tris:%9lld   cast shadow:%1d   cull:%8.0f   shadow:%1d     %s %s"), 
									LODIndex, InstancedRenderData.VertexFactories.Num(), SelectionGroupIndex, SectionIndex, LODModel.Sections.Num(), RunArray.Num() / 2, 
									RemainingInstances, Tris, (int)MeshElement.CastShadow, ElementParams.FinalCullDistance, ElementParams.ShadowFrustum,
									*StaticMesh->GetPathName(),
									*MeshElement.MaterialRenderProxy->GetMaterial(ElementParams.FeatureLevel)->GetFriendlyName());
							}
						}
#endif
					}
					if (ElementParams.bInstanced)
					{
						BatchElement0.NumInstances = RunArray.Num() / 2;
						BatchElement0.InstanceRuns = &RunArray[0];
						BatchElement0.bIsInstanceRuns = true;
#if STATS
						INC_DWORD_STAT_BY(STAT_FoliageRuns, BatchElement0.NumInstances);
#endif
					}
					else
					{
						uint32 NumInstancesThisBatch = FMath::Min(RemainingInstances, (int32)FInstancedStaticMeshVertexFactory::NumBitsForVisibilityMask());

						MeshElement.Elements.Reserve(NumInstancesThisBatch);
						check(NumInstancesThisBatch);

						for (uint32 Instance = 0; Instance < NumInstancesThisBatch; ++Instance)
						{
							FMeshBatchElement* NewBatchElement; 

							if (Instance == 0)
							{
								NewBatchElement = &MeshElement.Elements[0];
							}
							else
							{
								NewBatchElement = new(MeshElement.Elements) FMeshBatchElement();
								*NewBatchElement = MeshElement.Elements[0];
							}

							NewBatchElement->UserIndex = CurrentInstance;
							if (--RemainingInstances)
							{
								if ((uint32)CurrentInstance >= RunArray[CurrentRun + 1])
								{
									CurrentRun += 2;
									check(CurrentRun + 1 < RunArray.Num());
									CurrentInstance = RunArray[CurrentRun];
								}
								else
								{
									CurrentInstance++;
								}
							}
						}
					}
					if (TotalTriangles < (int64)CVarMaxTrianglesToRender.GetValueOnRenderThread())
					{
						Collector.AddMesh(ElementParams.ViewIndex, MeshElement);
					}
				}
			}
		}
	}
#if STATS
	TotalTriangles = FMath::Min<int64>(TotalTriangles, MAX_int32);
	INC_DWORD_STAT_BY(STAT_FoliageTriangles, (uint32)TotalTriangles);
	INC_DWORD_STAT_BY(STAT_StaticMeshTriangles, (uint32)TotalTriangles);
#endif
}

void FHierarchicalStaticMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	if (Views[0]->bRenderFirstInstanceOnly)
	{
		FInstancedStaticMeshSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);
		return;
	}

	QUICK_SCOPE_CYCLE_COUNTER(STAT_HierarchicalInstancedStaticMeshSceneProxy_GetMeshElements);

	bool bMultipleSections = ALLOW_DITHERED_LOD_FOR_INSTANCED_STATIC_MESHES && bDitheredLODTransitions && CVarDitheredLOD.GetValueOnRenderThread() > 0;
	bool bSingleSections = !bMultipleSections;
	bool bOverestimate = CVarOverestimateLOD.GetValueOnRenderThread() > 0;

	int32 MinVertsToSplitNode = CVarMinVertsToSplitNode.GetValueOnRenderThread();

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];

			FFoliageElementParams ElementParams;
			ElementParams.bSelectionRenderEnabled = GIsEditor && ViewFamily.EngineShowFlags.Selection;
			ElementParams.NumSelectionGroups = (ElementParams.bSelectionRenderEnabled && bHasSelectedInstances) ? 2 : 1;
			ElementParams.PassUserData[0] = bHasSelectedInstances && ElementParams.bSelectionRenderEnabled ? &UserData_SelectedInstances : &UserData_AllInstances;
			ElementParams.PassUserData[1] = &UserData_DeselectedInstances;
			ElementParams.BatchRenderSelection[0] = ElementParams.bSelectionRenderEnabled && IsSelected();
			ElementParams.BatchRenderSelection[1] = false;
			ElementParams.bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;
			ElementParams.bUseHoveredMaterial = IsHovered();
			ElementParams.bInstanced = GRHISupportsInstancing;
			ElementParams.FeatureLevel = InstancedRenderData.FeatureLevel;
			ElementParams.ViewIndex = ViewIndex;
			ElementParams.View = View;

			// Render built instances
			if (ClusterTree.Num())
			{

				FFoliageCullInstanceParams InstanceParams(bSingleSections, bMultipleSections, bOverestimate, ClusterTree);
				InstanceParams.LODs = RenderData->LODResources.Num();

				InstanceParams.View = View;

				FMatrix WorldToLocal = GetLocalToWorld().Inverse();
				bool bUseVectorCull = GUseVectorCull;
				bool bIsOrtho = false;

				bool bDisableCull = !!CVarDisableCull.GetValueOnRenderThread();
				ElementParams.ShadowFrustum = !!View->GetDynamicMeshElementsShadowCullFrustum();
				if (View->GetDynamicMeshElementsShadowCullFrustum())
				{
					for (int32 Index = 0; Index < View->GetDynamicMeshElementsShadowCullFrustum()->Planes.Num(); Index++)
					{
						FPlane Src = View->GetDynamicMeshElementsShadowCullFrustum()->Planes[Index];
						FPlane Norm = Src / Src.Size();
						// remove world space preview translation
						Norm.W -= (FVector(Norm) | View->GetPreShadowTranslation());
						FPlane Local = Norm.TransformBy(WorldToLocal);
						FPlane LocalNorm = Local / Local.Size();
						InstanceParams.ViewFrustumLocal.Planes.Add(LocalNorm);
					}
					bUseVectorCull = InstanceParams.ViewFrustumLocal.Planes.Num() == 4;
				}
				else
				{
					// Instanced stereo needs to use the right plane from the right eye when constructing the frustum bounds to cull against.
					// Otherwise we'll cull objects visible in the right eye, but not the left.
					if (Views[0]->IsInstancedStereoPass() && ViewIndex == 0)
					{
						check(Views.Num() == 2);
						
						const FMatrix LeftEyeLocalViewProjForCulling  = GetLocalToWorld() * Views[0]->ViewMatrices.GetViewProjectionMatrix();
						const FMatrix RightEyeLocalViewProjForCulling = GetLocalToWorld() * Views[1]->ViewMatrices.GetViewProjectionMatrix();

						FConvexVolume LeftEyeBounds, RightEyeBounds;
						GetViewFrustumBounds(LeftEyeBounds, LeftEyeLocalViewProjForCulling, false);
						GetViewFrustumBounds(RightEyeBounds, RightEyeLocalViewProjForCulling, false);
						
						InstanceParams.ViewFrustumLocal.Planes.Empty(5);
						InstanceParams.ViewFrustumLocal.Planes.Add(LeftEyeBounds.Planes[0]);
						InstanceParams.ViewFrustumLocal.Planes.Add(RightEyeBounds.Planes[1]);
						InstanceParams.ViewFrustumLocal.Planes.Add(LeftEyeBounds.Planes[2]);
						InstanceParams.ViewFrustumLocal.Planes.Add(LeftEyeBounds.Planes[3]);
						InstanceParams.ViewFrustumLocal.Planes.Add(LeftEyeBounds.Planes[4]);
						InstanceParams.ViewFrustumLocal.Init();
					}
					else
					{
						const FMatrix LocalViewProjForCulling = GetLocalToWorld() * View->ViewMatrices.GetViewProjectionMatrix();
						GetViewFrustumBounds(InstanceParams.ViewFrustumLocal, LocalViewProjForCulling, false);
					}

					if (View->ViewMatrices.IsPerspectiveProjection())
					{
						if (InstanceParams.ViewFrustumLocal.Planes.Num() == 5)
						{
							InstanceParams.ViewFrustumLocal.Planes.Pop(false); // we don't want the far plane either
							FMatrix ThreePlanes;
							ThreePlanes.SetIdentity();
							ThreePlanes.SetAxes(&InstanceParams.ViewFrustumLocal.Planes[0], &InstanceParams.ViewFrustumLocal.Planes[1], &InstanceParams.ViewFrustumLocal.Planes[2]);
							FVector ProjectionOrigin = ThreePlanes.Inverse().GetTransposed().TransformVector(FVector(InstanceParams.ViewFrustumLocal.Planes[0].W, InstanceParams.ViewFrustumLocal.Planes[1].W, InstanceParams.ViewFrustumLocal.Planes[2].W));

							for (int32 Index = 0; Index < InstanceParams.ViewFrustumLocal.Planes.Num(); Index++)
							{
								FPlane Src = InstanceParams.ViewFrustumLocal.Planes[Index];
								FVector Normal = Src.GetSafeNormal();
								InstanceParams.ViewFrustumLocal.Planes[Index] = FPlane(Normal, Normal | ProjectionOrigin);
							}
						}
						else
						{
							 // zero scaling or something, cull everything
							continue;
						}
					}
					else
					{
						bIsOrtho = true;
						bUseVectorCull = false;
					}
				}
				if (!InstanceParams.ViewFrustumLocal.Planes.Num())
				{
					bDisableCull = true;
				}
				else
				{
					InstanceParams.ViewFrustumLocal.Init();
				}

				ElementParams.bBlendLODs = bMultipleSections;

				InstanceParams.ViewOriginInLocalZero = WorldToLocal.TransformPosition(View->GetTemporalLODOrigin(0, bMultipleSections));
				InstanceParams.ViewOriginInLocalOne = WorldToLocal.TransformPosition(View->GetTemporalLODOrigin(1, bMultipleSections));

				float MinSize = bIsOrtho ? 0.0f : CVarFoliageMinimumScreenSize.GetValueOnRenderThread();
				float LODScale = CVarFoliageLODDistanceScale.GetValueOnRenderThread();
				float LODRandom = CVarRandomLODRange.GetValueOnRenderThread();
				float MaxDrawDistanceScale = GetCachedScalabilityCVars().ViewDistanceScale;
				float SphereRadius = RenderData->Bounds.SphereRadius;

				float FinalCull = MAX_flt;
				if (MinSize > 0.0)
				{
					FinalCull = ComputeBoundsDrawDistance(MinSize, SphereRadius, View->ViewMatrices.GetProjectionMatrix()) * LODScale;
				}
				if (UserData_AllInstances.EndCullDistance > 0.0f)
				{
					FinalCull = FMath::Min(FinalCull, UserData_AllInstances.EndCullDistance * MaxDrawDistanceScale);
				}
				ElementParams.FinalCullDistance = FinalCull;

				for (int32 LODIndex = 1; LODIndex < InstanceParams.LODs; LODIndex++)
				{
					float Distance = ComputeBoundsDrawDistance(RenderData->ScreenSize[LODIndex], SphereRadius, View->ViewMatrices.GetProjectionMatrix()) * LODScale;
					InstanceParams.LODPlanesMin[LODIndex - 1] = Distance - LODRandom;
					InstanceParams.LODPlanesMax[LODIndex - 1] = Distance;
				}
				InstanceParams.LODPlanesMin[InstanceParams.LODs - 1] = FinalCull - LODRandom;
				InstanceParams.LODPlanesMax[InstanceParams.LODs - 1] = FinalCull;
			
				for (int32 LODIndex = 0; LODIndex < InstanceParams.LODs; LODIndex++)
				{
					InstanceParams.MinInstancesToSplit[LODIndex] = 2;
					int32 NumVerts = RenderData->LODResources[LODIndex].VertexBuffer.GetNumVertices();
					if (NumVerts)
					{
						InstanceParams.MinInstancesToSplit[LODIndex] = MinVertsToSplitNode / NumVerts;
					}
				}

				if (FirstOcclusionNode >= 0 && LastOcclusionNode >= 0 && FirstOcclusionNode <= LastOcclusionNode)
				{
					uint32 ViewId = View->GetViewKey();
					const FFoliageOcclusionResults* OldResults = OcclusionResults.Find(ViewId);
					if (OldResults &&
						OldResults->FrameNumberRenderThread == GFrameNumberRenderThread &&
						1 + LastOcclusionNode - FirstOcclusionNode == OldResults->NumResults &&
						// OcclusionResultsArray[Params.OcclusionResultsStart + Index - Params.FirstOcclusionNode]

						OldResults->Results.IsValidIndex(OldResults->ResultsStart) &&
						OldResults->Results.IsValidIndex(OldResults->ResultsStart + LastOcclusionNode - FirstOcclusionNode)
						)
					{
						InstanceParams.FirstOcclusionNode = FirstOcclusionNode;
						InstanceParams.LastOcclusionNode = LastOcclusionNode;
						InstanceParams.OcclusionResults = &OldResults->Results;
						InstanceParams.OcclusionResultsStart = OldResults->ResultsStart;
					}
				}

				INC_DWORD_STAT(STAT_FoliageTraversals);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				if (GCaptureDebugRuns == GDebugTag && CaptureTag == GDebugTag)
				{
					for (int32 LODIndex = 0; LODIndex < InstanceParams.LODs; LODIndex++)
					{
						for (int32 Run = 0; Run < SingleDebugRuns[LODIndex].Num(); Run++)
						{
							InstanceParams.SingleLODRuns[LODIndex].Add(SingleDebugRuns[LODIndex][Run]);
						}
						InstanceParams.TotalSingleLODInstances[LODIndex] = SingleDebugTotalInstances[LODIndex];
						for (int32 Run = 0; Run < MultipleDebugRuns[LODIndex].Num(); Run++)
						{
							InstanceParams.MultipleLODRuns[LODIndex].Add(MultipleDebugRuns[LODIndex][Run]);
						}
						InstanceParams.TotalMultipleLODInstances[LODIndex] = MultipleDebugTotalInstances[LODIndex];
					}
				}
				else
#endif
				{
					SCOPE_CYCLE_COUNTER(STAT_FoliageTraversalTime);

					// validate that the bounding box is layed out correctly in memory
					check(&((const FVector4*)&ClusterTree[0].BoundMin)[1] == (const FVector4*)&ClusterTree[0].BoundMax);
					//check(UPTRINT(&ClusterTree[0].BoundMin) % 16 == 0);
					//check(UPTRINT(&ClusterTree[0].BoundMax) % 16 == 0);

					int32 UseMinLOD = ClampedMinLOD;

					int32 DebugMin = FMath::Min(CVarMinLOD.GetValueOnRenderThread(), InstanceParams.LODs - 1);
					if (DebugMin >= 0)
					{
						UseMinLOD = FMath::Max(UseMinLOD, DebugMin);
					}
					int32 UseMaxLOD = InstanceParams.LODs;

					int32 Force = CVarForceLOD.GetValueOnRenderThread();
					if (Force >= 0)
					{
						UseMinLOD = FMath::Clamp(Force, 0, InstanceParams.LODs - 1);
						UseMaxLOD = FMath::Clamp(Force, 0, InstanceParams.LODs - 1);
					}

					if (CVarCullAll.GetValueOnRenderThread() < 1)
					{
						if (bUseVectorCull)
						{
							Traverse<true>(InstanceParams, 0, UseMinLOD, UseMaxLOD, bDisableCull);
						}
						else
						{
							Traverse<false>(InstanceParams, 0, UseMinLOD, UseMaxLOD, bDisableCull);
						}
					}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
					if (GCaptureDebugRuns == GDebugTag && CaptureTag != GDebugTag)
					{
						CaptureTag = GDebugTag;
						for (int32 LODIndex = 0; LODIndex < InstanceParams.LODs; LODIndex++)
						{
							SingleDebugRuns[LODIndex].Empty();
							SingleDebugTotalInstances[LODIndex] = InstanceParams.TotalSingleLODInstances[LODIndex];
							for (int32 Run = 0; Run < InstanceParams.SingleLODRuns[LODIndex].Num(); Run++)
							{
								SingleDebugRuns[LODIndex].Add(InstanceParams.SingleLODRuns[LODIndex][Run]);
							}
							MultipleDebugRuns[LODIndex].Empty();
							MultipleDebugTotalInstances[LODIndex] = InstanceParams.TotalMultipleLODInstances[LODIndex];
							for (int32 Run = 0; Run < InstanceParams.MultipleLODRuns[LODIndex].Num(); Run++)
							{
								MultipleDebugRuns[LODIndex].Add(InstanceParams.MultipleLODRuns[LODIndex][Run]);
							}
						}
					}
#endif
				}

				FillDynamicMeshElements(Collector, ElementParams, InstanceParams);
			}

			// Render unbuilt instances
			if (UnbuiltInstanceCount > 0)
			{
				FFoliageRenderInstanceParams InstanceParams(true, false, false);

				// disable LOD blending for unbuilt instances as we haven't calculated the correct LOD.
				ElementParams.bBlendLODs = false;

				if (UnbuiltInstanceCount < 1000)
				{
					const int32 NumLODs = RenderData->LODResources.Num();

					int32 Force = CVarForceLOD.GetValueOnRenderThread();
					if (Force >= 0)
					{
						Force = FMath::Clamp(Force, 0, NumLODs - 1);
						InstanceParams.AddRun(Force, Force, FirstUnbuiltIndex, FirstUnbuiltIndex + UnbuiltInstanceCount);
					}
					else
					{
						FMatrix WorldToLocal = GetLocalToWorld().Inverse();
						FVector ViewOriginInLocalZero = WorldToLocal.TransformPosition(View->GetTemporalLODOrigin(0, bMultipleSections));
						FVector ViewOriginInLocalOne  = WorldToLocal.TransformPosition(View->GetTemporalLODOrigin(1, bMultipleSections));
						float LODPlanesMax[MAX_STATIC_MESH_LODS];
						float LODPlanesMin[MAX_STATIC_MESH_LODS];

						const bool bIsOrtho = !View->ViewMatrices.IsPerspectiveProjection();
						const float MinSize = bIsOrtho ? 0.0f : CVarFoliageMinimumScreenSize.GetValueOnRenderThread();
						const float LODScale = CVarFoliageLODDistanceScale.GetValueOnRenderThread();
						const float LODRandom = CVarRandomLODRange.GetValueOnRenderThread();
						const float MaxDrawDistanceScale = GetCachedScalabilityCVars().ViewDistanceScale;
						const float SphereRadius = RenderData->Bounds.SphereRadius;

						checkSlow(NumLODs > 0);

						float FinalCull = MAX_flt;
						if (MinSize > 0.0)
						{
							FinalCull = ComputeBoundsDrawDistance(MinSize, SphereRadius, View->ViewMatrices.GetProjectionMatrix()) * LODScale;
						}
						if (UserData_AllInstances.EndCullDistance > 0.0f)
						{
							FinalCull = FMath::Min(FinalCull, UserData_AllInstances.EndCullDistance * MaxDrawDistanceScale);
						}
						ElementParams.FinalCullDistance = FinalCull;

						for (int32 LODIndex = 1; LODIndex < NumLODs; LODIndex++)
						{
							float Distance = ComputeBoundsDrawDistance(RenderData->ScreenSize[LODIndex], SphereRadius, View->ViewMatrices.GetProjectionMatrix()) * LODScale;
							LODPlanesMin[LODIndex - 1] = Distance - LODRandom;
							LODPlanesMax[LODIndex - 1] = Distance;
						}
						LODPlanesMin[NumLODs - 1] = FinalCull - LODRandom;
						LODPlanesMax[NumLODs - 1] = FinalCull;				

						// calculate runs
						int32 MinLOD = 0;
						int32 MaxLOD = NumLODs;
						CalcLOD(MinLOD, MaxLOD, UnbuiltBounds[0].Min, UnbuiltBounds[0].Max, ViewOriginInLocalZero, ViewOriginInLocalOne, LODPlanesMin, LODPlanesMax);
						int32 FirstIndexInRun = 0;
						for (int32 Index = 1; Index < UnbuiltInstanceCount; ++Index)
						{
							int32 TempMinLOD = 0;
							int32 TempMaxLOD = NumLODs;
							CalcLOD(TempMinLOD, TempMaxLOD, UnbuiltBounds[Index].Min, UnbuiltBounds[Index].Max, ViewOriginInLocalZero, ViewOriginInLocalOne, LODPlanesMin, LODPlanesMax);
							if (TempMinLOD != MinLOD)
							{
								if (MinLOD < NumLODs)
								{
									InstanceParams.AddRun(MinLOD, MinLOD, FirstIndexInRun + FirstUnbuiltIndex, (Index - 1) + FirstUnbuiltIndex);
								}
								MinLOD = TempMinLOD;
								FirstIndexInRun = Index;
							}
						}
						InstanceParams.AddRun(MinLOD, MinLOD, FirstIndexInRun + FirstUnbuiltIndex, FirstIndexInRun + FirstUnbuiltIndex + UnbuiltInstanceCount);
					}
				}
				else
				{
					// more than 1000, render them all at lowest LOD (until we have an updated tree)
					InstanceParams.AddRun(RenderData->LODResources.Num() - 1, RenderData->LODResources.Num() - 1, FirstUnbuiltIndex, FirstUnbuiltIndex + UnbuiltInstanceCount);
				}
				FillDynamicMeshElements(Collector, ElementParams, InstanceParams);
			}

			if (View->Family->EngineShowFlags.FoliageOcclusionBounds)
			{
				for (auto& OcclusionBound : OcclusionBounds)
				{
					DrawWireBox(Collector.GetPDI(ViewIndex), OcclusionBound.GetBox(), FColor(255, 0, 0), View->Family->EngineShowFlags.Game ? SDPG_World : SDPG_Foreground);
				}
			}
		}
	}
}

void FHierarchicalStaticMeshSceneProxy::AcceptOcclusionResults(const FSceneView* View, TArray<bool>* Results, int32 ResultsStart, int32 NumResults)
{ 
	// Don't accept subprimitive occlusion results from a previously-created sceneproxy - the tree may have been different
	if (OcclusionBounds.Num() == NumResults && SceneProxyCreatedFrameNumberRenderThread < GFrameNumberRenderThread)
	{
		uint32 ViewId = View->GetViewKey();
		FFoliageOcclusionResults* OldResults = OcclusionResults.Find(ViewId);
		if (OldResults)
		{
			OldResults->FrameNumberRenderThread = GFrameNumberRenderThread;
			OldResults->Results = *Results;
			OldResults->ResultsStart = ResultsStart;
			OldResults->NumResults = NumResults;
		}
		else
		{
			// now is a good time to clean up any stale entries
			for (auto Iter = OcclusionResults.CreateIterator(); Iter; ++Iter)
			{
				if (Iter.Value().FrameNumberRenderThread != GFrameNumberRenderThread)
				{
					Iter.RemoveCurrent();
				}
			}
			OcclusionResults.Add(ViewId, FFoliageOcclusionResults(Results, ResultsStart, NumResults));
		}
	}
}

const TArray<FBoxSphereBounds>* FHierarchicalStaticMeshSceneProxy::GetOcclusionQueries(const FSceneView* View) const 
{
	return &OcclusionBounds;
}

FBoxSphereBounds UHierarchicalInstancedStaticMeshComponent::CalcBounds(const FTransform& BoundTransform) const
{
	ensure(BuiltInstanceBounds.IsValid || ClusterTreePtr->Num() == 0);

	if (BuiltInstanceBounds.IsValid || UnbuiltInstanceBounds.IsValid)
	{
		FBoxSphereBounds Result = BuiltInstanceBounds + UnbuiltInstanceBounds;
		return Result.TransformBy(BoundTransform);
	}
	else
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UHierarchicalInstancedStaticMeshComponent_CalcBounds_SlowPath);
		return Super::CalcBounds(BoundTransform);
	}
}

UHierarchicalInstancedStaticMeshComponent::UHierarchicalInstancedStaticMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ClusterTreePtr(MakeShareable(new TArray<FClusterNode>))
	, NumBuiltInstances(0)
	, NumBuiltRenderInstances(0)
	, UnbuiltInstanceBounds(ForceInit)
	, bEnableDensityScaling(false)
	, OcclusionLayerNumNodes(0)
	, bIsAsyncBuilding(false)
	, bDiscardAsyncBuildResults(false)
	, bConcurrentRemoval(false)
	, bAutoRebuildTreeOnInstanceChanges(true)
	, AccumulatedNavigationDirtyArea(ForceInit)
{
	bCanEverAffectNavigation = true;
	bUseAsOccluder = false;
}

UHierarchicalInstancedStaticMeshComponent::~UHierarchicalInstancedStaticMeshComponent()
{
	if (ProxySize)
	{
		DEC_DWORD_STAT_BY(STAT_FoliageInstanceBuffers, ProxySize);
	}
	ProxySize = 0;
}

#if WITH_EDITOR
void UHierarchicalInstancedStaticMeshComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	if ((PropertyChangedEvent.Property != NULL && PropertyChangedEvent.Property->GetFName() == "PerInstanceSMData") ||
		(PropertyChangedEvent.Property != NULL && PropertyChangedEvent.Property->GetFName() == "Transform"))
	{
		if (FApp::CanEverRender() && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			BuildTreeIfOutdated(true, false);
		}
	}
}
#endif

void UHierarchicalInstancedStaticMeshComponent::Serialize(FArchive& Ar)
{
	LLM_SCOPE(ELLMTag::StaticMesh);	

	// On save, if we have a pending async build we should wait for it to complete rather than saving an incomplete tree
	if (Ar.IsSaving())
	{
		if (!IsTreeFullyBuilt())
		{
			BuildTreeIfOutdated(false, true);
		}
	}

	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		ClusterTreePtr = MakeShareable(new TArray<FClusterNode>);
	}
	TArray<FClusterNode>& ClusterTree = *ClusterTreePtr;
	ClusterTree.BulkSerialize(Ar);
	if (Ar.IsLoading() && !BuiltInstanceBounds.IsValid)
	{
		BuiltInstanceBounds = (ClusterTree.Num() > 0 ? FBox(ClusterTree[0].BoundMin, ClusterTree[0].BoundMax) : FBox(ForceInit));
	}
}

void UHierarchicalInstancedStaticMeshComponent::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && bDuplicateForPIE)
	{
		BuildTreeIfOutdated(false, false);
	}
}

void UHierarchicalInstancedStaticMeshComponent::RemoveInstanceInternal(int32 InstanceIndex)
{
	PartialNavigationUpdate(InstanceIndex);

	// Save the render index
	const int32 RemovedRenderIndex = InstanceReorderTable[InstanceIndex];
	if (RemovedRenderIndex != INDEX_NONE)
	{
		RemovedInstances.Add(RemovedRenderIndex);
	}

	if (PerInstanceRenderData.IsValid())
	{
		PerInstanceRenderData->RemoveInstanceData(this, InstanceIndex);
	}

	// Remove the instance
	PerInstanceSMData.RemoveAtSwap(InstanceIndex);
	InstanceReorderTable.RemoveAtSwap(InstanceIndex);
	UnbuiltInstanceIndexList.RemoveSwap(InstanceIndex);

#if WITH_EDITOR
	if (SelectedInstances.Num())
	{
		SelectedInstances.RemoveAtSwap(InstanceIndex);
	}
#endif

	// update the physics state
	if (bPhysicsStateCreated)
	{
		// Clean up physics for removed instance
		if (InstanceBodies[InstanceIndex])
		{
			InstanceBodies[InstanceIndex]->TermBody();
			delete InstanceBodies[InstanceIndex];
		}

		int32 LastInstanceIndex = PerInstanceSMData.Num();

		if (InstanceIndex == LastInstanceIndex)
		{
			// If we removed the last instance in the array we just need to remove it from the InstanceBodies array too.
			InstanceBodies.RemoveAt(InstanceIndex);
		}
		else
		{
			if (InstanceBodies[LastInstanceIndex])
			{
				// term physics for swapped instance
				InstanceBodies[LastInstanceIndex]->TermBody();
			}

			// swap in the last instance body if we have one
			InstanceBodies.RemoveAtSwap(InstanceIndex);

			// recreate physics for the instance we swapped in the removed item's place
			if (InstanceBodies[InstanceIndex])
			{
				InitInstanceBody(InstanceIndex, InstanceBodies[InstanceIndex]);
			}
		}
	}

}

bool UHierarchicalInstancedStaticMeshComponent::RemoveInstances(const TArray<int32>& InstancesToRemove)
{
	LLM_SCOPE(ELLMTag::StaticMesh);

	if (InstancesToRemove.Num() == 0)
	{
		return true;
	}

	if (PerInstanceSMData.Num() > 0 && PerInstanceRenderData.IsValid() && PerInstanceRenderData->InstanceBuffer.GetCurrentNumInstances() == 0)
	{
		UE_LOG(LogStaticMesh, Warning, TEXT("Trying to change instance buffer for component %s, but we have no CPU copy. Set KeepInstanceBufferCPUAccess to true to keep access at the cost of memory."), *GetPathName());
		return false;
	}

	TArray<int32> SortedInstancesToRemove = InstancesToRemove;

	// Sort so RemoveAtSwaps don't alter the indices of items still to remove
	SortedInstancesToRemove.Sort(TGreater<int32>());

	if (!PerInstanceSMData.IsValidIndex(SortedInstancesToRemove[0]) || !PerInstanceSMData.IsValidIndex(SortedInstancesToRemove.Last()))
	{
		return false;
	}

	for (int32 Index : SortedInstancesToRemove)
	{
		RemoveInstanceInternal(Index);
	}

	if (bAutoRebuildTreeOnInstanceChanges)
	{
		BuildTreeIfOutdated(true, false);
	}

	MarkRenderStateDirty();

	return true;
}

bool UHierarchicalInstancedStaticMeshComponent::RemoveInstance(int32 InstanceIndex)
{
	if (!PerInstanceSMData.IsValidIndex(InstanceIndex))
	{
		return false;
	}

	if (PerInstanceSMData.Num() > 0 && PerInstanceRenderData.IsValid() && PerInstanceRenderData->InstanceBuffer.GetCurrentNumInstances() == 0)
	{
		UE_LOG(LogStaticMesh, Warning, TEXT("Trying to change instance buffer for component %s, but we have no CPU copy. Set KeepInstanceBufferCPUAccess to true to keep access at the cost of memory."), *GetPathName());
		return false;
	}

	RemoveInstanceInternal(InstanceIndex);

	if (bAutoRebuildTreeOnInstanceChanges)
	{
		BuildTreeIfOutdated(true, false);
	}

	MarkRenderStateDirty();
	
	return true;
}

bool UHierarchicalInstancedStaticMeshComponent::UpdateInstanceTransform(int32 InstanceIndex, const FTransform& NewInstanceTransform, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
	if (!PerInstanceSMData.IsValidIndex(InstanceIndex))
	{
		return false;
	}

	if (IsAsyncBuilding())
	{
		// invalidate the results of the current async build we need to modify the tree
		bConcurrentRemoval = true;
	}

	int32 RenderIndex = InstanceReorderTable[InstanceIndex];
	const FMatrix OldTransform = PerInstanceSMData[InstanceIndex].Transform;
	const FTransform NewLocalTransform = bWorldSpace ? NewInstanceTransform.GetRelativeTransform(GetComponentTransform()) : NewInstanceTransform;
	const FVector NewLocalLocation = NewLocalTransform.GetTranslation();

	// if we are only updating rotation/scale we update the instance directly in the cluster tree
	const bool bIsOmittedInstance = (RenderIndex == INDEX_NONE);
	const bool bIsBuiltInstance = !bIsOmittedInstance && RenderIndex < NumBuiltRenderInstances;
	const bool bDoInPlaceUpdate = bIsBuiltInstance && NewLocalLocation.Equals(OldTransform.GetOrigin());

	bool Result = Super::UpdateInstanceTransform(InstanceIndex, NewInstanceTransform, bWorldSpace, bMarkRenderStateDirty, bTeleport);
	
	if (Result && GetStaticMesh() != nullptr)
	{
		const FBox NewInstanceBounds = GetStaticMesh()->GetBounds().GetBox().TransformBy(NewLocalTransform);

		if (bDoInPlaceUpdate)
		{
			// If the new bounds are larger than the old ones, then expand the bounds on the tree to make sure culling works correctly
			const FBox OldInstanceBounds = GetStaticMesh()->GetBounds().GetBox().TransformBy(OldTransform);
			if (!OldInstanceBounds.IsInside(NewInstanceBounds))
			{
				BuiltInstanceBounds += NewInstanceBounds;

				MarkRenderStateDirty();
			}
		}
		else
		{
			UnbuiltInstanceBounds += NewInstanceBounds;
			UnbuiltInstanceBoundsList.Add(NewInstanceBounds);

			BuildTreeIfOutdated(true, false);
		}
	}

	return Result;
}

void UHierarchicalInstancedStaticMeshComponent::ApplyComponentInstanceData(FInstancedStaticMeshComponentInstanceData* InstancedMeshData)
{
	UInstancedStaticMeshComponent::ApplyComponentInstanceData(InstancedMeshData);

	BuildTreeIfOutdated(false, false);
}

int32 UHierarchicalInstancedStaticMeshComponent::AddInstance(const FTransform& InstanceTransform)
{
	int32 InstanceIndex = UInstancedStaticMeshComponent::AddInstance(InstanceTransform);

	if (InstanceIndex != INDEX_NONE)
	{	
		if (GetStaticMesh())
		{
			const FBox NewInstanceBounds = GetStaticMesh()->GetBounds().GetBox().TransformBy(InstanceTransform);
			UnbuiltInstanceBounds += NewInstanceBounds;
			UnbuiltInstanceBoundsList.Add(NewInstanceBounds);

			UnbuiltInstanceIndexList.Add(InstanceIndex);
		}

		if (bAutoRebuildTreeOnInstanceChanges)
		{
			BuildTreeIfOutdated(PerInstanceSMData.Num() > 1, false);
		}
	}

	return InstanceIndex;
}

void UHierarchicalInstancedStaticMeshComponent::ClearInstances()
{
	if (IsAsyncBuilding())
	{
		bConcurrentRemoval = true;
	}

	ClusterTreePtr = MakeShareable(new TArray<FClusterNode>);
	NumBuiltInstances = 0;
	NumBuiltRenderInstances = 0;
	SortedInstances.Empty();
	UnbuiltInstanceBounds.Init();
	UnbuiltInstanceBoundsList.Empty();
	UnbuiltInstanceIndexList.Empty();
	NeedUpdatingInstanceIndexList.Empty();

	if (ProxySize)
	{
		DEC_DWORD_STAT_BY(STAT_FoliageInstanceBuffers, ProxySize);
	}

	Super::ClearInstances();
}

bool UHierarchicalInstancedStaticMeshComponent::ShouldCreatePhysicsState() const
{
	if (bDisableCollision)
	{
		return false;
	}
	return Super::ShouldCreatePhysicsState();
}

int32 UHierarchicalInstancedStaticMeshComponent::GetVertsForLOD(int32 LODIndex)
{
	if (GetStaticMesh() && GetStaticMesh()->HasValidRenderData())
	{
		return GetStaticMesh()->GetNumVertices(LODIndex);
	}
	return 0;
}

int32 UHierarchicalInstancedStaticMeshComponent::DesiredInstancesPerLeaf()
{
	int32 LOD0Verts = GetVertsForLOD(0);
	int32 VertsToSplit = CVarMinVertsToSplitNode.GetValueOnAnyThread();
	if (LOD0Verts)
	{
		return FMath::Clamp(VertsToSplit / LOD0Verts, 1, 1024);
	}
	return 16;
}

float UHierarchicalInstancedStaticMeshComponent::ActualInstancesPerLeaf()
{
	const TArray<FClusterNode>& ClusterTree = *ClusterTreePtr;
	if (ClusterTree.Num())
	{
		int32 NumLeaves = 0;
		int32 NumInstances = 0;
		for (int32 Index = ClusterTree.Num() - 1; Index >= 0; Index--)
		{
			if (ClusterTree[Index].FirstChild >= 0)
			{
				break;
			}
			NumLeaves++;
			NumInstances += 1 + ClusterTree[Index].LastInstance - ClusterTree[Index].FirstInstance;
		}
		if (NumLeaves)
		{
			return float(NumInstances) / float(NumLeaves);
		}
	}
	return 0.0f;
}

void UHierarchicalInstancedStaticMeshComponent::PostBuildStats()
{
#if 0
	const TArray<FClusterNode>& ClusterTree = *ClusterTreePtr;
	check(PerInstanceRenderData.IsValid());
	bool bIsGrass = !PerInstanceSMData.Num();
	NumInst = bIsGrass ? PerInstanceRenderData->InstanceBuffer.GetNumInstances() : PerInstanceSMData.Num();

	UE_LOG(LogStaticMesh, Display, TEXT("Built a foliage hierarchy with %d instances, %d nodes, %f instances / leaf (desired %d) and %d verts in LOD0. Grass? %d "), NumInst, ClusterTree.Num(), ActualInstancesPerLeaf(), DesiredInstancesPerLeaf(), GetVertsForLOD(0), bIsGrass);
#endif
}


void UHierarchicalInstancedStaticMeshComponent::BuildTree()
{
	checkSlow(IsInGameThread());

	// If we try to build the tree with the static mesh not fully loaded, we can end up in an inconsistent state which ends in a crash later
	checkSlow(!GetStaticMesh() || !GetStaticMesh()->HasAnyFlags(RF_NeedPostLoad));

	QUICK_SCOPE_CYCLE_COUNTER(STAT_UHierarchicalInstancedStaticMeshComponent_BuildTree);
	// Verify that the mesh is valid before using it.
	const bool bMeshIsValid = 
		// make sure we have instances
		PerInstanceSMData.Num() > 0 &&
		// make sure we have an actual staticmesh
		GetStaticMesh() &&
		GetStaticMesh()->HasValidRenderData() &&
		// You really can't use hardware instancing on the consoles with multiple elements because they share the same index buffer. 
		// @todo: Level error or something to let LDs know this
		1;//GetStaticMesh()->LODModels(0).Elements.Num() == 1;

	if (bMeshIsValid)
	{
		//double StartTime = FPlatformTime::Seconds();
		// If we don't have a random seed for this instanced static mesh component yet, then go ahead and
		// generate one now.  This will be saved with the static mesh component and used for future generation
		// of random numbers for this component's instances. (Used by the PerInstanceRandom material expression)
		while (InstancingRandomSeed == 0)
		{
			InstancingRandomSeed = FMath::Rand();
		}

		TArray<FMatrix> InstanceTransforms;
		InstanceTransforms.AddUninitialized(PerInstanceSMData.Num());
		for (int32 Index = 0; Index < PerInstanceSMData.Num(); Index++)
		{
			InstanceTransforms[Index] = PerInstanceSMData[Index].Transform;
		}

		FClusterBuilder Builder(InstanceTransforms, GetStaticMesh()->GetBounds().GetBox(), DesiredInstancesPerLeaf(), ExcludedDueToDensityScaling);
		Builder.Build();

		NumBuiltInstances = Builder.Result->InstanceReorderTable.Num();
		NumBuiltRenderInstances = Builder.Result->SortedInstances.Num();
		OcclusionLayerNumNodes = Builder.Result->OutOcclusionLayerNum;
		UnbuiltInstanceBounds.Init();
		RemovedInstances.Empty();
		UnbuiltInstanceBoundsList.Empty();
		UnbuiltInstanceIndexList.Empty();
		BuiltInstanceBounds = (Builder.Result->Nodes.Num() > 0 ? FBox(Builder.Result->Nodes[0].BoundMin, Builder.Result->Nodes[0].BoundMax) : FBox(ForceInit));

		ClusterTreePtr = MakeShareable(new TArray<FClusterNode>(MoveTemp(Builder.Result->Nodes)));
		InstanceReorderTable = MoveTemp(Builder.Result->InstanceReorderTable);
		SortedInstances = MoveTemp(Builder.Result->SortedInstances);
		CacheMeshExtendedBounds = GetStaticMesh()->GetBounds();

		if (!PerInstanceRenderData.IsValid())
		{
			InitPerInstanceRenderData(false);
		}

		// Resync RenderData with newly built cluster tree so we take into account the newly generated InstanceReorderTable generated from the cluster tree
		PerInstanceRenderData->UpdateAllInstanceData(this, false);

		MarkRenderStateDirty();

		FlushAccumulatedNavigationUpdates();

		PostBuildStats();
	}
	else
	{
		ClusterTreePtr = MakeShareable(new TArray<FClusterNode>);
		NumBuiltInstances = 0;
		NumBuiltRenderInstances = 0;
		InstanceReorderTable.Empty();
		SortedInstances.Empty();
		RemovedInstances.Empty();

		UnbuiltInstanceBoundsList.Empty();
		UnbuiltInstanceIndexList.Empty();
		BuiltInstanceBounds.Init();
		CacheMeshExtendedBounds = FBoxSphereBounds();
	}

	if (bIsAsyncBuilding)
	{
		// We did a sync build while async building. The sync build is newer so we will use that.
		bDiscardAsyncBuildResults = true;
	}

}

void UHierarchicalInstancedStaticMeshComponent::BuildTreeAnyThread(
	TArray<FMatrix>& InstanceTransforms, 
	const FBox& MeshBox,
	TArray<FClusterNode>& OutClusterTree,
	TArray<int32>& OutSortedInstances,
	TArray<int32>& OutInstanceReorderTable,
	int32& OutOcclusionLayerNum,
	int32 MaxInstancesPerLeaf
	)
{
	check(MaxInstancesPerLeaf > 0);

	FClusterBuilder Builder(InstanceTransforms, MeshBox, MaxInstancesPerLeaf);
	Builder.Build();
	OutOcclusionLayerNum = Builder.Result->OutOcclusionLayerNum;

	OutClusterTree = MoveTemp(Builder.Result->Nodes);
	OutInstanceReorderTable = MoveTemp(Builder.Result->InstanceReorderTable);
	OutSortedInstances = MoveTemp(Builder.Result->SortedInstances);
}

void UHierarchicalInstancedStaticMeshComponent::AcceptPrebuiltTree(TArray<FClusterNode>& InClusterTree, int InOcclusionLayerNumNodes)
{
	checkSlow(IsInGameThread());

	QUICK_SCOPE_CYCLE_COUNTER(STAT_UHierarchicalInstancedStaticMeshComponent_AcceptPrebuiltTree);
	// this is only for prebuild data, already in the correct order
	check(!PerInstanceSMData.Num());
	NumBuiltInstances = 0;
	check(PerInstanceRenderData.IsValid());	
	NumBuiltRenderInstances = PerInstanceRenderData->InstanceBuffer.GetNumInstances();
	check(NumBuiltRenderInstances);
	UnbuiltInstanceBounds.Init();
	UnbuiltInstanceBoundsList.Empty();
	UnbuiltInstanceIndexList.Empty();
	RemovedInstances.Empty();
	ClusterTreePtr = MakeShareable(new TArray<FClusterNode>);
	InstanceReorderTable.Empty();
	SortedInstances.Empty();
	OcclusionLayerNumNodes = InOcclusionLayerNumNodes;
	BuiltInstanceBounds = (InClusterTree.Num() > 0 ? FBox(InClusterTree[0].BoundMin, InClusterTree[0].BoundMax) : FBox(ForceInit));

	// Verify that the mesh is valid before using it.
	const bool bMeshIsValid = 
		// make sure we have instances
		NumBuiltRenderInstances > 0 &&
		// make sure we have an actual staticmesh
		GetStaticMesh() &&
		GetStaticMesh()->HasValidRenderData() &&
		// You really can't use hardware instancing on the consoles with multiple elements because they share the same index buffer. 
		// @todo: Level error or something to let LDs know this
		1;//GetStaticMesh()->LODModels(0).Elements.Num() == 1;

	if(bMeshIsValid)
	{
		//double StartTime = FPlatformTime::Seconds();
		// If we don't have a random seed for this instanced static mesh component yet, then go ahead and
		// generate one now.  This will be saved with the static mesh component and used for future generation
		// of random numbers for this component's instances. (Used by the PerInstanceRandom material expression)
		while( InstancingRandomSeed == 0 )
		{
			InstancingRandomSeed = FMath::Rand();
		}
		*ClusterTreePtr = MoveTemp(InClusterTree);
		PostBuildStats();

	}
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UHierarchicalInstancedStaticMeshComponent_AcceptPrebuiltTree_Mark);

	MarkRenderStateDirty();
}

void UHierarchicalInstancedStaticMeshComponent::ApplyBuildTreeAsync(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent, TSharedRef<FClusterBuilder, ESPMode::ThreadSafe> Builder, double StartTime)
{
	checkSlow(IsInGameThread());

	bIsAsyncBuilding = false;

	if (bDiscardAsyncBuildResults)
	{
		// We did a sync build while async building. The sync build is newer so we will use that.
		bDiscardAsyncBuildResults = false;
	}
	else
	{
		if (bConcurrentRemoval)
		{
			bConcurrentRemoval = false;

			UE_LOG(LogStaticMesh, Verbose, TEXT("Discarded foliage hierarchy of %d elements build due to concurrent removal (%.1fs)"), Builder->Result->InstanceReorderTable.Num(), (float)(FPlatformTime::Seconds() - StartTime));

			// There were removes or updates while we were building, it's too slow to fix up the result now, so build async again.
			BuildTreeAsync();
		}
		else
		{
			NumBuiltInstances = Builder->Result->InstanceReorderTable.Num();
			NumBuiltRenderInstances = Builder->Result->SortedInstances.Num();

			if (NumBuiltInstances < PerInstanceSMData.Num())
			{
				// Add remap entries for unbuilt instances
				Builder->Result->InstanceReorderTable.AddUninitialized(PerInstanceSMData.Num() - NumBuiltInstances);
				for (int32 Index = NumBuiltInstances; Index < PerInstanceSMData.Num(); Index++)
				{
					Builder->Result->InstanceReorderTable[Index] = Index;
				}
			}

			ClusterTreePtr = MakeShareable(new TArray<FClusterNode>(MoveTemp(Builder->Result->Nodes)));
			TArray<FClusterNode>& ClusterTree = *ClusterTreePtr;
			InstanceReorderTable = MoveTemp(Builder->Result->InstanceReorderTable);
			SortedInstances = MoveTemp(Builder->Result->SortedInstances);
			CacheMeshExtendedBounds = GetStaticMesh()->GetBounds();
			RemovedInstances.Empty();
			OcclusionLayerNumNodes = Builder->Result->OutOcclusionLayerNum;
			BuiltInstanceBounds = (ClusterTree.Num() > 0 ? FBox(ClusterTree[0].BoundMin, ClusterTree[0].BoundMax) : FBox(ForceInit));

			UE_LOG(LogStaticMesh, Verbose, TEXT("Built a foliage hierarchy with %d of %d elements in %.1fs."), NumBuiltInstances, PerInstanceSMData.Num(), (float)(FPlatformTime::Seconds() - StartTime));

			if (NumBuiltInstances < PerInstanceSMData.Num())
			{
				// There are new outstanding instances, build again!
				UnbuiltInstanceBoundsList.RemoveAt(0, UnbuiltInstanceBoundsList.Num() - (PerInstanceSMData.Num() - NumBuiltInstances));
				UnbuiltInstanceIndexList.RemoveAt(0, UnbuiltInstanceIndexList.Num() - (PerInstanceSMData.Num() - NumBuiltInstances));
				BuildTreeAsync();
			}
			else
			{
				UnbuiltInstanceBounds.Init();
				UnbuiltInstanceBoundsList.Empty();
				UnbuiltInstanceIndexList.Empty();
				FlushAccumulatedNavigationUpdates();
			}

			// Resync RenderData with newly built cluster tree so we take into account the newly generated InstanceReorderTable generated from the cluster tree
			if (PerInstanceRenderData.IsValid())
			{
				PerInstanceRenderData->UpdateAllInstanceData(this, false);
			}

			MarkRenderStateDirty();

			PostBuildStats();
		}
	}
}

bool UHierarchicalInstancedStaticMeshComponent::BuildTreeIfOutdated(bool Async, bool ForceUpdate)
{
	if (ForceUpdate 
		|| InstanceReorderTable.Num() != PerInstanceSMData.Num()
		|| NumBuiltInstances != PerInstanceSMData.Num() 
		|| (GetStaticMesh() != nullptr && CacheMeshExtendedBounds != GetStaticMesh()->GetBounds())
		|| UnbuiltInstanceBoundsList.Num() > 0 || UnbuiltInstanceIndexList.Num() > 0
		|| GetLinkerUE4Version() < VER_UE4_REBUILD_HIERARCHICAL_INSTANCE_TREES)
	{
		if (GetStaticMesh() != nullptr && !GetStaticMesh()->HasAnyFlags(RF_NeedLoad)) // we can build the tree if the static mesh is not even loaded, and we can't call PostLoad as the load is not even done
		{
			GetStaticMesh()->ConditionalPostLoad();

			if (Async)
			{
				if (IsAsyncBuilding())
				{
					// invalidate the results of the current async build we need to modify the tree
					bConcurrentRemoval = true;
					bDiscardAsyncBuildResults = false;
				}
				else
				{
					BuildTreeAsync();
				}
			}
			else
			{
				BuildTree();
			}

			return true;
		}
	}

	return false;
}

void UHierarchicalInstancedStaticMeshComponent::BuildTreeAsync()
{
	checkSlow(IsInGameThread());

	// If we try to build the tree with the static mesh not fully loaded, we can end up in an inconsistent state which ends in a crash later
	checkSlow(!GetStaticMesh() || !GetStaticMesh()->HasAnyFlags(RF_NeedPostLoad));

	check(!bIsAsyncBuilding);

	// Verify that the mesh is valid before using it.
	const bool bMeshIsValid =
		// make sure we have instances
		PerInstanceSMData.Num() > 0 &&
		// make sure we have an actual staticmesh
		GetStaticMesh() &&
		GetStaticMesh()->HasValidRenderData() &&
		// You really can't use hardware instancing on the consoles with multiple elements because they share the same index buffer. 
		// @todo: Level error or something to let LDs know this
		1;//GetStaticMesh()->LODModels(0).Elements.Num() == 1;

	if (bMeshIsValid)
	{
		double StartTime = FPlatformTime::Seconds();
		// If we don't have a random seed for this instanced static mesh component yet, then go ahead and
		// generate one now.  This will be saved with the static mesh component and used for future generation
		// of random numbers for this component's instances. (Used by the PerInstanceRandom material expression)
		while (InstancingRandomSeed == 0)
		{
			InstancingRandomSeed = FMath::Rand();
		}

		int32 Num = PerInstanceSMData.Num();
		TArray<FMatrix> InstanceTransforms;
		InstanceTransforms.AddUninitialized(Num);
		for (int32 Index = 0; Index < Num; Index++)
		{
			InstanceTransforms[Index] = PerInstanceSMData[Index].Transform;
		}

		UE_LOG(LogStaticMesh, Verbose, TEXT("Copied %d transforms in %.3fs."), Num, float(FPlatformTime::Seconds() - StartTime));

		TSharedRef<FClusterBuilder, ESPMode::ThreadSafe> Builder(new FClusterBuilder(InstanceTransforms, GetStaticMesh()->GetBounds().GetBox(), DesiredInstancesPerLeaf(), ExcludedDueToDensityScaling));

		bIsAsyncBuilding = true;

		FGraphEventRef BuildTreeAsyncResult(
			FDelegateGraphTask::CreateAndDispatchWhenReady(FDelegateGraphTask::FDelegate::CreateRaw(&Builder.Get(), &FClusterBuilder::BuildAsync), GET_STATID(STAT_FoliageBuildTime), NULL, ENamedThreads::GameThread, ENamedThreads::AnyBackgroundThreadNormalTask));

		// add a dependent task to run on the main thread when build is complete
		FGraphEventRef UnusedAsyncResult(
			FDelegateGraphTask::CreateAndDispatchWhenReady(
			FDelegateGraphTask::FDelegate::CreateUObject(this, &UHierarchicalInstancedStaticMeshComponent::ApplyBuildTreeAsync, Builder, StartTime), GET_STATID(STAT_FoliageBuildTime),
			BuildTreeAsyncResult, ENamedThreads::GameThread, ENamedThreads::GameThread
			)
			);
	}
	else
	{
		ClusterTreePtr = MakeShareable(new TArray<FClusterNode>);
		NumBuiltInstances = 0;
		NumBuiltRenderInstances = 0;
		InstanceReorderTable.Empty();
		SortedInstances.Empty();
		RemovedInstances.Empty();
		CacheMeshExtendedBounds = FBoxSphereBounds();

		UnbuiltInstanceBoundsList.Empty();
		UnbuiltInstanceIndexList.Empty();
		BuiltInstanceBounds.Init();
	}
}

FPrimitiveSceneProxy* UHierarchicalInstancedStaticMeshComponent::CreateSceneProxy()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_HierarchicalInstancedStaticMeshComponent_CreateSceneProxy);
	SCOPE_CYCLE_COUNTER(STAT_FoliageCreateProxy);

	if (ProxySize)
	{
		DEC_DWORD_STAT_BY(STAT_FoliageInstanceBuffers, ProxySize);
	}
	ProxySize = 0;

	FlushAsyncBuildInstanceBufferTask();

	// Verify that the mesh is valid before using it.
	const bool bMeshIsValid = 
		// make sure we have instances		
		((PerInstanceRenderData.IsValid() && PerInstanceRenderData->InstanceBuffer.GetNumInstances() > 0) || bPerInstanceRenderDataWasPrebuilt) &&
		// make sure we have an actual staticmesh
		GetStaticMesh() &&
		GetStaticMesh()->HasValidRenderData() &&
		// You really can't use hardware instancing on the consoles with multiple elements because they share the same index buffer. 
		// @todo: Level error or something to let LDs know this
		1;//GetStaticMesh()->LODModels(0).Elements.Num() == 1;

	if (bMeshIsValid)
	{
		// If we don't have a random seed for this instanced static mesh component yet, then go ahead and
		// generate one now.  This will be saved with the static mesh component and used for future generation
		// of random numbers for this component's instances. (Used by the PerInstanceRandom material expression)
		while (InstancingRandomSeed == 0)
		{
			InstancingRandomSeed = FMath::Rand();
		}

		ProxySize = FStaticMeshInstanceData::GetResourceSize(PerInstanceRenderData->InstanceBuffer.GetNumInstances(), GVertexElementTypeSupport.IsSupported(VET_Half2));
		bool bIsGrass = !PerInstanceSMData.Num();
		INC_DWORD_STAT_BY(STAT_FoliageInstanceBuffers, ProxySize);
		return ::new FHierarchicalStaticMeshSceneProxy(bIsGrass, this, GetWorld()->FeatureLevel);
	}
	return nullptr;
}

void UHierarchicalInstancedStaticMeshComponent::PostLoad()
{
	SCOPE_CYCLE_COUNTER(STAT_FoliagePostLoad);

	Super::PostLoad();

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		NumBuiltRenderInstances = ClusterTreePtr.IsValid() && ClusterTreePtr->Num() > 0 ? (*ClusterTreePtr.Get())[0].LastInstance - (*ClusterTreePtr.Get())[0].FirstInstance + 1 : 0;

		if (bEnableDensityScaling && GetWorld() && GetWorld()->IsGameWorld())
		{
			const float ScalabilityDensity = FMath::Clamp(CVarFoliageDensityScale.GetValueOnGameThread(), 0.0f, 1.0f);
			if (ScalabilityDensity == 0)
			{
				// exclude all instances
				ExcludedDueToDensityScaling.Init(true, PerInstanceSMData.Num());
				NumBuiltRenderInstances = 0;
			}
			else if (ScalabilityDensity > 0.0f && ScalabilityDensity < 1.0f)
			{
				FRandomStream Rand(InstancingRandomSeed);
				ExcludedDueToDensityScaling.Init(false, PerInstanceSMData.Num());

				for (int32 i = 0; i < ExcludedDueToDensityScaling.Num(); ++i)
				{
					ExcludedDueToDensityScaling[i] = (Rand.FRand() > ScalabilityDensity);
				}
			}
		}

		// If any of the data is out of sync, build the tree now!
		BuildTreeIfOutdated(true, false);
	}
}

static void GatherInstanceTransformsInArea(const UHierarchicalInstancedStaticMeshComponent& Component, const FBox& AreaBox, int32 Child, TArray<FTransform>& InstanceData)
{
	const TArray<FClusterNode>& ClusterTree = *Component.ClusterTreePtr;
	if (ClusterTree.Num())
	{
		const FClusterNode& ChildNode = ClusterTree[Child];
		const FBox WorldNodeBox = FBox(ChildNode.BoundMin, ChildNode.BoundMax).TransformBy(Component.GetComponentTransform());
	
		if (AreaBox.Intersect(WorldNodeBox))
		{
			if (ChildNode.FirstChild < 0 || AreaBox.IsInside(WorldNodeBox))
			{
				// Unfortunately ordering of PerInstanceSMData does not match ordering of cluster tree, so we have to use remaping
				const bool bUseRemaping = Component.SortedInstances.Num() > 0;
			
				// In case there no more subdivision or node is completely encapsulated by a area box
				// add all instances to the result
				for (int32 i = ChildNode.FirstInstance; i <= ChildNode.LastInstance; ++i)
				{
					int32 SortedIdx = bUseRemaping ? Component.SortedInstances[i] : i;

					FTransform InstanceToComponent;
					if (Component.PerInstanceSMData.IsValidIndex(SortedIdx))
					{
						InstanceToComponent = FTransform(Component.PerInstanceSMData[SortedIdx].Transform);
					}
					else if (Component.PerInstanceRenderData.IsValid())
					{
						// if there's no PerInstanceSMData (e.g. for grass), we'll go ge the transform from the render buffer
						FMatrix XformMat;
						Component.PerInstanceRenderData->InstanceBuffer.GetInstanceTransform(i, XformMat);
						InstanceToComponent = FTransform(XformMat);
					}
					
					if (!InstanceToComponent.GetScale3D().IsZero())
					{
						InstanceData.Add(InstanceToComponent*Component.GetComponentTransform());
					}
				}
			}
			else
			{
				for (int32 i = ChildNode.FirstChild; i <= ChildNode.LastChild; ++i)
				{
					GatherInstanceTransformsInArea(Component, AreaBox, i, InstanceData);
				}
			}
		}
	}
}

int32 UHierarchicalInstancedStaticMeshComponent::GetOverlappingSphereCount(const FSphere& Sphere) const
{
	int32 Count = 0;
	TArray<FTransform> Transforms;
	const FBox AABB(Sphere.Center - FVector(Sphere.W), Sphere.Center + FVector(Sphere.W));
	GatherInstanceTransformsInArea(*this, AABB, 0, Transforms);
	const FBoxSphereBounds MeshBounds = GetStaticMesh()->GetBounds();

	for (const FTransform& TM : Transforms)
	{
		const FVector Center = TM.GetLocation();
		const FSphere InstanceSphere(Center, MeshBounds.SphereRadius);
		
		if (Sphere.Intersects(InstanceSphere))
		{
			++Count;
		}
	}

	return Count;
}

int32 UHierarchicalInstancedStaticMeshComponent::GetOverlappingBoxCount(const FBox& Box) const
{
	TArray<FTransform> Transforms;
	GatherInstanceTransformsInArea(*this, Box, 0, Transforms);
	
	int32 Count = 0;
	const FBoxSphereBounds MeshBounds = GetStaticMesh()->GetBounds();
	for(FTransform& T : Transforms)
	{
		const FVector Centre = T.GetLocation();
		const FBox OtherBox(FVector(Centre - MeshBounds.BoxExtent), FVector(Centre + MeshBounds.BoxExtent));

		if(Box.Intersect(OtherBox))
		{
			Count++;
		}
	}

	return Count;
}

void UHierarchicalInstancedStaticMeshComponent::GetOverlappingBoxTransforms(const FBox& Box, TArray<FTransform>& OutTransforms) const
{
	GatherInstanceTransformsInArea(*this, Box, 0, OutTransforms);

	const FBoxSphereBounds MeshBounds = GetStaticMesh()->GetBounds();
	int32 NumTransforms = OutTransforms.Num();
	for(int32 Idx = NumTransforms - 1 ; Idx >= 0 ; --Idx)
	{
		FTransform& TM = OutTransforms[Idx];
		const FVector Centre = TM.GetLocation();
		const FBox OtherBox(FVector(Centre - MeshBounds.BoxExtent), FVector(Centre + MeshBounds.BoxExtent));

		if(!Box.Intersect(OtherBox))
		{
			OutTransforms.RemoveAt(Idx);
		}
	}
}

void UHierarchicalInstancedStaticMeshComponent::GetNavigationPerInstanceTransforms(const FBox& AreaBox, TArray<FTransform>& InstanceData) const
{
	if (IsTreeFullyBuilt())
	{
		const TArray<FClusterNode>& ClusterTree = *ClusterTreePtr;
		if (ClusterTree.Num())
		{
			GatherInstanceTransformsInArea(*this, AreaBox, 0, InstanceData);
		}
	}
	else
	{
		// This area should be processed again by navigation system when cluster tree is available
		// Store smaller tile box in accumulated dirty area, so we will not unintentionally mark as dirty neighbor tiles 
		const FBox SmallTileBox = AreaBox.ExpandBy(-AreaBox.GetExtent()/2.f);
		AccumulatedNavigationDirtyArea+= SmallTileBox;
	}
}

void UHierarchicalInstancedStaticMeshComponent::PartialNavigationUpdate(int32 InstanceIdx)
{
	if (InstanceIdx == INDEX_NONE)
	{
		AccumulatedNavigationDirtyArea.Init();
		UNavigationSystem::UpdateComponentInNavOctree(*this);
	}
	else if (GetStaticMesh())
	{
		// Accumulate dirty areas and send them to navigation system once cluster tree is rebuilt
		UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
		// Check if this component is registered in navigation system
		if (NavSys && (NavSys->GetObjectsNavOctreeId(this) || NavSys->HasPendingObjectNavOctreeId(this)))
		{
			FTransform InstanceTransform(PerInstanceSMData[InstanceIdx].Transform);
			FBox InstanceBox = GetStaticMesh()->GetBounds().TransformBy(InstanceTransform*GetComponentTransform()).GetBox(); // in world space
			AccumulatedNavigationDirtyArea+= InstanceBox;
		}
	}
}

void UHierarchicalInstancedStaticMeshComponent::FlushAccumulatedNavigationUpdates()
{
	if (AccumulatedNavigationDirtyArea.IsValid)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UHierarchicalInstancedStaticMeshComponent_FlushAccumulatedNavigationUpdates);

		const TArray<FClusterNode>& ClusterTree = *ClusterTreePtr;
		UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
		// Check if this component is registered in navigation system
		if (ClusterTree.Num() && NavSys && NavSys->GetObjectsNavOctreeId(this))
		{
			FBox NewBounds = FBox(ClusterTree[0].BoundMin, ClusterTree[0].BoundMax).TransformBy(GetComponentTransform());
			NavSys->UpdateNavOctreeElementBounds(this, NewBounds, AccumulatedNavigationDirtyArea);
		}
			
		AccumulatedNavigationDirtyArea.Init();
	}
}

// recursive helper to gather all instances with locations inside the specified area. Supply a Filter to exclude leaf nodes based on the instance transform.
static void GatherInstancesOverlappingArea(const UHierarchicalInstancedStaticMeshComponent& Component, const FBox& AreaBox, int32 Child, TFunctionRef<bool(const FMatrix&)> Filter, TArray<int32>& OutInstanceIndices)
{
	const TArray<FClusterNode>& ClusterTree = *Component.ClusterTreePtr;
	const FClusterNode& ChildNode = ClusterTree[Child];
	const FBox WorldNodeBox = FBox(ChildNode.BoundMin, ChildNode.BoundMax).TransformBy(Component.GetComponentTransform());

	if (AreaBox.Intersect(WorldNodeBox))
	{
		if (ChildNode.FirstChild < 0 || AreaBox.IsInside(WorldNodeBox))
		{
			// Unfortunately ordering of PerInstanceSMData does not match ordering of cluster tree, so we have to use remaping
			const bool bUseRemaping = Component.SortedInstances.Num() > 0;

			// In case there no more subdivision or node is completely encapsulated by a area box
			// add all instances to the result
			for (int32 i = ChildNode.FirstInstance; i <= ChildNode.LastInstance; ++i)
			{
				int32 SortedIdx = bUseRemaping ? Component.SortedInstances[i] : i;
				if (Component.PerInstanceSMData.IsValidIndex(SortedIdx))
				{
					const FMatrix& Matrix = Component.PerInstanceSMData[SortedIdx].Transform;
					if (Filter(Matrix))
					{
						OutInstanceIndices.Add(SortedIdx);
					}
				}
			}
		}
		else
		{
			for (int32 i = ChildNode.FirstChild; i <= ChildNode.LastChild; ++i)
			{
				GatherInstancesOverlappingArea(Component, AreaBox, i, Filter, OutInstanceIndices);
			}
		}
	}
}

TArray<int32> UHierarchicalInstancedStaticMeshComponent::GetInstancesOverlappingSphere(const FVector& Center, float Radius, bool bSphereInWorldSpace) const
{
	if (ClusterTreePtr.IsValid() && ClusterTreePtr->Num())
	{
		TArray<int32> Result;
		FSphere Sphere(Center, Radius);
		
		FBox WorldSpaceAABB(Sphere.Center - FVector(Sphere.W), Sphere.Center + FVector(Sphere.W));
		if (bSphereInWorldSpace)
		{
			Sphere = Sphere.TransformBy(GetComponentTransform().Inverse());
		}
		else
		{
			WorldSpaceAABB = WorldSpaceAABB.TransformBy(GetComponentTransform());
		}

		const float StaticMeshBoundsRadius = GetStaticMesh()->GetBounds().SphereRadius;
		GatherInstancesOverlappingArea(*this, WorldSpaceAABB, 0,
			[Sphere, StaticMeshBoundsRadius](const FMatrix& InstanceTransform)->bool
			{
				FSphere InstanceSphere(InstanceTransform.GetOrigin(), StaticMeshBoundsRadius * InstanceTransform.GetScaleVector().GetMax());
				return Sphere.Intersects(InstanceSphere);
			},
			Result);
		return Result;
	}
	else
	{
		return Super::GetInstancesOverlappingSphere(Center, Radius, bSphereInWorldSpace);
	}
}

TArray<int32> UHierarchicalInstancedStaticMeshComponent::GetInstancesOverlappingBox(const FBox& InBox, bool bBoxInWorldSpace) const
{
	if (ClusterTreePtr.IsValid() && ClusterTreePtr->Num())
	{
		TArray<int32> Result;

		FBox WorldSpaceBox(InBox);
		FBox LocalSpaceSpaceBox(InBox);
		if (bBoxInWorldSpace)
		{
			LocalSpaceSpaceBox = LocalSpaceSpaceBox.TransformBy(GetComponentTransform().Inverse());
		}
		else
		{
			WorldSpaceBox = WorldSpaceBox.TransformBy(GetComponentTransform());
		}

		const FBox StaticMeshBox = GetStaticMesh()->GetBounds().GetBox();
		GatherInstancesOverlappingArea(*this, WorldSpaceBox, 0,
			[LocalSpaceSpaceBox, StaticMeshBox](const FMatrix& InstanceTransform)->bool
			{
				FBox InstanceBox = StaticMeshBox.TransformBy(InstanceTransform);
				return LocalSpaceSpaceBox.Intersect(InstanceBox);
			},
			Result);

		return Result;
	}
	else
	{
		return Super::GetInstancesOverlappingBox(InBox, bBoxInWorldSpace);
	}
}

static void RebuildFoliageTrees(const TArray<FString>& Args)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UE_LOG(LogConsoleResponse, Display, TEXT("Rebuild Foliage Trees"));
#endif
	for (TObjectIterator<UHierarchicalInstancedStaticMeshComponent> It; It; ++It)
	{
		UHierarchicalInstancedStaticMeshComponent* Comp = *It;
		if (Comp && !Comp->IsTemplate() && !Comp->IsPendingKill())
		{
			Comp->BuildTreeIfOutdated(false, true);
			Comp->MarkRenderStateDirty();
		}
	}
}

static FAutoConsoleCommand RebuildFoliageTreesCmd(
	TEXT("foliage.RebuildFoliageTrees"),
	TEXT("Rebuild the trees for non-grass foliage."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&RebuildFoliageTrees)
	);


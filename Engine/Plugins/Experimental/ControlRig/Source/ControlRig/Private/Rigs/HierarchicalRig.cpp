// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HierarchicalRig.h"
#include "Engine/SkeletalMesh.h"
#include "AnimationRuntime.h"
#include "AnimationCoreLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"

#define LOCTEXT_NAMESPACE "HierarchicalRig"

UHierarchicalRig::UHierarchicalRig()
{
}

#if WITH_EDITOR
void UHierarchicalRig::SetConstraints(const TArray<FTransformConstraint>& InConstraints)
{
	Constraints = InConstraints;
}

void UHierarchicalRig::BuildHierarchyFromSkeletalMesh(USkeletalMesh* SkeletalMesh)
{
	const TArray<FMeshBoneInfo>& MeshBoneInfos = SkeletalMesh->RefSkeleton.GetRawRefBoneInfo();
	const TArray<FTransform>& LocalTransforms = SkeletalMesh->RefSkeleton.GetRefBonePose();
	check(MeshBoneInfos.Num() == LocalTransforms.Num());
	
	// clear hierarchy - is this necessary? Maybe not, but it makes it simpler for not handling duplicated names and so on
	Hierarchy.Empty();

	TArray<FTransform> GlobalTransforms;
	FAnimationRuntime::FillUpComponentSpaceTransforms(SkeletalMesh->RefSkeleton, LocalTransforms, GlobalTransforms);

	const int32 BoneCount = MeshBoneInfos.Num();
	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		const FMeshBoneInfo& MeshBoneInfo = MeshBoneInfos[BoneIndex];
		const FTransform& LocalTransform = LocalTransforms[BoneIndex];
		const FTransform& GlobalTransform = GlobalTransforms[BoneIndex];
		FName ParentName;

		FConstraintNodeData NodeData;
		if (MeshBoneInfo.ParentIndex != INDEX_NONE)
		{
			ParentName = MeshBoneInfos[MeshBoneInfo.ParentIndex].Name;
			const FTransform& GlobalParentTransform = GlobalTransforms[MeshBoneInfo.ParentIndex];
			NodeData.RelativeParent = GlobalTransform.GetRelativeTransform(GlobalParentTransform);
		}
		else
		{
			NodeData.RelativeParent = GlobalTransform;
		}

		Hierarchy.Add(MeshBoneInfo.Name, ParentName, GlobalTransform, NodeData);
	}
}
#endif // WITH_EDITOR

FTransform UHierarchicalRig::GetLocalTransform(FName NodeName) const
{
	return Hierarchy.GetLocalTransformByName(NodeName);
}

FVector UHierarchicalRig::GetLocalLocation(FName NodeName) const
{
	return Hierarchy.GetLocalTransformByName(NodeName).GetLocation();
}

FRotator UHierarchicalRig::GetLocalRotation(FName NodeName) const
{
	return Hierarchy.GetLocalTransformByName(NodeName).GetRotation().Rotator();
}


FVector UHierarchicalRig::GetLocalScale(FName NodeName) const
{
	return Hierarchy.GetLocalTransformByName(NodeName).GetScale3D();
}

FTransform UHierarchicalRig::GetGlobalTransform(FName NodeName) const
{
	return Hierarchy.GetGlobalTransformByName(NodeName);
}

FTransform UHierarchicalRig::GetMappedGlobalTransform(FName NodeName) const
{
	FTransform GlobalTransform = GetGlobalTransform(NodeName);
	ApplyMappingTransform(NodeName, GlobalTransform);

	return GlobalTransform;
}

FTransform UHierarchicalRig::GetMappedLocalTransform(FName NodeName) const
{
	// should recalculate since mapping is happening in component space
	const FAnimationHierarchy& CacheHiearchy = GetHierarchy();
	int32 NodeIndex = CacheHiearchy.GetNodeIndex(NodeName);
	if (CacheHiearchy.IsValidIndex(NodeIndex))
	{
		FTransform GlobalTransform = CacheHiearchy.GetGlobalTransform(NodeIndex);
		ApplyMappingTransform(NodeName, GlobalTransform);

		int32 ParentIndex  = CacheHiearchy.GetParentIndex(NodeIndex);
		if (CacheHiearchy.IsValidIndex(ParentIndex))
		{
			FTransform ParentGlobalTransform = CacheHiearchy.GetGlobalTransform(ParentIndex);
			ApplyMappingTransform(CacheHiearchy.GetNodeName(ParentIndex), ParentGlobalTransform);
			GlobalTransform = GlobalTransform.GetRelativeTransform(ParentGlobalTransform);
		}

		GlobalTransform.NormalizeRotation();
		return GlobalTransform;
	}

	return FTransform::Identity;
}

FVector UHierarchicalRig::GetGlobalLocation(FName NodeName) const
{
	return Hierarchy.GetGlobalTransformByName(NodeName).GetLocation();
}

FRotator UHierarchicalRig::GetGlobalRotation(FName NodeName) const
{
	return Hierarchy.GetGlobalTransformByName(NodeName).GetRotation().Rotator();
}

FVector UHierarchicalRig::GetGlobalScale(FName NodeName) const
{
	return Hierarchy.GetGlobalTransformByName(NodeName).GetScale3D();
}

void UHierarchicalRig::SetLocalTransform(FName NodeName, const FTransform& Transform)
{
	Hierarchy.SetLocalTransformByName(NodeName, Transform);
}

void UHierarchicalRig::SetGlobalTransform(FName NodeName, const FTransform& Transform)
{
	int32 NodeIndex = Hierarchy.GetNodeIndex(NodeName);
	if (Hierarchy.IsValidIndex(NodeIndex))
	{
		const FTransform& OldTransform = Hierarchy.GetGlobalTransform(NodeIndex);
		if (!OldTransform.Equals(Transform))
		{
			Hierarchy.SetGlobalTransform(NodeIndex, Transform);
		}

		// we still have to call evaluate node to update all dependency even if this node didn't change
		// because constraints might have to update
		EvaluateNode(NodeName);
	}
}

void UHierarchicalRig::SetMappedGlobalTransform(FName NodeName, const FTransform& Transform)
{
	FTransform NewTransform = Transform;
	NewTransform.NormalizeRotation();
	ApplyInverseMappingTransform(NodeName, NewTransform);
	SetGlobalTransform(NodeName, NewTransform);
}

void UHierarchicalRig::SetMappedLocalTransform(FName NodeName, const FTransform& Transform)
{
	// should recalculate since mapping is happening in component space
	const FAnimationHierarchy& CacheHiearchy = GetHierarchy();
	int32 NodeIndex = CacheHiearchy.GetNodeIndex(NodeName);
	if (CacheHiearchy.IsValidIndex(NodeIndex))
	{
		FTransform GlobalTransform;
		int32 ParentIndex = CacheHiearchy.GetParentIndex(NodeIndex);
		if (CacheHiearchy.IsValidIndex(ParentIndex))
		{
			FTransform ParentGlobalTransform = CacheHiearchy.GetGlobalTransform(ParentIndex);
			ApplyMappingTransform(CacheHiearchy.GetNodeName(ParentIndex), ParentGlobalTransform);
			// have to apply to mapped transform
			GlobalTransform = Transform * ParentGlobalTransform;
		}
		else
		{
			GlobalTransform = Transform;
		}

		// inverse mapping transform
		ApplyInverseMappingTransform(NodeName, GlobalTransform);
		SetGlobalTransform(NodeName, GlobalTransform);
	}
}

void UHierarchicalRig::ApplyMappingTransform(FName NodeName, FTransform& InOutTransform) const
{
	if (NodeMappingContainer)
	{
		const FNodeMap* NodeMapping = NodeMappingContainer->GetNodeMapping(NodeName);
		if (NodeMapping)
		{
			InOutTransform = NodeMapping->SourceToTargetTransform * InOutTransform;
		}
		else
		{
			// get node data
			// @todo: do it here or create mapping for manually. Creating mapping will be more efficient
			const int32 Index = Hierarchy.GetNodeIndex(NodeName);
			if (Index != INDEX_NONE)
			{
				const FConstraintNodeData* UserData = static_cast<const FConstraintNodeData*> (Hierarchy.GetUserDataImpl(Index));
				if (UserData->LinkedNode != NAME_None)
				{
					NodeMapping = NodeMappingContainer->GetNodeMapping(UserData->LinkedNode);
					if (NodeMapping)
					{
						InOutTransform = NodeMapping->SourceToTargetTransform * InOutTransform;
					}
				}
			}
		}
	}
}

void UHierarchicalRig::ApplyInverseMappingTransform(FName NodeName, FTransform& InOutTransform) const
{
	if (NodeMappingContainer)
	{
		const FNodeMap* NodeMapping = NodeMappingContainer->GetNodeMapping(NodeName);
		if (NodeMapping)
		{
			InOutTransform = NodeMapping->SourceToTargetTransform.GetRelativeTransformReverse(InOutTransform);
		}
		else
		{
			// get node data
			// @todo: do it here or create mapping for manually. Creating mapping will be more efficient
			const int32 Index = Hierarchy.GetNodeIndex(NodeName);
			if (Index != INDEX_NONE)
			{
				const FConstraintNodeData* UserData = static_cast<const FConstraintNodeData*> (Hierarchy.GetUserDataImpl(Index));
				if (UserData->LinkedNode != NAME_None)
				{
					NodeMapping = NodeMappingContainer->GetNodeMapping(UserData->LinkedNode);
					if (NodeMapping)
					{
						InOutTransform = NodeMapping->SourceToTargetTransform.GetRelativeTransformReverse(InOutTransform);
					}
				}
			}
		}
	}
}

#if WITH_EDITOR
FText UHierarchicalRig::GetCategory() const
{
	return LOCTEXT("HierarchicalRigCategory", "Animation|ControlRigs");
}

FText UHierarchicalRig::GetTooltipText() const
{
	return LOCTEXT("HierarchicalRigTooltip", "Handles hierarchical (node based) data, constraints etc.");
}
#endif

void UHierarchicalRig::GetTickDependencies(TArray<FTickPrerequisite, TInlineAllocator<1>>& OutTickPrerequisites)
{
	if (SkeletalMeshComponent.IsValid())
	{
		OutTickPrerequisites.Add(FTickPrerequisite(SkeletalMeshComponent.Get(), SkeletalMeshComponent->PrimaryComponentTick));
	}
}

void UHierarchicalRig::Initialize()
{
	Super::Initialize();

	// Initialize any manipulators we have
	for (UControlManipulator* Manipulator : Manipulators)
	{
		if (Manipulator)
		{
#if WITH_EDITOR
			TGuardValue<bool> ScopeGuard(Manipulator->bNotifyListeners, false);
#endif		
			Manipulator->Initialize(this);
			if (Hierarchy.Contains(Manipulator->Name))
			{
				if (Manipulator->bInLocalSpace)
				{
					// do not add node in initialize, that is only for editor purpose and to serialize
					Manipulator->SetTransform(GetMappedLocalTransform(Manipulator->Name), this);
				}
				else
				{
					// do not add node in initialize, that is only for editor purpose and to serialize
					Manipulator->SetTransform(GetMappedGlobalTransform(Manipulator->Name), this);
				}
			}
		}
	}

	Sort();
}

AActor* UHierarchicalRig::GetHostingActor() const
{
	if (SkeletalMeshComponent.Get())
	{
		return SkeletalMeshComponent->GetOwner();
	}

	return Super::GetHostingActor();
}

void UHierarchicalRig::BindToObject(UObject* InObject)
{
	// If we are binding to an actor, find the first skeletal mesh component
	if (AActor* Actor = Cast<AActor>(InObject))
	{
		if (USkeletalMeshComponent* Component = Actor->FindComponentByClass<USkeletalMeshComponent>())
		{
			SkeletalMeshComponent = Component;
		}
	}
	else if (USkeletalMeshComponent* Component = Cast<USkeletalMeshComponent>(InObject))
	{
		SkeletalMeshComponent = Component;
	}
}

void UHierarchicalRig::UnbindFromObject()
{
	SkeletalMeshComponent = nullptr;
}

bool UHierarchicalRig::IsBoundToObject(UObject* InObject) const
{
	if (AActor* Actor = Cast<AActor>(InObject))
	{
		if (USkeletalMeshComponent* Component = Actor->FindComponentByClass<USkeletalMeshComponent>())
		{
			return SkeletalMeshComponent.Get() == Component;
		}
	}
	else if (USkeletalMeshComponent* Component = Cast<USkeletalMeshComponent>(InObject))
	{
		return SkeletalMeshComponent.Get() == Component;
	}

	return false;
}

UObject* UHierarchicalRig::GetBoundObject() const
{
	return SkeletalMeshComponent.Get();
}

void UHierarchicalRig::PreEvaluate()
{
	Super::PreEvaluate();

	// @todo: sequencer will need this - 
	// Propagate manipulators to nodes
	for (UControlManipulator* Manipulator : Manipulators)
	{
		if (Manipulator)
		{
			if (Manipulator->bInLocalSpace)
			{
				SetMappedLocalTransform(Manipulator->Name, Manipulator->GetTransform(this));
			}
			else
			{
				SetMappedGlobalTransform(Manipulator->Name, Manipulator->GetTransform(this));
			}
		}
	}
}

void UHierarchicalRig::Evaluate()
{
	Super::Evaluate();
}

// we don't really have to update nodes as they're updated by manipulator anyway if input
// but I'm leaving here as a function just in case in the future this came up again
void UHierarchicalRig::UpdateNodes()
{
	// Calculate each node
	for (const FName& NodeName : SortedNodes)
	{
		const FTransform& GlobalTransfrom = GetGlobalTransform(NodeName);
		SetGlobalTransform(NodeName, GlobalTransfrom);
	}
}

void UHierarchicalRig::PostEvaluate()
{
	Super::PostEvaluate();

	UpdateManipulatorToNode(false);
}

void UHierarchicalRig::UpdateManipulatorToNode(bool bNotifyListeners)
{
	// Propagate back to manipulators after evaluation
	for (UControlManipulator* Manipulator : Manipulators)
	{
		if (Manipulator)
		{
#if WITH_EDITOR
			TGuardValue<bool> ScopeGuard(Manipulator->bNotifyListeners, bNotifyListeners);
#endif

			if (Manipulator->bInLocalSpace)
			{
				// do not add node in initialize, that is only for editor purpose and to serialize
				Manipulator->SetTransform(GetMappedLocalTransform(Manipulator->Name), this);
			}
			else
			{
				// do not add node in initialize, that is only for editor purpose and to serialize
				Manipulator->SetTransform(GetMappedGlobalTransform(Manipulator->Name), this);
			}
		}
	}
}

#if WITH_EDITOR
void UHierarchicalRig::AddNode(FName NodeName, FName ParentName, const FTransform& GlobalTransform, FName LinkedNode /*= NAME_None*/)
{
	FConstraintNodeData NewNodeData;
	NewNodeData.LinkedNode = LinkedNode;

	if (ParentName != NAME_None)
	{
		FTransform ParentTransform = Hierarchy.GetGlobalTransformByName(ParentName);
		NewNodeData.RelativeParent = GlobalTransform.GetRelativeTransform(ParentTransform);
	}
	else
	{
		NewNodeData.RelativeParent = GlobalTransform;
	}

	Hierarchy.Add(NodeName, ParentName, GlobalTransform, NewNodeData);
}

void UHierarchicalRig::SetParent(FName NodeName, FName NewParentName)
{
	if (Hierarchy.Contains(NodeName) && (NewParentName == NAME_None || Hierarchy.Contains(NewParentName)))
	{
		const int32 NodeIndex = Hierarchy.GetNodeIndex(NodeName);
		check(NodeIndex != INDEX_NONE);
		const FTransform NodeTransform = Hierarchy.GetGlobalTransform(NodeIndex);

		Hierarchy.SetParentName(NodeIndex, NewParentName);

		FConstraintNodeData& MyNodeData = Hierarchy.GetNodeData<FConstraintNodeData>(NodeIndex);
		if (NewParentName != NAME_None)
		{
			FTransform ParentTransform = Hierarchy.GetGlobalTransformByName(NewParentName);
			MyNodeData.RelativeParent = NodeTransform.GetRelativeTransform(ParentTransform);
		}
		else
		{
			MyNodeData.RelativeParent = NodeTransform;
		}
	}
}

void UHierarchicalRig::DeleteConstraint(FName NodeName, FName TargetNode)
{
	int32 NodeIndex = Hierarchy.GetNodeIndex(NodeName);
	if (Hierarchy.IsValidIndex(NodeIndex))
	{
		FConstraintNodeData& NodeData = Hierarchy.GetNodeData<FConstraintNodeData>(NodeIndex);
		NodeData.DeleteConstraint(TargetNode);
	}
}

void UHierarchicalRig::DeleteNode(FName NodeName)
{
	int32 NodeIndex = Hierarchy.GetNodeIndex(NodeName);
	if (Hierarchy.IsValidIndex(NodeIndex))
	{
		TArray<FName> Children = Hierarchy.GetChildren(NodeIndex);
		FName ParentName = Hierarchy.GetParentName(NodeIndex);
		int32 ParentIndex = Hierarchy.GetNodeIndex(ParentName);
		FTransform ParentTransform = (ParentIndex != INDEX_NONE)? Hierarchy.GetGlobalTransform(ParentIndex) : FTransform::Identity;

		// now reparent the children
		for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
		{
			int32 ChildNodeIndex = Hierarchy.GetNodeIndex(Children[ChildIndex]);
			Hierarchy.SetParentName(ChildNodeIndex, ParentName);

			// when delete, we have to re-adjust relative transform
			FTransform ChildTransform = Hierarchy.GetGlobalTransform(ChildNodeIndex);
			FConstraintNodeData& ChildNodeData = Hierarchy.GetNodeData<FConstraintNodeData>(ChildNodeIndex);
			ChildNodeData.RelativeParent = ChildTransform.GetRelativeTransform(ParentTransform);
		}

		Hierarchy.Remove(NodeName);
	}
}

FNodeChain UHierarchicalRig::MakeNodeChain(FName RootNode, FName EndNode)
{
	FNodeChain NodeChain;

	// walk up hierarchy towards root from end to start
	FName BoneName = EndNode;
	while (BoneName != RootNode)
	{
		// we hit the root, so clear the bone chain - we have an invalid chain
		if (BoneName == NAME_None)
		{
			NodeChain.Nodes.Reset();
			break;
		}

		NodeChain.Nodes.EmplaceAt(0, BoneName);

		int32 NodeIndex = Hierarchy.GetNodeIndex(BoneName);
		if (NodeIndex == INDEX_NONE)
		{
			NodeChain.Nodes.Reset();
			break;
		}

		BoneName = Hierarchy.GetParentName(NodeIndex);
	}

	return NodeChain;
}

UControlManipulator* UHierarchicalRig::AddManipulator(TSubclassOf<UControlManipulator> ManipulatorClass, FText DisplayName, FName NodeName, FName PropertyToManipulate, EIKSpaceMode KinematicSpace, bool bUsesTranslation, bool bUsesRotation, bool bUsesScale, bool bInLocalSpace)
{
	// make sure manipulator doesn't exists already
	for (int32 ManipulatorIndex = 0; ManipulatorIndex < Manipulators.Num(); ++ManipulatorIndex)
	{
		if (UControlManipulator* Manipulator = Manipulators[ManipulatorIndex])
		{
			if (Manipulator->Name == NodeName)
			{
				// same name exists, failed, return
				return Manipulator;
			}
		}
	}

	UControlManipulator* NewManipulator = NewObject<UControlManipulator>(this, ManipulatorClass.Get(), NAME_None, RF_Public | RF_Transactional | RF_ArchetypeObject);
	NewManipulator->DisplayName = DisplayName;
	NewManipulator->Name = NodeName;
	NewManipulator->PropertyToManipulate = PropertyToManipulate;
	NewManipulator->KinematicSpace = KinematicSpace;
	NewManipulator->bUsesTranslation = bUsesTranslation;
	NewManipulator->bUsesRotation = bUsesRotation;
	NewManipulator->bUsesScale = bUsesScale;
	NewManipulator->bInLocalSpace = bInLocalSpace;

	Manipulators.Add(NewManipulator);

	return NewManipulator;
}

void UHierarchicalRig::UpdateConstraints()
{
	if (Constraints.Num() > 0)
	{
		for (const FTransformConstraint& Constraint : Constraints)
		{
			AddConstraint(Constraint);
		}
	}
}
#endif // WITH_EDITOR

void UHierarchicalRig::AddConstraint(const FTransformConstraint& TransformConstraint)
{
	int32 NodeIndex = Hierarchy.GetNodeIndex(TransformConstraint.SourceNode);
	int32 ConstraintNodeIndex = Hierarchy.GetNodeIndex(TransformConstraint.TargetNode);

	if (NodeIndex != INDEX_NONE && ConstraintNodeIndex != INDEX_NONE)
	{
		FConstraintNodeData& NodeData = Hierarchy.GetNodeData<FConstraintNodeData>(NodeIndex);
		NodeData.AddConstraint(TransformConstraint);

		// recalculate main offset for all constraint
		if (TransformConstraint.bMaintainOffset)
		{
			int32 ParentIndex = Hierarchy.GetParentIndex(NodeIndex);
			FTransform ParentTransform = (ParentIndex != INDEX_NONE) ? Hierarchy.GetGlobalTransform(ParentIndex) : FTransform::Identity;
			FTransform LocalTransform = Hierarchy.GetLocalTransform(NodeIndex);
			FTransform TargetTransform = ResolveConstraints(LocalTransform, ParentTransform, NodeData);
			NodeData.ConstraintOffset.SaveInverseOffset(LocalTransform, TargetTransform, TransformConstraint.Operator);
		}
		else
		{
			NodeData.ConstraintOffset.Reset();
		}
	}
}

static int32& EnsureNodeExists(TMap<FName, int32>& InGraph, FName Name)
{
	int32* Value = InGraph.Find(Name);
	if (!Value)
	{
		Value = &InGraph.Add(Name);
		*Value = 0;
	}

	return *Value;
}

static void IncreaseEdgeCount(TMap<FName, int32>& InGraph, FName Name)
{
	int32& EdgeCount = EnsureNodeExists(InGraph, Name);
	++EdgeCount;
}

void UHierarchicalRig::AddDependenciesRecursive(int32 OriginalNodeIndex, int32 NodeIndex)
{
	const FName& NodeName = Hierarchy.GetNodeName(NodeIndex);

	TArray<FName> Neighbors;
	GetDependentArray(NodeName, Neighbors);

	for (const FName& Neighbor : Neighbors)
	{
		int32 NeighborNodeIndex = Hierarchy.GetNodeIndex(Neighbor);
		if (NeighborNodeIndex != INDEX_NONE)
		{
			TArray<int32>& NodeDependencies = DependencyGraph[NeighborNodeIndex];
			NodeDependencies.AddUnique(OriginalNodeIndex);
			AddDependenciesRecursive(OriginalNodeIndex, NeighborNodeIndex);
		}
	}
}

void UHierarchicalRig::CreateSortedNodes()
{
	// Comparison Operator for Sorting.
	struct FSortDependencyGraph
	{
	private:
		TArray<int32>* SortedNodeIndices;

	public:
		FSortDependencyGraph(TArray<int32>* InSortedNodeIndices)
			: SortedNodeIndices(InSortedNodeIndices)
		{
		}

		FORCEINLINE bool operator()(const int32& A, const int32& B) const
		{
			return SortedNodeIndices->Find(A) > SortedNodeIndices->Find(B);
		}
	};

	SortedNodes.Reset();

	// name to incoming edge
	TMap<FName, int32> Graph;

	// calculate incoming edges
	for (int32 NodeIndex = 0; NodeIndex < Hierarchy.GetNum(); ++NodeIndex)
	{
		const FName& NodeName = Hierarchy.GetNodeName(NodeIndex);
		EnsureNodeExists(Graph, NodeName);

		TArray<FName> Neighbors;
		GetDependentArray(NodeName, Neighbors);
		// @todo: can include itself?
		for (int32 NeighborsIndex = 0; NeighborsIndex < Neighbors.Num(); ++NeighborsIndex)
		{
			IncreaseEdgeCount(Graph, Neighbors[NeighborsIndex]);
		}
	}

	// do run Kahn
	TArray<FName> SortingQueue;
	TArray<int32> SortedNodeIndices;
	// first remove 0 in-degree vertices
	for (const TPair<FName, int32>& GraphPair : Graph)
	{
		if (GraphPair.Value == 0)
		{
			SortingQueue.Add(GraphPair.Key);
		}
	}

	// if sorting queue is same as node count, that means nothing is dependent
	if (SortingQueue.Num() == Hierarchy.GetNum())
	{
		SortedNodes = SortingQueue;
	}
	else
	{
		while (SortingQueue.Num() != 0)
		{
			FName Name = SortingQueue[0];

			// move the element
			SortingQueue.Remove(Name);
			SortedNodes.Add(Name);
			SortedNodeIndices.Add(Hierarchy.GetNodeIndex(Name));

			TArray<FName> Neighbors;

			GetDependentArray(Name, Neighbors);

			for (int32 NeighborsIndex = 0; NeighborsIndex < Neighbors.Num(); ++NeighborsIndex)
			{
				int32* EdgeCount = Graph.Find(Neighbors[NeighborsIndex]);
				--(*EdgeCount);
				if (*EdgeCount == 0)
				{
					SortingQueue.Add(Neighbors[NeighborsIndex]);
				}
			}
		}

		DependencyGraph.Reset();
		if (SortedNodes.Num() == Hierarchy.GetNum())
		{
			// If the sorted node count is different to the hierarchy we have a cycle, so we dont regenerate the graph in that case
			// Now generate a path through the DAG for each node to evaluate
			DependencyGraph.SetNum(Hierarchy.GetNum());
			for (int32 NodeIndex = 0; NodeIndex < Hierarchy.GetNum(); ++NodeIndex)
			{
				AddDependenciesRecursive(NodeIndex, NodeIndex);
			}

			// after this, we should sort using SortedNodes.
			for (int32 NodeIndex = 0; NodeIndex < Hierarchy.GetNum(); ++NodeIndex)
			{
				DependencyGraph[NodeIndex].Sort(FSortDependencyGraph(&SortedNodeIndices));
			}
		}
	}
}

void UHierarchicalRig::ApplyConstraint(const FName& NodeName)
{
	int32 NodeIndex = Hierarchy.GetNodeIndex(NodeName);
	if (NodeIndex != INDEX_NONE)
	{
		FConstraintNodeData& NodeData = Hierarchy.GetNodeData<FConstraintNodeData>(NodeIndex);
		if (NodeData.DoesHaveConstraint())
		{
			FTransform LocalTransform = Hierarchy.GetLocalTransform(NodeIndex);
			int32 ParentIndex = Hierarchy.GetParentIndex(NodeIndex);
			FTransform ParentTransform = (ParentIndex != INDEX_NONE)? Hierarchy.GetGlobalTransform(ParentIndex) : FTransform::Identity;
			FTransform ConstraintTransform = ResolveConstraints(LocalTransform, ParentTransform, NodeData);
			NodeData.ConstraintOffset.ApplyInverseOffset(ConstraintTransform, LocalTransform);
			Hierarchy.SetGlobalTransform(NodeIndex, LocalTransform * ParentTransform);
		}
	}
}

void UHierarchicalRig::EvaluateNode(const FName& NodeName)
{
	// constraints have to update when current transform changes - I think that should happen before here
	ApplyConstraint(NodeName);

	int32 NodeIndex = Hierarchy.GetNodeIndex(NodeName);
	if (NodeIndex != INDEX_NONE && NodeIndex < DependencyGraph.Num())
	{
		for(int32 ChildNodeIndex : DependencyGraph[NodeIndex])
		{
			FName ChildNodeName = Hierarchy.GetNodeName(ChildNodeIndex);
			int32 ParentIndex = Hierarchy.GetParentIndex(ChildNodeIndex);
			if (ParentIndex != INDEX_NONE)
			{
				FTransform ParentTransform = Hierarchy.GetGlobalTransform(ParentIndex);

				// Note we don't call SetGlobalTransform here as the local transform has not changed, 
				// so we dont want to introduce error
				Hierarchy.GetTransforms()[ChildNodeIndex] = Hierarchy.GetLocalTransform(ChildNodeIndex) * ParentTransform;
			}
			ApplyConstraint(ChildNodeName);
		}
	}
}

void UHierarchicalRig::GetDependentArray(const FName& Name, TArray<FName>& OutList) const
{
	int32 NodeIndex = Hierarchy.GetNodeIndex(Name);

	OutList.Reset();
	if (NodeIndex != INDEX_NONE)
	{
		FName ParentName = Hierarchy.GetParentName(NodeIndex);

		if (ParentName != NAME_None)
		{
			OutList.AddUnique(ParentName);
		}

		const FConstraintNodeData& NodeData = Hierarchy.GetNodeData<FConstraintNodeData>(NodeIndex);
		const TArray<FTransformConstraint>& NodeConstraints = NodeData.GetConstraints();
		for (int32 ConstraintsIndex = 0; ConstraintsIndex < NodeConstraints.Num(); ++ConstraintsIndex)
		{
			if (NodeConstraints[ConstraintsIndex].TargetNode != NAME_None)
			{
				OutList.AddUnique(NodeConstraints[ConstraintsIndex].TargetNode);
			}
		}
	}
}

FTransform UHierarchicalRig::ResolveConstraints(const FTransform& LocalTransform, const FTransform& ParentTransform, const FConstraintNodeData& NodeData)
{
	FTransform CurrentLocalTransform = LocalTransform;
	FGetGlobalTransform OnGetGlobalTransform;
	OnGetGlobalTransform.BindUObject(this, &UHierarchicalRig::GetGlobalTransform);
	return AnimationCore::SolveConstraints(CurrentLocalTransform, ParentTransform, NodeData.GetConstraints(), OnGetGlobalTransform);
}

void UHierarchicalRig::Sort()
{
	CreateSortedNodes();
}

UControlManipulator* UHierarchicalRig::FindManipulator(const FName& Name)
{
	for (UControlManipulator* Manipulator : Manipulators)
	{
		if (Manipulator && Manipulator->Name == Name)
		{
			return Manipulator;
		}
	}

	return nullptr;
}

void UHierarchicalRig::GetMappableNodeData(TArray<FName>& OutNames, TArray<FTransform>& OutTransforms) const
{
	OutNames.Reset();
	OutTransforms.Reset();

	// now add all nodes
	TArray<FNodeObject> Nodes = Hierarchy.GetNodes();
	const TArray<FTransform>& Transforms = Hierarchy.GetTransforms();

	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const FConstraintNodeData* UserData = static_cast<const FConstraintNodeData*> (Hierarchy.GetUserDataImpl(Index));
		// if no node is linked, we only allow them to map, so add them
		if (UserData->LinkedNode == NAME_None)
		{
			OutNames.Add(Nodes[Index].Name);
			OutTransforms.Add(Transforms[Index]);
		}
	}
}

bool UHierarchicalRig::RenameNode(const FName& CurrentNodeName, const FName& NewNodeName)
{
	if (Hierarchy.Contains(NewNodeName))
	{
		// the name is already used
		return false;
	}

	if (Hierarchy.Contains(CurrentNodeName))
	{
		const int32 NodeIndex = Hierarchy.GetNodeIndex(CurrentNodeName);
		Hierarchy.SetNodeName(NodeIndex, NewNodeName);

		// now updates Constraints as well as data Constraints
		for (int32 Index = 0; Index < Hierarchy.UserData.Num(); ++Index)
		{
			FConstraintNodeData& ConstraintNodeData = Hierarchy.UserData[Index];

			if (ConstraintNodeData.LinkedNode == CurrentNodeName)
			{
				ConstraintNodeData.LinkedNode = NewNodeName;
			}

			FTransformConstraint* Constraint = ConstraintNodeData.FindConstraint(CurrentNodeName);
			if (Constraint)
			{
				Constraint->TargetNode = NewNodeName;
			}
		}

		for (int32 Index = 0; Index < Constraints.Num(); ++Index)
		{
			FTransformConstraint& Constraint = Constraints[Index];
			if (Constraint.SourceNode == CurrentNodeName)
			{
				Constraint.SourceNode = NewNodeName;
			}

			if (Constraint.TargetNode == CurrentNodeName)
			{
				Constraint.TargetNode = NewNodeName;
			}
		}

		for (int32 Index = 0; Index < Manipulators.Num(); ++Index)
		{
			UControlManipulator* Manipulator = Manipulators[Index];
			if (Manipulator)
			{
				if (Manipulator->Name == CurrentNodeName)
				{
					Manipulator->Name = NewNodeName;
				}
			}
		}

		return true;
	}

	return false;
}

void UHierarchicalRig::Setup()
{
	//Initialize();
}
#undef LOCTEXT_NAMESPACE
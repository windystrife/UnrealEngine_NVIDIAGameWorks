// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/BehaviorTreeTypes.h"
#include "GameFramework/Actor.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_NativeEnum.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Rotator.h"
#include "VisualLogger/VisualLogger.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Class.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_String.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BTCompositeNode.h"

//----------------------------------------------------------------------//
// FBehaviorTreeInstance
//----------------------------------------------------------------------//
void FBehaviorTreeInstance::Initialize(UBehaviorTreeComponent& OwnerComp, UBTCompositeNode& Node, int32& InstancedIndex, EBTMemoryInit::Type InitType)
{
	for (int32 ServiceIndex = 0; ServiceIndex < Node.Services.Num(); ServiceIndex++)
	{
		Node.Services[ServiceIndex]->InitializeInSubtree(OwnerComp, Node.Services[ServiceIndex]->GetNodeMemory<uint8>(*this), InstancedIndex, InitType);
	}

	uint8* NodeMemory = Node.GetNodeMemory<uint8>(*this);
	Node.InitializeInSubtree(OwnerComp, NodeMemory, InstancedIndex, InitType);

	UBTCompositeNode* InstancedComposite = Cast<UBTCompositeNode>(Node.GetNodeInstance(OwnerComp, NodeMemory));
	if (InstancedComposite)
	{
		InstancedComposite->InitializeComposite(Node.GetLastExecutionIndex());
	}

	for (int32 ChildIndex = 0; ChildIndex < Node.Children.Num(); ChildIndex++)
	{
		FBTCompositeChild& ChildInfo = Node.Children[ChildIndex];

		for (int32 DecoratorIndex = 0; DecoratorIndex < ChildInfo.Decorators.Num(); DecoratorIndex++)
		{
			UBTDecorator* DecoratorOb = ChildInfo.Decorators[DecoratorIndex];
			uint8* DecoratorMemory = DecoratorOb->GetNodeMemory<uint8>(*this);
			DecoratorOb->InitializeInSubtree(OwnerComp, DecoratorMemory, InstancedIndex, InitType);

			UBTDecorator* InstancedDecoratorOb = Cast<UBTDecorator>(DecoratorOb->GetNodeInstance(OwnerComp, DecoratorMemory));
			if (InstancedDecoratorOb)
			{
				InstancedDecoratorOb->InitializeParentLink(DecoratorOb->GetChildIndex());
			}
		}

		if (ChildInfo.ChildComposite)
		{
			Initialize(OwnerComp, *(ChildInfo.ChildComposite), InstancedIndex, InitType);
		}
		else if (ChildInfo.ChildTask)
		{
			for (int32 ServiceIndex = 0; ServiceIndex < ChildInfo.ChildTask->Services.Num(); ServiceIndex++)
			{
				UBTService* ServiceOb = ChildInfo.ChildTask->Services[ServiceIndex];
				uint8* ServiceMemory = ServiceOb->GetNodeMemory<uint8>(*this);
				ServiceOb->InitializeInSubtree(OwnerComp, ServiceMemory, InstancedIndex, InitType);

				UBTService* InstancedServiceOb = Cast<UBTService>(ServiceOb->GetNodeInstance(OwnerComp, ServiceMemory));
				if (InstancedServiceOb)
				{
					InstancedServiceOb->InitializeParentLink(ServiceOb->GetChildIndex());
				}
			}

			ChildInfo.ChildTask->InitializeInSubtree(OwnerComp, ChildInfo.ChildTask->GetNodeMemory<uint8>(*this), InstancedIndex, InitType);
		}
	}
}

void FBehaviorTreeInstance::Cleanup(UBehaviorTreeComponent& OwnerComp, EBTMemoryClear::Type CleanupType)
{
	FBehaviorTreeInstanceId& Info = OwnerComp.KnownInstances[InstanceIdIndex];
	if (Info.FirstNodeInstance >= 0)
	{
		const int32 MaxAllowedIdx = OwnerComp.NodeInstances.Num();
		const int32 LastNodeIdx = OwnerComp.KnownInstances.IsValidIndex(InstanceIdIndex + 1) ?
			FMath::Min(OwnerComp.KnownInstances[InstanceIdIndex + 1].FirstNodeInstance, MaxAllowedIdx) :
			MaxAllowedIdx;

		for (int32 Idx = Info.FirstNodeInstance; Idx < LastNodeIdx; Idx++)
		{
			OwnerComp.NodeInstances[Idx]->OnInstanceDestroyed(OwnerComp);
		}
	}

	CleanupNodes(OwnerComp, *RootNode, CleanupType);

	// remove memory when instance is destroyed - it will need full initialize anyway
	if (CleanupType == EBTMemoryClear::Destroy)
	{
		Info.InstanceMemory.Empty();
	}
	else
	{
		Info.InstanceMemory = InstanceMemory;
	}
}

void FBehaviorTreeInstance::CleanupNodes(UBehaviorTreeComponent& OwnerComp, UBTCompositeNode& Node, EBTMemoryClear::Type CleanupType)
{
	for (int32 ServiceIndex = 0; ServiceIndex < Node.Services.Num(); ServiceIndex++)
	{
		Node.Services[ServiceIndex]->CleanupInSubtree(OwnerComp, Node.Services[ServiceIndex]->GetNodeMemory<uint8>(*this), CleanupType);
	}

	Node.CleanupInSubtree(OwnerComp, Node.GetNodeMemory<uint8>(*this), CleanupType);

	for (int32 ChildIndex = 0; ChildIndex < Node.Children.Num(); ChildIndex++)
	{
		FBTCompositeChild& ChildInfo = Node.Children[ChildIndex];

		for (int32 DecoratorIndex = 0; DecoratorIndex < ChildInfo.Decorators.Num(); DecoratorIndex++)
		{
			ChildInfo.Decorators[DecoratorIndex]->CleanupInSubtree(OwnerComp, ChildInfo.Decorators[DecoratorIndex]->GetNodeMemory<uint8>(*this), CleanupType);
		}

		if (ChildInfo.ChildComposite)
		{
			CleanupNodes(OwnerComp, *(ChildInfo.ChildComposite), CleanupType);
		}
		else if (ChildInfo.ChildTask)
		{
			for (int32 ServiceIndex = 0; ServiceIndex < ChildInfo.ChildTask->Services.Num(); ServiceIndex++)
			{
				ChildInfo.ChildTask->Services[ServiceIndex]->CleanupInSubtree(OwnerComp, ChildInfo.ChildTask->Services[ServiceIndex]->GetNodeMemory<uint8>(*this), CleanupType);
			}

			ChildInfo.ChildTask->CleanupInSubtree(OwnerComp, ChildInfo.ChildTask->GetNodeMemory<uint8>(*this), CleanupType);
		}
	}
}

bool FBehaviorTreeInstance::HasActiveNode(uint16 TestExecutionIndex) const
{
	if (ActiveNode && ActiveNode->GetExecutionIndex() == TestExecutionIndex)
	{
		return (ActiveNodeType == EBTActiveNode::ActiveTask);
	}

	for (int32 Idx = 0; Idx < ParallelTasks.Num(); Idx++)
	{
		const FBehaviorTreeParallelTask& ParallelTask = ParallelTasks[Idx];
		if (ParallelTask.TaskNode && ParallelTask.TaskNode->GetExecutionIndex() == TestExecutionIndex)
		{
			return (ParallelTask.Status == EBTTaskStatus::Active);
		}
	}

	for (int32 Idx = 0; Idx < ActiveAuxNodes.Num(); Idx++)
	{
		if (ActiveAuxNodes[Idx] && ActiveAuxNodes[Idx]->GetExecutionIndex() == TestExecutionIndex)
		{
			return true;
		}
	}

	return false;
}

void FBehaviorTreeInstance::DeactivateNodes(FBehaviorTreeSearchData& SearchData, uint16 InstanceIndex)
{
	for (int32 Idx = SearchData.PendingUpdates.Num() - 1; Idx >= 0; Idx--)
	{
		FBehaviorTreeSearchUpdate& UpdateInfo = SearchData.PendingUpdates[Idx];
		if (UpdateInfo.InstanceIndex == InstanceIndex && UpdateInfo.Mode == EBTNodeUpdateMode::Add)
		{
			UE_VLOG(SearchData.OwnerComp.GetOwner(), LogBehaviorTree, Verbose, TEXT("Search node update[%s]: %s"),
				*UBehaviorTreeTypes::DescribeNodeUpdateMode(EBTNodeUpdateMode::Remove),
				*UBehaviorTreeTypes::DescribeNodeHelper(UpdateInfo.AuxNode ? (UBTNode*)UpdateInfo.AuxNode : (UBTNode*)UpdateInfo.TaskNode));

			SearchData.PendingUpdates.RemoveAt(Idx, 1, false);
		}
	}

	for (int32 Idx = 0; Idx < ParallelTasks.Num(); Idx++)
	{
		const FBehaviorTreeParallelTask& ParallelTask = ParallelTasks[Idx];
		if (ParallelTask.TaskNode && ParallelTask.Status == EBTTaskStatus::Active)
		{
			SearchData.AddUniqueUpdate(FBehaviorTreeSearchUpdate(ParallelTask.TaskNode, InstanceIndex, EBTNodeUpdateMode::Remove));
		}
	}

	for (int32 Idx = 0; Idx < ActiveAuxNodes.Num(); Idx++)
	{
		if (ActiveAuxNodes[Idx])
		{
			SearchData.AddUniqueUpdate(FBehaviorTreeSearchUpdate(ActiveAuxNodes[Idx], InstanceIndex, EBTNodeUpdateMode::Remove));
		}
	}
}


//----------------------------------------------------------------------//
// FBTNodeIndex
//----------------------------------------------------------------------//

bool FBTNodeIndex::TakesPriorityOver(const FBTNodeIndex& Other) const
{
	// instance closer to root is more important
	if (InstanceIndex != Other.InstanceIndex)
	{
		return InstanceIndex < Other.InstanceIndex;
	}

	// higher priority is more important
	return ExecutionIndex < Other.ExecutionIndex;
}

//----------------------------------------------------------------------//
// FBehaviorTreeSearchData
//----------------------------------------------------------------------//

int32 FBehaviorTreeSearchData::NextSearchId = 1;

void FBehaviorTreeSearchData::AddUniqueUpdate(const FBehaviorTreeSearchUpdate& UpdateInfo)
{
	UE_VLOG(OwnerComp.GetOwner(), LogBehaviorTree, Verbose, TEXT("Search node update[%s]: %s"),
		*UBehaviorTreeTypes::DescribeNodeUpdateMode(UpdateInfo.Mode),
		*UBehaviorTreeTypes::DescribeNodeHelper(UpdateInfo.AuxNode ? (UBTNode*)UpdateInfo.AuxNode : (UBTNode*)UpdateInfo.TaskNode));

	bool bSkipAdding = false;
	for (int32 UpdateIndex = 0; UpdateIndex < PendingUpdates.Num(); UpdateIndex++)
	{
		const FBehaviorTreeSearchUpdate& Info = PendingUpdates[UpdateIndex];
		if (Info.AuxNode == UpdateInfo.AuxNode && Info.TaskNode == UpdateInfo.TaskNode)
		{
			// duplicate, skip
			if (Info.Mode == UpdateInfo.Mode)
			{
				bSkipAdding = true;
				break;
			}

			// don't add pairs add-remove
			bSkipAdding = (Info.Mode == EBTNodeUpdateMode::Remove) || (UpdateInfo.Mode == EBTNodeUpdateMode::Remove);

			PendingUpdates.RemoveAt(UpdateIndex, 1, false);
		}
	}
	
	// don't add Remove updates for inactive aux nodes, as they will block valid Add update coming later from the same search
	// check only aux nodes, it happens due to UBTCompositeNode::NotifyDecoratorsOnActivation
	if (!bSkipAdding && UpdateInfo.Mode == EBTNodeUpdateMode::Remove && UpdateInfo.AuxNode)
	{
		const bool bIsActive = OwnerComp.IsAuxNodeActive(UpdateInfo.AuxNode, UpdateInfo.InstanceIndex);
		bSkipAdding = !bIsActive;
	}

	if (!bSkipAdding)
	{
		const int32 Idx = PendingUpdates.Add(UpdateInfo);
		PendingUpdates[Idx].bPostUpdate = (UpdateInfo.Mode == EBTNodeUpdateMode::Add) && (Cast<UBTService>(UpdateInfo.AuxNode) != NULL);
	}
	else
	{
		UE_VLOG(OwnerComp.GetOwner(), LogBehaviorTree, Verbose, TEXT(">> or not, update skipped"));
	}
}

void FBehaviorTreeSearchData::AssignSearchId()
{
	SearchId = NextSearchId;
	NextSearchId++;
}

void FBehaviorTreeSearchData::Reset()
{
	PendingUpdates.Reset();
	SearchStart = FBTNodeIndex();
	SearchEnd = FBTNodeIndex();
	bSearchInProgress = false;
	bPostponeSearch = false;
}

//----------------------------------------------------------------------//
// FBlackboardKeySelector
//----------------------------------------------------------------------//
void FBlackboardKeySelector::ResolveSelectedKey(const UBlackboardData& BlackboardAsset)
{
	if (SelectedKeyName.IsNone() == false || !bNoneIsAllowedValue)
	{
		if (SelectedKeyName.IsNone() && !bNoneIsAllowedValue)
		{
			InitSelection(BlackboardAsset);
		}

		SelectedKeyID = BlackboardAsset.GetKeyID(SelectedKeyName);
		SelectedKeyType = BlackboardAsset.GetKeyType(SelectedKeyID);
	}
}

void FBlackboardKeySelector::InitSelection(const UBlackboardData& BlackboardAsset)
{
	for (const UBlackboardData* It = &BlackboardAsset; It; It = It->Parent)
	{
		for (int32 KeyIndex = 0; KeyIndex < It->Keys.Num(); KeyIndex++)
		{
			const FBlackboardEntry& EntryInfo = It->Keys[KeyIndex];
			if (EntryInfo.KeyType)
			{
				bool bFilterPassed = true;
				if (AllowedTypes.Num())
				{
					bFilterPassed = false;
					for (int32 FilterIndex = 0; FilterIndex < AllowedTypes.Num(); FilterIndex++)
					{
						if (EntryInfo.KeyType->IsAllowedByFilter(AllowedTypes[FilterIndex]))
						{
							bFilterPassed = true;
							break;
						}
					}
				}

				if (bFilterPassed)
				{
					SelectedKeyName = EntryInfo.EntryName;
					break;
				}
			}
		}
	}
}

void FBlackboardKeySelector::AddObjectFilter(UObject* Owner, FName PropertyName, TSubclassOf<UObject> AllowedClass)
{
	const FName FilterName = MakeUniqueObjectName(Owner, UBlackboardKeyType_Object::StaticClass(), *FString::Printf(TEXT("%s_Object"), *PropertyName.ToString()));
	UBlackboardKeyType_Object* FilterOb = NewObject<UBlackboardKeyType_Object>(Owner, FilterName);
	FilterOb->BaseClass = AllowedClass;
	AllowedTypes.Add(FilterOb);
}

void FBlackboardKeySelector::AddClassFilter(UObject* Owner, FName PropertyName, TSubclassOf<UClass> AllowedClass)
{
	const FName FilterName = MakeUniqueObjectName(Owner, UBlackboardKeyType_Class::StaticClass(), *FString::Printf(TEXT("%s_Class"), *PropertyName.ToString()));
	UBlackboardKeyType_Class* FilterOb = NewObject<UBlackboardKeyType_Class>(Owner, FilterName);
	FilterOb->BaseClass = AllowedClass;
	AllowedTypes.Add(FilterOb);
}

void FBlackboardKeySelector::AddEnumFilter(UObject* Owner, FName PropertyName, UEnum* AllowedEnum)
{
	const FName FilterName = MakeUniqueObjectName(Owner, UBlackboardKeyType_Enum::StaticClass(), *FString::Printf(TEXT("%s_Enum"), *PropertyName.ToString()));
	UBlackboardKeyType_Enum* FilterOb = NewObject<UBlackboardKeyType_Enum>(Owner, FilterName);
	FilterOb->EnumType = AllowedEnum;
	AllowedTypes.Add(FilterOb);
}

void FBlackboardKeySelector::AddNativeEnumFilter(UObject* Owner, FName PropertyName, const FString& AllowedEnumName)
{
	const FName FilterName = MakeUniqueObjectName(Owner, UBlackboardKeyType_NativeEnum::StaticClass(), *FString::Printf(TEXT("%s_NativeEnum"), *PropertyName.ToString()));
	UBlackboardKeyType_NativeEnum* FilterOb = NewObject<UBlackboardKeyType_NativeEnum>(Owner, FilterName);
	FilterOb->EnumName = AllowedEnumName;
	AllowedTypes.Add(FilterOb);
}

void FBlackboardKeySelector::AddIntFilter(UObject* Owner, FName PropertyName)
{
	const FString FilterName = PropertyName.ToString() + TEXT("_Int");
	AllowedTypes.Add(NewObject<UBlackboardKeyType_Int>(Owner, *FilterName));
}

void FBlackboardKeySelector::AddFloatFilter(UObject* Owner, FName PropertyName)
{
	const FString FilterName = PropertyName.ToString() + TEXT("_Float");
	AllowedTypes.Add(NewObject<UBlackboardKeyType_Float>(Owner, *FilterName));
}

void FBlackboardKeySelector::AddBoolFilter(UObject* Owner, FName PropertyName)
{
	const FString FilterName = PropertyName.ToString() + TEXT("_Bool");
	AllowedTypes.Add(NewObject<UBlackboardKeyType_Bool>(Owner, *FilterName));
}

void FBlackboardKeySelector::AddVectorFilter(UObject* Owner, FName PropertyName)
{
	const FString FilterName = PropertyName.ToString() + TEXT("_Vector");
	AllowedTypes.Add(NewObject<UBlackboardKeyType_Vector>(Owner, *FilterName));
}

void FBlackboardKeySelector::AddRotatorFilter(UObject* Owner, FName PropertyName)
{
	const FString FilterName = PropertyName.ToString() + TEXT("_Rotator");
	AllowedTypes.Add(NewObject<UBlackboardKeyType_Rotator>(Owner, *FilterName));
}

void FBlackboardKeySelector::AddStringFilter(UObject* Owner, FName PropertyName)
{
	const FString FilterName = PropertyName.ToString() + TEXT("_String");
	AllowedTypes.Add(NewObject<UBlackboardKeyType_String>(Owner, *FilterName));
}

void FBlackboardKeySelector::AddNameFilter(UObject* Owner, FName PropertyName)
{
	const FString FilterName = PropertyName.ToString() + TEXT("_Name");
	AllowedTypes.Add(NewObject<UBlackboardKeyType_Name>(Owner, *FilterName));
}

//----------------------------------------------------------------------//
// UBehaviorTreeTypes
//----------------------------------------------------------------------//
UBehaviorTreeTypes::UBehaviorTreeTypes(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

FString UBehaviorTreeTypes::DescribeNodeResult(EBTNodeResult::Type NodeResult)
{
	static FString ResultDesc[] = { TEXT("Succeeded"), TEXT("Failed"), TEXT("Aborted"), TEXT("InProgress") };
	return (NodeResult < ARRAY_COUNT(ResultDesc)) ? ResultDesc[NodeResult] : FString();
}

FString UBehaviorTreeTypes::DescribeFlowAbortMode(EBTFlowAbortMode::Type AbortMode)
{
	static FString AbortModeDesc[] = { TEXT("None"), TEXT("Lower Priority"), TEXT("Self"), TEXT("Both") };
	return (AbortMode < ARRAY_COUNT(AbortModeDesc)) ? AbortModeDesc[AbortMode] : FString();
}

FString UBehaviorTreeTypes::DescribeActiveNode(EBTActiveNode::Type ActiveNodeType)
{
	static FString ActiveDesc[] = { TEXT("Composite"), TEXT("ActiveTask"), TEXT("AbortingTask"), TEXT("InactiveTask") };
	return (ActiveNodeType < ARRAY_COUNT(ActiveDesc)) ? ActiveDesc[ActiveNodeType] : FString();
}

FString UBehaviorTreeTypes::DescribeTaskStatus(EBTTaskStatus::Type TaskStatus)
{
	static FString TaskStatusDesc[] = { TEXT("Active"), TEXT("Aborting"), TEXT("Inactive") };
	return (TaskStatus < ARRAY_COUNT(TaskStatusDesc)) ? TaskStatusDesc[TaskStatus] : FString();
}

FString UBehaviorTreeTypes::DescribeNodeUpdateMode(EBTNodeUpdateMode::Type UpdateMode)
{
	static FString UpdateModeDesc[] = { TEXT("Unknown"), TEXT("Add"), TEXT("Remove") };
	return (UpdateMode < ARRAY_COUNT(UpdateModeDesc)) ? UpdateModeDesc[UpdateMode] : FString();
}

FString UBehaviorTreeTypes::DescribeNodeHelper(const UBTNode* Node)
{
	return Node ? FString::Printf(TEXT("%s[%d]"), *Node->GetNodeName(), Node->GetExecutionIndex()) : FString();
}

FString UBehaviorTreeTypes::GetShortTypeName(const UObject* Ob)
{
	if (Ob->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		return Ob->GetClass()->GetName().LeftChop(2);
	}

	FString TypeDesc = Ob->GetClass()->GetName();
	const int32 ShortNameIdx = TypeDesc.Find(TEXT("_"));
	if (ShortNameIdx != INDEX_NONE)
	{
		TypeDesc = TypeDesc.Mid(ShortNameIdx + 1);
	}

	return TypeDesc;
}

//----------------------------------------------------------------------//
// DEPRECATED
//----------------------------------------------------------------------//

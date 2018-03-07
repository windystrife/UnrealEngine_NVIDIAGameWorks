// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "InputCoreTypes.h"
#include "Templates/SubclassOf.h"
#include "BehaviorTreeTypes.generated.h"

class FBlackboardDecoratorDetails;
class UBehaviorTree;
class UBehaviorTreeComponent;
class UBlackboardComponent;
class UBlackboardData;
class UBlackboardKeyType;
class UBTAuxiliaryNode;
class UBTCompositeNode;
class UBTNode;
class UBTTaskNode;
struct FBehaviorTreeSearchData;

// Visual logging helper
#define BT_VLOG(Context, Verbosity, Format, ...) UE_VLOG(Context->OwnerComp.IsValid() ? Context->OwnerComp->GetOwner() : NULL, LogBehaviorTree, Verbosity, Format, ##__VA_ARGS__)
#define BT_SEARCHLOG(SearchData, Verbosity, Format, ...) UE_VLOG(SearchData.OwnerComp.GetOwner(), LogBehaviorTree, Verbosity, Format, ##__VA_ARGS__)

// Behavior tree debugger in editor
#define USE_BEHAVIORTREE_DEBUGGER	(1 && WITH_EDITORONLY_DATA)

DECLARE_STATS_GROUP(TEXT("Behavior Tree"), STATGROUP_AIBehaviorTree, STATCAT_Advanced);

DECLARE_CYCLE_STAT_EXTERN(TEXT("Tick"),STAT_AI_BehaviorTree_Tick,STATGROUP_AIBehaviorTree, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Load Time"),STAT_AI_BehaviorTree_LoadTime,STATGROUP_AIBehaviorTree, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Search Time"),STAT_AI_BehaviorTree_SearchTime,STATGROUP_AIBehaviorTree, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Execution Time"),STAT_AI_BehaviorTree_ExecutionTime,STATGROUP_AIBehaviorTree, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Auxiliary Update Time"),STAT_AI_BehaviorTree_AuxUpdateTime,STATGROUP_AIBehaviorTree, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num Templates"),STAT_AI_BehaviorTree_NumTemplates,STATGROUP_AIBehaviorTree, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num Instances"),STAT_AI_BehaviorTree_NumInstances,STATGROUP_AIBehaviorTree, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Instance memory"),STAT_AI_BehaviorTree_InstanceMemory,STATGROUP_AIBehaviorTree, AIMODULE_API);

namespace FBlackboard
{
	const FName KeySelf = TEXT("SelfActor");

	typedef uint8 FKey;

	const FKey InvalidKey = FKey(-1);
}

enum class EBlackboardNotificationResult : uint8
{
	RemoveObserver,
	ContinueObserving
};

// delegate defines
DECLARE_DELEGATE_TwoParams(FOnBlackboardChange, const UBlackboardComponent&, FBlackboard::FKey /*key ID*/);
DECLARE_DELEGATE_RetVal_TwoParams(EBlackboardNotificationResult, FOnBlackboardChangeNotification, const UBlackboardComponent&, FBlackboard::FKey /*key ID*/);

namespace BTSpecialChild
{
	const int32 NotInitialized = -1;	// special value for child indices: needs to be initialized
	const int32 ReturnToParent = -2;	// special value for child indices: return to parent node
	
	const uint8 OwnedByComposite = MAX_uint8;	// special value for aux node's child index: owned by composite node instead of a task
}

UENUM(BlueprintType)
namespace EBTNodeResult
{
	// keep in sync with DescribeNodeResult()
	enum Type
	{
		Succeeded,		// finished as success
		Failed,			// finished as failure
		Aborted,		// finished aborting = failure
		InProgress,		// not finished yet
	};
}

namespace EBTExecutionMode
{
	enum Type
	{
		SingleRun,
		Looped,
	};
}

namespace EBTStopMode
{
	enum Type
	{
		Safe,
		Forced,
	};
}

namespace EBTMemoryInit
{
	enum Type
	{
		Initialize,		// first time initialization
		RestoreSubtree,	// loading saved data on reentering subtree
	};
}

namespace EBTMemoryClear
{
	enum Type
	{
		Destroy,		// final clear
		StoreSubtree,	// saving data on leaving subtree
	};
}

UENUM()
namespace EBTFlowAbortMode
{
	// keep in sync with DescribeFlowAbortMode()

	enum Type
	{
		None				UMETA(DisplayName="Nothing"),
		LowerPriority		UMETA(DisplayName="Lower Priority"),
		Self				UMETA(DisplayName="Self"),
		Both				UMETA(DisplayName="Both"),
	};
}

namespace EBTActiveNode
{
	// keep in sync with DescribeActiveNode()
	enum Type
	{
		Composite,
		ActiveTask,
		AbortingTask,
		InactiveTask,
	};
}

namespace EBTTaskStatus
{
	// keep in sync with DescribeTaskStatus()
	enum Type
	{
		Active,
		Aborting,
		Inactive,
	};
}

namespace EBTNodeUpdateMode
{
	// keep in sync with DescribeNodeUpdateMode()
	enum Type
	{
		Unknown,
		Add,				// add node
		Remove,				// remove node
	};
}

/** wrapper struct for holding a parallel task node and its status */
struct FBehaviorTreeParallelTask
{
	/** worker object */
	const UBTTaskNode* TaskNode;

	/** additional mode data used for context switching */
	EBTTaskStatus::Type Status;

	FBehaviorTreeParallelTask() : TaskNode(NULL) {}
	FBehaviorTreeParallelTask(const UBTTaskNode* InTaskNode, EBTTaskStatus::Type InStatus) : TaskNode(InTaskNode), Status(InStatus) {}

	bool operator==(const FBehaviorTreeParallelTask& Other) const { return TaskNode == Other.TaskNode; }
	bool operator==(const UBTTaskNode* OtherTask) const { return TaskNode == OtherTask; }
};

namespace EBTExecutionSnap
{
	enum Type
	{
		Regular,
		OutOfNodes,
	};
}

namespace EBTDescriptionVerbosity
{
	enum Type
	{
		Basic,
		Detailed,
	};
}

enum class EBTNodeRelativePriority : uint8
{
	Lower,
	Same,
	Higher
};

/** debugger data about subtree instance */
struct FBehaviorTreeDebuggerInstance
{
	struct FNodeFlowData
	{
		uint16 ExecutionIndex;
		uint16 bPassed : 1;
		uint16 bTrigger : 1;
		uint16 bDiscardedTrigger : 1;

		FNodeFlowData() : ExecutionIndex(INDEX_NONE), bPassed(0), bTrigger(0), bDiscardedTrigger(0) {}
	};

	FBehaviorTreeDebuggerInstance() : TreeAsset(NULL), RootNode(NULL) {}

	/** behavior tree asset */
	UBehaviorTree* TreeAsset;

	/** root node in template */
	UBTCompositeNode* RootNode;

	/** execution indices of active nodes */
	TArray<uint16> ActivePath;

	/** execution indices of active nodes */
	TArray<uint16> AdditionalActiveNodes;

	/** search flow from previous state */
	TArray<FNodeFlowData> PathFromPrevious;

	/** runtime descriptions for each execution index */
	TArray<FString> RuntimeDesc;

	FORCEINLINE bool IsValid() const { return ActivePath.Num() != 0; }
};

/** debugger data about current execution step */
struct FBehaviorTreeExecutionStep
{
	FBehaviorTreeExecutionStep() : TimeStamp(0.f), StepIndex(INDEX_NONE) {}

	/** subtree instance stack */
	TArray<FBehaviorTreeDebuggerInstance> InstanceStack;

	/** blackboard snapshot: value descriptions */
	TMap<FName, FString> BlackboardValues;

	/** Game world's time stamp of this step */
	float TimeStamp;

	/** index of execution step */
	int32 StepIndex;
};

/** identifier of subtree instance */
struct FBehaviorTreeInstanceId
{
	/** behavior tree asset */
	UBehaviorTree* TreeAsset;

	/** root node in template for cleanup purposes */
	UBTCompositeNode* RootNode;

	/** execution index path from root */
	TArray<uint16> Path;

	/** persistent instance memory */
	TArray<uint8> InstanceMemory;

	/** index of first node instance (BehaviorTreeComponent.NodeInstances) */
	int32 FirstNodeInstance;

	FBehaviorTreeInstanceId() : TreeAsset(0), RootNode(0), FirstNodeInstance(-1) {}

	bool operator==(const FBehaviorTreeInstanceId& Other) const
	{
		return (TreeAsset == Other.TreeAsset) && (Path == Other.Path);
	}
};

struct FBehaviorTreeSearchData;

/** data required for instance of single subtree */
struct FBehaviorTreeInstance
{
	/** root node in template */
	UBTCompositeNode* RootNode;

	/** active node in template */
	UBTNode* ActiveNode;

	/** active auxiliary nodes */
	TArray<UBTAuxiliaryNode*> ActiveAuxNodes;

	/** active parallel tasks */
	TArray<FBehaviorTreeParallelTask> ParallelTasks;

	/** memory: instance */
	TArray<uint8> InstanceMemory;

	/** index of identifier (BehaviorTreeComponent.KnownInstances) */
	uint8 InstanceIdIndex;

	/** active node type */
	TEnumAsByte<EBTActiveNode::Type> ActiveNodeType;

	FBehaviorTreeInstance() { IncMemoryStats(); }
	FBehaviorTreeInstance(const FBehaviorTreeInstance& Other) { *this = Other; IncMemoryStats(); }
	FBehaviorTreeInstance(int32 MemorySize) { InstanceMemory.AddZeroed(MemorySize); IncMemoryStats(); }
	~FBehaviorTreeInstance() { DecMemoryStats(); }

#if STATS
	FORCEINLINE void IncMemoryStats() { INC_MEMORY_STAT_BY(STAT_AI_BehaviorTree_InstanceMemory, GetAllocatedSize()); }
	FORCEINLINE void DecMemoryStats() { DEC_MEMORY_STAT_BY(STAT_AI_BehaviorTree_InstanceMemory, GetAllocatedSize()); }
	FORCEINLINE uint32 GetAllocatedSize() const 
	{
		return sizeof(*this) + ActiveAuxNodes.GetAllocatedSize() + ParallelTasks.GetAllocatedSize() + InstanceMemory.GetAllocatedSize(); 
	}
#else
	FORCEINLINE uint32 GetAllocatedSize() const { return 0; }
	FORCEINLINE void IncMemoryStats() {}
	FORCEINLINE void DecMemoryStats() {}
#endif // STATS

	/** initialize memory and create node instances */
	void Initialize(UBehaviorTreeComponent& OwnerComp, UBTCompositeNode& Node, int32& InstancedIndex, EBTMemoryInit::Type InitType);

	/** cleanup node instances */
	void Cleanup(UBehaviorTreeComponent& OwnerComp, EBTMemoryClear::Type CleanupType);

	/** check if instance has active node with given execution index */
	bool HasActiveNode(uint16 TestExecutionIndex) const;

	/** deactivate all active aux nodes and remove their requests from SearchData */
	void DeactivateNodes(FBehaviorTreeSearchData& SearchData, uint16 InstanceIndex);

protected:

	/** worker for updating all nodes */
	void CleanupNodes(UBehaviorTreeComponent& OwnerComp, UBTCompositeNode& Node, EBTMemoryClear::Type CleanupType);
};

struct FBTNodeIndex
{
	/** index of instance of stack */
	uint16 InstanceIndex;

	/** execution index within instance */
	uint16 ExecutionIndex;

	FBTNodeIndex() : InstanceIndex(MAX_uint16), ExecutionIndex(MAX_uint16) {}
	FBTNodeIndex(uint16 InInstanceIndex, uint16 InExecutionIndex) : InstanceIndex(InInstanceIndex), ExecutionIndex(InExecutionIndex) {}

	bool TakesPriorityOver(const FBTNodeIndex& Other) const;
	bool IsSet() const { return InstanceIndex < MAX_uint16; }

	FORCEINLINE bool operator==(const FBTNodeIndex& Other) const { return Other.ExecutionIndex == ExecutionIndex && Other.InstanceIndex == InstanceIndex; }
	FORCEINLINE friend uint32 GetTypeHash(const FBTNodeIndex& Other) { return Other.ExecutionIndex ^ Other.InstanceIndex; }

	FORCEINLINE FString Describe() const { return FString::Printf(TEXT("[%d:%d]"), InstanceIndex, ExecutionIndex); }
};

/** node update data */
struct FBehaviorTreeSearchUpdate
{
	UBTAuxiliaryNode* AuxNode;
	UBTTaskNode* TaskNode;

	uint16 InstanceIndex;

	TEnumAsByte<EBTNodeUpdateMode::Type> Mode;

	/** if set, this entry will be applied AFTER other are processed */
	uint8 bPostUpdate : 1;

	FBehaviorTreeSearchUpdate() : AuxNode(0), TaskNode(0), InstanceIndex(0), Mode(EBTNodeUpdateMode::Unknown), bPostUpdate(false) {}
	FBehaviorTreeSearchUpdate(const UBTAuxiliaryNode* InAuxNode, uint16 InInstanceIndex, EBTNodeUpdateMode::Type InMode) :
		AuxNode((UBTAuxiliaryNode*)InAuxNode), TaskNode(0), InstanceIndex(InInstanceIndex), Mode(InMode), bPostUpdate(false)
	{}
	FBehaviorTreeSearchUpdate(const UBTTaskNode* InTaskNode, uint16 InInstanceIndex, EBTNodeUpdateMode::Type InMode) :
		AuxNode(0), TaskNode((UBTTaskNode*)InTaskNode), InstanceIndex(InInstanceIndex), Mode(InMode), bPostUpdate(false)
	{}
};

/** node search data */
struct FBehaviorTreeSearchData
{
	/** BT component */
	UBehaviorTreeComponent& OwnerComp;

	/** requested updates of additional nodes (preconditions, services, parallels)
	 *  buffered during search to prevent instant add & remove pairs */
	TArray<FBehaviorTreeSearchUpdate> PendingUpdates;

	/** first node allowed in search */
	FBTNodeIndex SearchStart;

	/** last node allowed in search */
	FBTNodeIndex SearchEnd;

	/** search unique number */
	int32 SearchId;

	/** if set, current search will be restarted in next tick */
	uint32 bPostponeSearch : 1;

	/** set when task search is in progress */
	uint32 bSearchInProgress : 1;

	/** adds update info to PendingUpdates array, removing all previous updates for this node */
	void AddUniqueUpdate(const FBehaviorTreeSearchUpdate& UpdateInfo);

	/** assign unique Id number */
	void AssignSearchId();

	/** clear state of search */
	void Reset();

	FBehaviorTreeSearchData(UBehaviorTreeComponent& InOwnerComp) 
		: OwnerComp(InOwnerComp), bPostponeSearch(false), bSearchInProgress(false)
	{}

private:

	static int32 NextSearchId;
};

/** property block in blueprint defined nodes */
struct FBehaviorTreePropertyMemory
{
	uint16 Offset;
	uint16 BlockSize;
	
	FBehaviorTreePropertyMemory() {}
	FBehaviorTreePropertyMemory(int32 Value) : Offset((uint32)Value >> 16), BlockSize((uint32)Value & 0xFFFF) {}

	int32 Pack() const { return (int32)(((uint32)Offset << 16) | BlockSize); }
};

/** helper struct for defining types of allowed blackboard entries
 *  (e.g. only entries holding points and objects derived form actor class) */
USTRUCT(BlueprintType)
struct AIMODULE_API FBlackboardKeySelector
{
	GENERATED_USTRUCT_BODY()

	FBlackboardKeySelector() : SelectedKeyID(FBlackboard::InvalidKey)
	{}

	/** array of allowed types with additional properties (e.g. uobject's base class) 
	  * EditAnywhere is required for FBlackboardSelectorDetails::CacheBlackboardData() */
	UPROPERTY(transient, EditAnywhere, BlueprintReadWrite, Category = Blackboard)
	TArray<UBlackboardKeyType*> AllowedTypes;

	/** name of selected key */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = Blackboard)
	FName SelectedKeyName;

	/** class of selected key  */
	UPROPERTY(transient, EditInstanceOnly, BlueprintReadWrite, Category = Blackboard)
	TSubclassOf<UBlackboardKeyType> SelectedKeyType;

protected:
	/** ID of selected key */
	UPROPERTY(transient, EditInstanceOnly, BlueprintReadWrite, Category = Blackboard)
	uint8 SelectedKeyID;
	// SelectedKeyId type should be FBlackboard::FKey, but typedefs are not supported by UHT
	static_assert(sizeof(uint8) == sizeof(FBlackboard::FKey), "FBlackboardKeySelector::SelectedKeyId should be of FBlackboard::FKey-compatible type.");

	// Requires BlueprintReadWrite so that blueprint creators (using MakeBlackboardKeySelector) can specify whether or not None is Allowed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Blackboard, Meta = (Tooltip = ""))
	uint32 bNoneIsAllowedValue:1;

	/** find initial selection. Called when None is not a valid option for this key selector */
	void InitSelection(const UBlackboardData& BlackboardAsset);

public:
	/** find ID and class of selected key */
	void ResolveSelectedKey(const UBlackboardData& BlackboardAsset);
		
	void AllowNoneAsValue(bool bAllow) { bNoneIsAllowedValue = bAllow; }

	FORCEINLINE FBlackboard::FKey GetSelectedKeyID() const { return SelectedKeyID; }

	/** helper functions for setting basic filters */
	void AddObjectFilter(UObject* Owner, FName PropertyName, TSubclassOf<UObject> AllowedClass);
	void AddClassFilter(UObject* Owner, FName PropertyName, TSubclassOf<UClass> AllowedClass);
	void AddEnumFilter(UObject* Owner, FName PropertyName, UEnum* AllowedEnum);
	void AddNativeEnumFilter(UObject* Owner, FName PropertyName, const FString& AllowedEnumName);
	void AddIntFilter(UObject* Owner, FName PropertyName);
	void AddFloatFilter(UObject* Owner, FName PropertyName);
	void AddBoolFilter(UObject* Owner, FName PropertyName);
	void AddVectorFilter(UObject* Owner, FName PropertyName);
	void AddRotatorFilter(UObject* Owner, FName PropertyName);
	void AddStringFilter(UObject* Owner, FName PropertyName);
	void AddNameFilter(UObject* Owner, FName PropertyName);

	FORCEINLINE bool IsNone() const { return bNoneIsAllowedValue && SelectedKeyID == FBlackboard::InvalidKey; }
	FORCEINLINE bool IsSet() const { return SelectedKeyID != FBlackboard::InvalidKey; }
	FORCEINLINE bool NeedsResolving() const { return SelectedKeyID == FBlackboard::InvalidKey && SelectedKeyName.IsNone() == false; }
	FORCEINLINE void InvalidateResolvedKey() { SelectedKeyID = FBlackboard::InvalidKey; }

	friend FBlackboardDecoratorDetails;
};

UCLASS(Abstract)
class AIMODULE_API UBehaviorTreeTypes : public UObject
{
	GENERATED_UCLASS_BODY()

	static FString DescribeNodeHelper(const UBTNode* Node);

	static FString DescribeNodeResult(EBTNodeResult::Type NodeResult);
	static FString DescribeFlowAbortMode(EBTFlowAbortMode::Type FlowAbortMode);
	static FString DescribeActiveNode(EBTActiveNode::Type ActiveNodeType);
	static FString DescribeTaskStatus(EBTTaskStatus::Type TaskStatus);
	static FString DescribeNodeUpdateMode(EBTNodeUpdateMode::Type UpdateMode);

	/** returns short name of object's class (BTTaskNode_Wait -> Wait) */
	static FString GetShortTypeName(const UObject* Ob);
};

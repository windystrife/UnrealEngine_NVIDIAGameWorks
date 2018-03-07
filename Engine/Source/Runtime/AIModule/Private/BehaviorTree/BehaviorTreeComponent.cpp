// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "VisualLogger/VisualLoggerTypes.h"
#include "VisualLogger/VisualLogger.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTreeDelegates.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeManager.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Tasks/BTTask_RunBehaviorDynamic.h"
#include "Misc/ConfigCacheIni.h"

#if USE_BEHAVIORTREE_DEBUGGER
int32 UBehaviorTreeComponent::ActiveDebuggerCounter = 0;
#endif

//----------------------------------------------------------------------//
// UBehaviorTreeComponent
//----------------------------------------------------------------------//

UBehaviorTreeComponent::UBehaviorTreeComponent(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
	, SearchData(*this)
{
	ActiveInstanceIdx = 0;
	bLoopExecution = false;
	bWaitingForAbortingTasks = false;
	bRequestedFlowUpdate = false;
	bAutoActivate = true;
	bWantsInitializeComponent = true; 
	bIsRunning = false;
	bIsPaused = false;
}

UBehaviorTreeComponent::UBehaviorTreeComponent(FVTableHelper& Helper)
	: Super(Helper)
	, SearchData(*this)
{

}

void UBehaviorTreeComponent::UninitializeComponent()
{
	UBehaviorTreeManager* BTManager = UBehaviorTreeManager::GetCurrent(GetWorld());
	if (BTManager)
	{
		BTManager->RemoveActiveComponent(*this);
	}

	RemoveAllInstances();
	Super::UninitializeComponent();
}

void UBehaviorTreeComponent::RestartLogic()
{
	UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("UBehaviorTreeComponent::RestartLogic"));
	RestartTree();
}

void UBehaviorTreeComponent::StopLogic(const FString& Reason) 
{
	UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("Stopping BT, reason: \'%s\'"), *Reason);
	StopTree(EBTStopMode::Safe);
}

void UBehaviorTreeComponent::PauseLogic(const FString& Reason)
{
	UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("Execution updates: PAUSED (%s)"), *Reason);
	bIsPaused = true;

	if (BlackboardComp)
	{
		BlackboardComp->PauseObserverNotifications();
	}
}

EAILogicResuming::Type UBehaviorTreeComponent::ResumeLogic(const FString& Reason)
{
	const EAILogicResuming::Type SuperResumeResult = Super::ResumeLogic(Reason);
	if (!!bIsPaused)
	{
		bIsPaused = false;

		if (SuperResumeResult == EAILogicResuming::Continue)
		{
			if (BlackboardComp)
			{
				// Resume the blackboard's observer notifications and send any queued notifications
				BlackboardComp->ResumeObserverNotifications(true);
			}

			const bool bOutOfNodesPending = PendingExecution.IsSet() && PendingExecution.bOutOfNodes;
			if (ExecutionRequest.ExecuteNode || bOutOfNodesPending)
			{
				ScheduleExecutionUpdate();
			}

			return EAILogicResuming::Continue;
		}
		else if (SuperResumeResult == EAILogicResuming::RestartedInstead)
		{
			if (BlackboardComp)
			{
				// Resume the blackboard's observer notifications but do not send any queued notifications
				BlackboardComp->ResumeObserverNotifications(false);
			}
		}
	}

	return SuperResumeResult;
}

bool UBehaviorTreeComponent::TreeHasBeenStarted() const
{
	return bIsRunning && InstanceStack.Num();
}

bool UBehaviorTreeComponent::IsRunning() const
{ 
	return bIsPaused == false && TreeHasBeenStarted() == true;
}

bool UBehaviorTreeComponent::IsPaused() const
{
	return bIsPaused;
}

void UBehaviorTreeComponent::StartTree(UBehaviorTree& Asset, EBTExecutionMode::Type ExecuteMode /*= EBTExecutionMode::Looped*/)
{
	// clear instance stack, start should always run new tree from root
	UBehaviorTree* CurrentRoot = GetRootTree();
	
	if (CurrentRoot == &Asset && TreeHasBeenStarted())
	{
		UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("Skipping behavior start request - it's already running"));
		return;
	}
	else if (CurrentRoot)
	{
		UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("Abandoning behavior %s to start new one (%s)"),
			*GetNameSafe(CurrentRoot), *Asset.GetName());
	}

	StopTree(EBTStopMode::Safe);

	TreeStartInfo.Asset = &Asset;
	TreeStartInfo.ExecuteMode = ExecuteMode;
	TreeStartInfo.bPendingInitialize = true;

	ProcessPendingInitialize();
}

void UBehaviorTreeComponent::ProcessPendingInitialize()
{
	StopTree(EBTStopMode::Safe);
	if (bWaitingForAbortingTasks)
	{
		return;
	}

	// finish cleanup
	RemoveAllInstances();

	bLoopExecution = (TreeStartInfo.ExecuteMode == EBTExecutionMode::Looped);
	bIsRunning = true;

#if USE_BEHAVIORTREE_DEBUGGER
	DebuggerSteps.Reset();
#endif

	UBehaviorTreeManager* BTManager = UBehaviorTreeManager::GetCurrent(GetWorld());
	if (BTManager)
	{
		BTManager->AddActiveComponent(*this);
	}

	// push new instance
	const bool bPushed = PushInstance(*TreeStartInfo.Asset);
	TreeStartInfo.bPendingInitialize = false;
}

void UBehaviorTreeComponent::StopTree(EBTStopMode::Type StopMode)
{
	if (!bRequestedStop)
	{
		bRequestedStop = true;

		for (int32 InstanceIndex = InstanceStack.Num() - 1; InstanceIndex >= 0; InstanceIndex--)
		{
			FBehaviorTreeInstance& InstanceInfo = InstanceStack[InstanceIndex];

			// notify active aux nodes
			for (int32 AuxIndex = 0; AuxIndex < InstanceInfo.ActiveAuxNodes.Num(); AuxIndex++)
			{
				const UBTAuxiliaryNode* AuxNode = InstanceInfo.ActiveAuxNodes[AuxIndex];
				check(AuxNode != NULL);
				uint8* NodeMemory = AuxNode->GetNodeMemory<uint8>(InstanceInfo);
				AuxNode->WrappedOnCeaseRelevant(*this, NodeMemory);
			}
			InstanceInfo.ActiveAuxNodes.Reset();

			// notify active parallel tasks
			//
			// calling OnTaskFinished with result other than InProgress will unregister parallel task,
			// modifying array we're iterating on - iterator needs to be moved one step back in that case
			//
			for (int32 ParallelIndex = 0; ParallelIndex < InstanceInfo.ParallelTasks.Num(); ParallelIndex++)
			{
				FBehaviorTreeParallelTask& ParallelTaskInfo = InstanceInfo.ParallelTasks[ParallelIndex];
				const UBTTaskNode* CachedTaskNode = ParallelTaskInfo.TaskNode;

				if (CachedTaskNode && (ParallelTaskInfo.Status == EBTTaskStatus::Active) && !CachedTaskNode->IsPendingKill())
				{
					// remove all message observers added by task execution, so they won't interfere with Abort call
					UnregisterMessageObserversFrom(CachedTaskNode);

					uint8* NodeMemory = CachedTaskNode->GetNodeMemory<uint8>(InstanceInfo);
					EBTNodeResult::Type NodeResult = CachedTaskNode->WrappedAbortTask(*this, NodeMemory);

					UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("Parallel task aborted: %s (%s)"),
						*UBehaviorTreeTypes::DescribeNodeHelper(CachedTaskNode),
						(NodeResult == EBTNodeResult::InProgress) ? TEXT("in progress") : TEXT("instant"));

					// mark as pending abort
					if (NodeResult == EBTNodeResult::InProgress)
					{
						const bool bIsValidForStatus = InstanceInfo.ParallelTasks.IsValidIndex(ParallelIndex) && (ParallelTaskInfo.TaskNode == CachedTaskNode);
						if (bIsValidForStatus)
						{
							ParallelTaskInfo.Status = EBTTaskStatus::Aborting;
							bWaitingForAbortingTasks = true;
						}
						else
						{
							UE_VLOG(GetOwner(), LogBehaviorTree, Warning, TEXT("Parallel task %s was unregistered before completing Abort state!"),
								*UBehaviorTreeTypes::DescribeNodeHelper(CachedTaskNode));
						}
					}

					OnTaskFinished(CachedTaskNode, NodeResult);

					const bool bIsValidAfterFinishing = InstanceInfo.ParallelTasks.IsValidIndex(ParallelIndex) && (ParallelTaskInfo.TaskNode == CachedTaskNode);
					if (!bIsValidAfterFinishing)
					{
						// move iterator back if current task was unregistered
						ParallelIndex--;
					}
				}
			}

			// notify active task
			if (InstanceInfo.ActiveNodeType == EBTActiveNode::ActiveTask)
			{
				const UBTTaskNode* TaskNode = Cast<const UBTTaskNode>(InstanceInfo.ActiveNode);
				check(TaskNode != NULL);

				// remove all observers before requesting abort
				UnregisterMessageObserversFrom(TaskNode);
				InstanceInfo.ActiveNodeType = EBTActiveNode::AbortingTask;

				UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("Abort task: %s"), *UBehaviorTreeTypes::DescribeNodeHelper(TaskNode));

				// abort task using current state of tree
				uint8* NodeMemory = TaskNode->GetNodeMemory<uint8>(InstanceInfo);
				EBTNodeResult::Type TaskResult = TaskNode->WrappedAbortTask(*this, NodeMemory);

				// pass task finished if wasn't already notified (FinishLatentAbort)
				if (InstanceInfo.ActiveNodeType == EBTActiveNode::AbortingTask)
				{
					OnTaskFinished(TaskNode, TaskResult);
				}
			}
		}
	}

	if (bWaitingForAbortingTasks)
	{
		if (StopMode == EBTStopMode::Safe)
		{
			UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("StopTree is waiting for aborting tasks to finish..."));
			return;
		}

		UE_VLOG(GetOwner(), LogBehaviorTree, Warning, TEXT("StopTree was forced while waiting for tasks to finish aborting!"));
	}

	// make sure that all nodes are getting deactivation notifies
	if (InstanceStack.Num())
	{
		EBTNodeResult::Type AbortedResult = EBTNodeResult::Aborted;
		DeactivateUpTo(InstanceStack[0].RootNode, 0, AbortedResult);
	}

	// clear current state, don't touch debugger data
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceStack.Num(); InstanceIndex++)
	{
		FBehaviorTreeInstance& InstanceInfo = InstanceStack[InstanceIndex];
		InstanceStack[InstanceIndex].Cleanup(*this, EBTMemoryClear::Destroy);
	}

	InstanceStack.Reset();
	TaskMessageObservers.Reset();
	SearchData.Reset();
	ExecutionRequest = FBTNodeExecutionInfo();
	PendingExecution = FBTPendingExecutionInfo();
	ActiveInstanceIdx = 0;

	// make sure to allow new execution requests
	bRequestedFlowUpdate = false;
	bRequestedStop = false;
	bIsRunning = false;
	bWaitingForAbortingTasks = false;
}

void UBehaviorTreeComponent::RestartTree()
{
	UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("UBehaviorTreeComponent::RestartTree"));
	
	if (!bIsRunning)
	{
		if (TreeStartInfo.IsSet())
		{
			TreeStartInfo.bPendingInitialize = true;
			ProcessPendingInitialize();
		}
	}
	else if (bRequestedStop)
	{
		TreeStartInfo.bPendingInitialize = true;
	}
	else if (InstanceStack.Num())
	{
		FBehaviorTreeInstance& TopInstance = InstanceStack[0];
		RequestExecution(TopInstance.RootNode, 0, TopInstance.RootNode, -1, EBTNodeResult::Aborted);
	}
}

void UBehaviorTreeComponent::Cleanup()
{
	StopTree(EBTStopMode::Forced);
	RemoveAllInstances();

	KnownInstances.Reset();
	InstanceStack.Reset();
	NodeInstances.Reset();
}

void UBehaviorTreeComponent::OnTaskFinished(const UBTTaskNode* TaskNode, EBTNodeResult::Type TaskResult)
{
	if (TaskNode == NULL || InstanceStack.Num() == 0 || IsPendingKill())
	{
		return;
	}

	UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("Task %s finished: %s"), 
		*UBehaviorTreeTypes::DescribeNodeHelper(TaskNode), *UBehaviorTreeTypes::DescribeNodeResult(TaskResult));

	// notify parent node
	UBTCompositeNode* ParentNode = TaskNode->GetParentNode();
	const int32 TaskInstanceIdx = FindInstanceContainingNode(TaskNode);
	if (!InstanceStack.IsValidIndex(TaskInstanceIdx))
	{
		return;
	}

	uint8* ParentMemory = ParentNode->GetNodeMemory<uint8>(InstanceStack[TaskInstanceIdx]);

	const bool bWasWaitingForAbort = bWaitingForAbortingTasks;
	ParentNode->ConditionalNotifyChildExecution(*this, ParentMemory, *TaskNode, TaskResult);
	
	if (TaskResult != EBTNodeResult::InProgress)
	{
		StoreDebuggerSearchStep(TaskNode, TaskInstanceIdx, TaskResult);

		// cleanup task observers
		UnregisterMessageObserversFrom(TaskNode);

		// notify task about it
		uint8* TaskMemory = TaskNode->GetNodeMemory<uint8>(InstanceStack[TaskInstanceIdx]);
		TaskNode->WrappedOnTaskFinished(*this, TaskMemory, TaskResult);

		// update execution when active task is finished
		if (InstanceStack.IsValidIndex(ActiveInstanceIdx) && InstanceStack[ActiveInstanceIdx].ActiveNode == TaskNode)
		{
			FBehaviorTreeInstance& ActiveInstance = InstanceStack[ActiveInstanceIdx];
			const bool bWasAborting = (ActiveInstance.ActiveNodeType == EBTActiveNode::AbortingTask);
			ActiveInstance.ActiveNodeType = EBTActiveNode::InactiveTask;

			// request execution from parent
			if (!bWasAborting)
			{
				RequestExecution(TaskResult);
			}
		}
		else if (TaskResult == EBTNodeResult::Aborted && InstanceStack.IsValidIndex(TaskInstanceIdx) && InstanceStack[TaskInstanceIdx].ActiveNode == TaskNode)
		{
			// active instance may be already changed when getting back from AbortCurrentTask 
			// (e.g. new task is higher on stack)

			InstanceStack[TaskInstanceIdx].ActiveNodeType = EBTActiveNode::InactiveTask;
		}

		// update state of aborting tasks after currently finished one was set to Inactive
		UpdateAbortingTasks();

		// make sure that we continue execution after all pending latent aborts finished
		if (!bWaitingForAbortingTasks && bWasWaitingForAbort)
		{
			if (bRequestedStop)
			{
				StopTree(EBTStopMode::Safe);
			}
			else
			{
				ScheduleExecutionUpdate();
			}
		}
	}
	else
	{
		// always update state of aborting tasks
		UpdateAbortingTasks();
	}

	if (TreeStartInfo.HasPendingInitialize())
	{
		ProcessPendingInitialize();
	}
}

void UBehaviorTreeComponent::OnTreeFinished()
{
	UE_VLOG(GetOwner(), LogBehaviorTree, Verbose, TEXT("Ran out of nodes to check, %s tree."),
		bLoopExecution ? TEXT("looping") : TEXT("stopping"));

	ActiveInstanceIdx = 0;
	StoreDebuggerExecutionStep(EBTExecutionSnap::OutOfNodes);

	if (bLoopExecution && InstanceStack.Num())
	{
		// it should be already deactivated (including root)
		// set active node to initial state: root activation
		FBehaviorTreeInstance& TopInstance = InstanceStack[0];
		TopInstance.ActiveNode = NULL;
		TopInstance.ActiveNodeType = EBTActiveNode::Composite;

		// make sure that all active aux nodes will be removed
		// root level services are being handled on applying search data
		UnregisterAuxNodesUpTo(FBTNodeIndex(0, 0));

		// result doesn't really matter, root node will be reset and start iterating child nodes from scratch
		// although it shouldn't be set to Aborted, as it has special meaning in RequestExecution (switch to higher priority)
		RequestExecution(TopInstance.RootNode, 0, TopInstance.RootNode, 0, EBTNodeResult::InProgress);
	}
	else
	{
		StopTree(EBTStopMode::Safe);
	}
}

bool UBehaviorTreeComponent::IsExecutingBranch(const UBTNode* Node, int32 ChildIndex) const
{
	const int32 TestInstanceIdx = FindInstanceContainingNode(Node);
	if (!InstanceStack.IsValidIndex(TestInstanceIdx) || InstanceStack[TestInstanceIdx].ActiveNode == NULL)
	{
		return false;
	}

	// is it active node or root of tree?
	const FBehaviorTreeInstance& TestInstance = InstanceStack[TestInstanceIdx];
	if (Node == TestInstance.RootNode || Node == TestInstance.ActiveNode)
	{
		return true;
	}

	// compare with index of next child
	const uint16 ActiveExecutionIndex = TestInstance.ActiveNode->GetExecutionIndex();
	const uint16 NextChildExecutionIndex = Node->GetParentNode()->GetChildExecutionIndex(ChildIndex + 1);
	return (ActiveExecutionIndex >= Node->GetExecutionIndex()) && (ActiveExecutionIndex < NextChildExecutionIndex);
}

bool UBehaviorTreeComponent::IsAuxNodeActive(const UBTAuxiliaryNode* AuxNode) const
{
	if (AuxNode == NULL)
	{
		return false;
	}

	const uint16 AuxExecutionIndex = AuxNode->GetExecutionIndex();
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceStack.Num(); InstanceIndex++)
	{
		const FBehaviorTreeInstance& InstanceInfo = InstanceStack[InstanceIndex];
		for (int32 AuxIndex = 0; AuxIndex < InstanceInfo.ActiveAuxNodes.Num(); AuxIndex++)
		{
			const UBTAuxiliaryNode* TestAuxNode = InstanceInfo.ActiveAuxNodes[AuxIndex];

			// check template version
			if (TestAuxNode == AuxNode)
			{
				return true;
			}

			// check instanced version
			CA_SUPPRESS(6011);
			if (AuxNode->IsInstanced() && TestAuxNode && TestAuxNode->GetExecutionIndex() == AuxExecutionIndex)
			{
				const uint8* NodeMemory = TestAuxNode->GetNodeMemory<uint8>(InstanceInfo);
				UBTNode* NodeInstance = TestAuxNode->GetNodeInstance(*this, (uint8*)NodeMemory);

				if (NodeInstance == AuxNode)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool UBehaviorTreeComponent::IsAuxNodeActive(const UBTAuxiliaryNode* AuxNodeTemplate, int32 InstanceIdx) const
{
	return InstanceStack.IsValidIndex(InstanceIdx) && InstanceStack[InstanceIdx].ActiveAuxNodes.Contains(AuxNodeTemplate);
}

EBTTaskStatus::Type UBehaviorTreeComponent::GetTaskStatus(const UBTTaskNode* TaskNode) const
{
	EBTTaskStatus::Type Status = EBTTaskStatus::Inactive;
	const int32 InstanceIdx = FindInstanceContainingNode(TaskNode);

	if (InstanceStack.IsValidIndex(InstanceIdx))
	{
		const uint16 ExecutionIndex = TaskNode->GetExecutionIndex();
		const FBehaviorTreeInstance& InstanceInfo = InstanceStack[InstanceIdx];

		// always check parallel execution first, it takes priority over ActiveNodeType
		for (int32 TaskIndex = 0; TaskIndex < InstanceInfo.ParallelTasks.Num(); TaskIndex++)
		{
			const FBehaviorTreeParallelTask& ParallelInfo = InstanceInfo.ParallelTasks[TaskIndex];
			if (ParallelInfo.TaskNode == TaskNode ||
				(TaskNode->IsInstanced() && ParallelInfo.TaskNode && ParallelInfo.TaskNode->GetExecutionIndex() == ExecutionIndex))
			{
				Status = ParallelInfo.Status;
				break;
			}
		}

		if (Status == EBTTaskStatus::Inactive)
		{
			if (InstanceInfo.ActiveNode == TaskNode ||
				(TaskNode->IsInstanced() && InstanceInfo.ActiveNode && InstanceInfo.ActiveNode->GetExecutionIndex() == ExecutionIndex))
			{
				Status =
					(InstanceInfo.ActiveNodeType == EBTActiveNode::ActiveTask) ? EBTTaskStatus::Active :
					(InstanceInfo.ActiveNodeType == EBTActiveNode::AbortingTask) ? EBTTaskStatus::Aborting :
					EBTTaskStatus::Inactive;
			}
		}
	}

	return Status;
}

void UBehaviorTreeComponent::RequestExecution(const UBTDecorator* RequestedBy)
{
	check(RequestedBy);
	// search range depends on decorator's FlowAbortMode:
	//
	// - LowerPri: try entering branch = search only nodes under decorator
	//
	// - Self: leave execution = from node under decorator to end of tree
	//
	// - Both: check if active node is within inner child nodes and choose Self or LowerPri
	//

	EBTFlowAbortMode::Type AbortMode = RequestedBy->GetFlowAbortMode();
	if (AbortMode == EBTFlowAbortMode::None)
	{
		return;
	}

	const int32 InstanceIdx = FindInstanceContainingNode(RequestedBy->GetParentNode());
	if (InstanceIdx == INDEX_NONE)
	{
		return;
	}

	const FBehaviorTreeInstance& ActiveInstance = InstanceStack.Last();
	if (ActiveInstance.ActiveNodeType == EBTActiveNode::ActiveTask)
	{
		EBTNodeRelativePriority RelativePriority = CalculateRelativePriority(RequestedBy, ActiveInstance.ActiveNode);
		UE_CVLOG(RelativePriority < EBTNodeRelativePriority::Same, GetOwner(), LogBehaviorTree, Error, TEXT("UBehaviorTreeComponent::RequestExecution decorator requesting restart has lower priority than Current Task"));
		ensure(RelativePriority >= EBTNodeRelativePriority::Same);
	}

	if (AbortMode == EBTFlowAbortMode::Both)
	{
		const bool bIsExecutingChildNodes = IsExecutingBranch(RequestedBy, RequestedBy->GetChildIndex());
		AbortMode = bIsExecutingChildNodes ? EBTFlowAbortMode::Self : EBTFlowAbortMode::LowerPriority;
	}

	EBTNodeResult::Type ContinueResult = (AbortMode == EBTFlowAbortMode::Self) ? EBTNodeResult::Failed : EBTNodeResult::Aborted;
	RequestExecution(RequestedBy->GetParentNode(), InstanceIdx, RequestedBy, RequestedBy->GetChildIndex(), ContinueResult);
}

EBTNodeRelativePriority UBehaviorTreeComponent::CalculateRelativePriority(const UBTNode* NodeA, const UBTNode* NodeB) const
{
	EBTNodeRelativePriority RelativePriority = EBTNodeRelativePriority::Same;

	if (NodeA != NodeB)
	{
		const int32 InstanceIndexA = FindInstanceContainingNode(NodeA);
		const int32 InstanceIndexB = FindInstanceContainingNode(NodeB);
		if (InstanceIndexA == InstanceIndexB)
		{
			RelativePriority = NodeA->GetExecutionIndex() < NodeB->GetExecutionIndex() ? EBTNodeRelativePriority::Higher : EBTNodeRelativePriority::Lower;
		}
		else
		{
			RelativePriority = (InstanceIndexA != INDEX_NONE && InstanceIndexB != INDEX_NONE) ? (InstanceIndexA < InstanceIndexB ? EBTNodeRelativePriority::Higher : EBTNodeRelativePriority::Lower)
				: (InstanceIndexA != INDEX_NONE ? EBTNodeRelativePriority::Higher : EBTNodeRelativePriority::Lower);
		}
	}

	return RelativePriority;
}

void UBehaviorTreeComponent::RequestExecution(EBTNodeResult::Type LastResult)
{
	// task helpers can't continue with InProgress or Aborted result, it should be handled 
	// either by decorator helper or regular RequestExecution() (6 param version)

	if (LastResult != EBTNodeResult::Aborted && LastResult != EBTNodeResult::InProgress && InstanceStack.IsValidIndex(ActiveInstanceIdx))
	{
		const FBehaviorTreeInstance& ActiveInstance = InstanceStack[ActiveInstanceIdx];
		UBTCompositeNode* ExecuteParent = (ActiveInstance.ActiveNode == NULL) ? ActiveInstance.RootNode :
			(ActiveInstance.ActiveNodeType == EBTActiveNode::Composite) ? (UBTCompositeNode*)ActiveInstance.ActiveNode :
			ActiveInstance.ActiveNode->GetParentNode();

		RequestExecution(ExecuteParent, InstanceStack.Num() - 1,
			ActiveInstance.ActiveNode ? ActiveInstance.ActiveNode : ActiveInstance.RootNode, -1,
			LastResult, false);
	}
}

static void FindCommonParent(const TArray<FBehaviorTreeInstance>& Instances, const TArray<FBehaviorTreeInstanceId>& KnownInstances,
							 UBTCompositeNode* InNodeA, uint16 InstanceIdxA,
							 UBTCompositeNode* InNodeB, uint16 InstanceIdxB,
							 UBTCompositeNode*& CommonParentNode, uint16& CommonInstanceIdx)
{
	// find two nodes in the same instance (choose lower index = closer to root)
	CommonInstanceIdx = (InstanceIdxA <= InstanceIdxB) ? InstanceIdxA : InstanceIdxB;

	UBTCompositeNode* NodeA = (CommonInstanceIdx == InstanceIdxA) ? InNodeA : Instances[CommonInstanceIdx].ActiveNode->GetParentNode();
	UBTCompositeNode* NodeB = (CommonInstanceIdx == InstanceIdxB) ? InNodeB : Instances[CommonInstanceIdx].ActiveNode->GetParentNode();

	// special case: node was taken from CommonInstanceIdx, but it had ActiveNode set to root (no parent)
	if (!NodeA && CommonInstanceIdx != InstanceIdxA)
	{
		NodeA = Instances[CommonInstanceIdx].RootNode;
	}
	if (!NodeB && CommonInstanceIdx != InstanceIdxB)
	{
		NodeB = Instances[CommonInstanceIdx].RootNode;
	}

	// if one of nodes is still empty, we have serious problem with execution flow - crash and log details
	if (!NodeA || !NodeB)
	{
		FString AssetAName = Instances.IsValidIndex(InstanceIdxA) && KnownInstances.IsValidIndex(Instances[InstanceIdxA].InstanceIdIndex) ? GetNameSafe(KnownInstances[Instances[InstanceIdxA].InstanceIdIndex].TreeAsset) : TEXT("unknown");
		FString AssetBName = Instances.IsValidIndex(InstanceIdxB) && KnownInstances.IsValidIndex(Instances[InstanceIdxB].InstanceIdIndex) ? GetNameSafe(KnownInstances[Instances[InstanceIdxB].InstanceIdIndex].TreeAsset) : TEXT("unknown");
		FString AssetCName = Instances.IsValidIndex(CommonInstanceIdx) && KnownInstances.IsValidIndex(Instances[CommonInstanceIdx].InstanceIdIndex) ? GetNameSafe(KnownInstances[Instances[CommonInstanceIdx].InstanceIdIndex].TreeAsset) : TEXT("unknown");

		UE_LOG(LogBehaviorTree, Fatal, TEXT("Fatal error in FindCommonParent() call.\nInNodeA: %s, InstanceIdxA: %d (%s), NodeA: %s\nInNodeB: %s, InstanceIdxB: %d (%s), NodeB: %s\nCommonInstanceIdx: %d (%s), ActiveNode: %s%s"),
			*UBehaviorTreeTypes::DescribeNodeHelper(InNodeA), InstanceIdxA, *AssetAName, *UBehaviorTreeTypes::DescribeNodeHelper(NodeA),
			*UBehaviorTreeTypes::DescribeNodeHelper(InNodeB), InstanceIdxB, *AssetBName, *UBehaviorTreeTypes::DescribeNodeHelper(NodeB),
			CommonInstanceIdx, *AssetCName, *UBehaviorTreeTypes::DescribeNodeHelper(Instances[CommonInstanceIdx].ActiveNode),
			(Instances[CommonInstanceIdx].ActiveNode == Instances[CommonInstanceIdx].RootNode) ? TEXT(" (root)") : TEXT(""));
	}

	// find common parent of two nodes
	int32 NodeADepth = NodeA->GetTreeDepth();
	int32 NodeBDepth = NodeB->GetTreeDepth();

	while (NodeADepth > NodeBDepth)
	{
		NodeA = NodeA->GetParentNode();
		NodeADepth = NodeA->GetTreeDepth();
	}

	while (NodeBDepth > NodeADepth)
	{
		NodeB = NodeB->GetParentNode();
		NodeBDepth = NodeB->GetTreeDepth();
	}

	while (NodeA != NodeB)
	{
		NodeA = NodeA->GetParentNode();
		NodeB = NodeB->GetParentNode();
	}

	CommonParentNode = NodeA;
}

void UBehaviorTreeComponent::ScheduleExecutionUpdate()
{
	bRequestedFlowUpdate = true;
}

void UBehaviorTreeComponent::RequestExecution(UBTCompositeNode* RequestedOn, int32 InstanceIdx, const UBTNode* RequestedBy,
											  int32 RequestedByChildIndex, EBTNodeResult::Type ContinueWithResult, bool bStoreForDebugger)
{
	SCOPE_CYCLE_COUNTER(STAT_AI_BehaviorTree_SearchTime);

	UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("Execution request by %s (result: %s)"),
		*UBehaviorTreeTypes::DescribeNodeHelper(RequestedBy),
		*UBehaviorTreeTypes::DescribeNodeResult(ContinueWithResult));

	if (!bIsRunning || !InstanceStack.IsValidIndex(ActiveInstanceIdx) || (GetOwner() && GetOwner()->IsPendingKillPending()))
	{
		UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("> skip: tree is not running"));
		return;
	}

	const bool bOutOfNodesPending = PendingExecution.IsSet() && PendingExecution.bOutOfNodes;
	if (bOutOfNodesPending)
	{
		UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("> skip: tree ran out of nodes on previous restart and needs to process it first"));
		return;
	}

	const bool bSwitchToHigherPriority = (ContinueWithResult == EBTNodeResult::Aborted);
	const bool bAlreadyHasRequest = (ExecutionRequest.ExecuteNode != NULL);
	const UBTNode* DebuggerNode = bStoreForDebugger ? RequestedBy : NULL;

	FBTNodeIndex ExecutionIdx;
	ExecutionIdx.InstanceIndex = InstanceIdx;
	ExecutionIdx.ExecutionIndex = RequestedBy->GetExecutionIndex();
	uint16 LastExecutionIndex = MAX_uint16;

	if (bSwitchToHigherPriority && RequestedByChildIndex >= 0)
	{
		ExecutionIdx.ExecutionIndex = RequestedOn->GetChildExecutionIndex(RequestedByChildIndex, EBTChildIndex::FirstNode);
		
		// first index outside allowed range		
		LastExecutionIndex = RequestedOn->GetChildExecutionIndex(RequestedByChildIndex + 1, EBTChildIndex::FirstNode);
	}

	const FBTNodeIndex SearchEnd(InstanceIdx, LastExecutionIndex);

	// check if it's more important than currently requested
	if (bAlreadyHasRequest && ExecutionRequest.SearchStart.TakesPriorityOver(ExecutionIdx))
	{
		UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("> skip: already has request with higher priority"));
		StoreDebuggerRestart(DebuggerNode, InstanceIdx, true);

		// make sure to update end of search range
		if (bSwitchToHigherPriority)
		{
			if (ExecutionRequest.SearchEnd.IsSet() && ExecutionRequest.SearchEnd.TakesPriorityOver(SearchEnd))
			{
				UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("> expanding end of search range!"));
				ExecutionRequest.SearchEnd = SearchEnd;
			}
		}
		else
		{
			if (ExecutionRequest.SearchEnd.IsSet())
			{
				UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("> removing limit from end of search range!"));
				ExecutionRequest.SearchEnd = FBTNodeIndex();
			}
		}

		return;
	}

	// when it's aborting and moving to higher priority node:
	if (bSwitchToHigherPriority)
	{
		// check if decorators allow execution on requesting link
		// unless it's branch restart (abort result within current branch), when it can't be skipped because branch can be no longer valid
		const bool bShouldCheckDecorators = (RequestedByChildIndex >= 0) && !IsExecutingBranch(RequestedBy, RequestedByChildIndex);
		const bool bCanExecute = !bShouldCheckDecorators || RequestedOn->DoDecoratorsAllowExecution(*this, InstanceIdx, RequestedByChildIndex);
		if (!bCanExecute)
		{
			UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("> skip: decorators are not allowing execution"));
			StoreDebuggerRestart(DebuggerNode, InstanceIdx, false);
			return;
		}

		// update common parent: requesting node with prev common/active node
		UBTCompositeNode* CurrentNode = ExecutionRequest.ExecuteNode;
		uint16 CurrentInstanceIdx = ExecutionRequest.ExecuteInstanceIdx;
		if (ExecutionRequest.ExecuteNode == NULL)
		{
			FBehaviorTreeInstance& ActiveInstance = InstanceStack[ActiveInstanceIdx];
			CurrentNode = (ActiveInstance.ActiveNode == NULL) ? ActiveInstance.RootNode :
				(ActiveInstance.ActiveNodeType == EBTActiveNode::Composite) ? (UBTCompositeNode*)ActiveInstance.ActiveNode :
				ActiveInstance.ActiveNode->GetParentNode();

			CurrentInstanceIdx = ActiveInstanceIdx;
		}

		if (ExecutionRequest.ExecuteNode != RequestedOn)
		{
			UBTCompositeNode* CommonParent = NULL;
			uint16 CommonInstanceIdx = MAX_uint16;

			FindCommonParent(InstanceStack, KnownInstances, RequestedOn, InstanceIdx, CurrentNode, CurrentInstanceIdx, CommonParent, CommonInstanceIdx);

			// check decorators between common parent and restart parent
			int32 ItInstanceIdx = InstanceIdx;
			for (UBTCompositeNode* It = RequestedOn; It && It != CommonParent;)
			{
				UBTCompositeNode* ParentNode = It->GetParentNode();
				int32 ChildIdx = INDEX_NONE;

				if (ParentNode == nullptr)
				{
					// move up the tree stack
					if (ItInstanceIdx > 0)
					{
						ItInstanceIdx--;
						UBTNode* SubtreeTaskNode = InstanceStack[ItInstanceIdx].ActiveNode;
						ParentNode = SubtreeTaskNode->GetParentNode();
						ChildIdx = ParentNode->GetChildIndex(*SubtreeTaskNode);
					}
					else
					{
						// something went wrong...
						break;
					}
				}
				else
				{
					ChildIdx = ParentNode->GetChildIndex(*It);
				}

				const bool bCanExecuteTest = ParentNode->DoDecoratorsAllowExecution(*this, ItInstanceIdx, ChildIdx);
				if (!bCanExecuteTest)
				{
					UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("> skip: decorators are not allowing execution"));
					StoreDebuggerRestart(DebuggerNode, InstanceIdx, false);
					return;
				}

				It = ParentNode;
			}

			ExecutionRequest.ExecuteNode = CommonParent;
			ExecutionRequest.ExecuteInstanceIdx = CommonInstanceIdx;
		}
	}
	else
	{
		// check if decorators allow execution on requesting link (only when restart comes from composite decorator)
		const bool bShouldCheckDecorators = RequestedOn->Children.IsValidIndex(RequestedByChildIndex) &&
			(RequestedOn->Children[RequestedByChildIndex].DecoratorOps.Num() > 0) &&
			RequestedBy->IsA(UBTDecorator::StaticClass());

		const bool bCanExecute = bShouldCheckDecorators && RequestedOn->DoDecoratorsAllowExecution(*this, InstanceIdx, RequestedByChildIndex);
		if (bCanExecute)
		{
			UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("> skip: decorators are still allowing execution"));
			StoreDebuggerRestart(DebuggerNode, InstanceIdx, false);
			return;
		}

		ExecutionRequest.ExecuteNode = RequestedOn;
		ExecutionRequest.ExecuteInstanceIdx = InstanceIdx;
	}

	// store it
	StoreDebuggerRestart(DebuggerNode, InstanceIdx, true);

	// search end can be set only when switching to high priority
	// or previous request was limited and current limit is wider
	if ((!bAlreadyHasRequest && bSwitchToHigherPriority) ||
		(ExecutionRequest.SearchEnd.IsSet() && ExecutionRequest.SearchEnd.TakesPriorityOver(SearchEnd)))
	{
		UE_CVLOG(bAlreadyHasRequest, GetOwner(), LogBehaviorTree, Log, TEXT("%s"), (SearchEnd.ExecutionIndex < MAX_uint16) ? TEXT("> expanding end of search range!") : TEXT("> removing limit from end of search range!"));
		ExecutionRequest.SearchEnd = SearchEnd;
	}

	ExecutionRequest.SearchStart = ExecutionIdx;
	ExecutionRequest.ContinueWithResult = ContinueWithResult;
	ExecutionRequest.bTryNextChild = !bSwitchToHigherPriority && !PendingExecution.bAborting;
	ExecutionRequest.bIsRestart = (RequestedBy != GetActiveNode());
	PendingExecution.Lock();
	
	// break out of current search if new request is more important than currently processed one
	// no point in starting new task just to abandon it in next tick
	if (SearchData.bSearchInProgress)
	{
		UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("> aborting current task search!"));
		SearchData.bPostponeSearch = true;
	}

	ScheduleExecutionUpdate();
}

void UBehaviorTreeComponent::ApplySearchUpdates(const TArray<FBehaviorTreeSearchUpdate>& UpdateList, int32 NewNodeExecutionIndex, bool bPostUpdate)
{
	for (int32 Index = 0; Index < UpdateList.Num(); Index++)
	{
		const FBehaviorTreeSearchUpdate& UpdateInfo = UpdateList[Index];
		if (!InstanceStack.IsValidIndex(UpdateInfo.InstanceIndex))
		{
			continue;
		}

		FBehaviorTreeInstance& UpdateInstance = InstanceStack[UpdateInfo.InstanceIndex];
		int32 ParallelTaskIdx = INDEX_NONE;
		bool bIsComponentActive = false;

		if (UpdateInfo.AuxNode)
		{
			bIsComponentActive = UpdateInstance.ActiveAuxNodes.Contains(UpdateInfo.AuxNode);
		}
		else if (UpdateInfo.TaskNode)
		{
			ParallelTaskIdx = UpdateInstance.ParallelTasks.IndexOfByKey(UpdateInfo.TaskNode);
			bIsComponentActive = (ParallelTaskIdx != INDEX_NONE && UpdateInstance.ParallelTasks[ParallelTaskIdx].Status == EBTTaskStatus::Active);
		}

		const UBTNode* UpdateNode = UpdateInfo.AuxNode ? (const UBTNode*)UpdateInfo.AuxNode : (const UBTNode*)UpdateInfo.TaskNode;
		checkSlow(UpdateNode);

		if ((UpdateInfo.Mode == EBTNodeUpdateMode::Remove && !bIsComponentActive) ||
			(UpdateInfo.Mode == EBTNodeUpdateMode::Add && (bIsComponentActive || UpdateNode->GetExecutionIndex() > NewNodeExecutionIndex)) ||
			(UpdateInfo.bPostUpdate != bPostUpdate))
		{
			continue;
		}

		UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("Update: %s for %s: %s"),
			*UBehaviorTreeTypes::DescribeNodeUpdateMode(UpdateInfo.Mode),
			UpdateInfo.AuxNode ? TEXT("auxiliary node") : TEXT("parallel's main task"),
			*UBehaviorTreeTypes::DescribeNodeHelper(UpdateNode));

		if (UpdateInfo.AuxNode)
		{
			// special case: service node at root of top most subtree - don't remove/re-add them when tree is in looping mode
			// don't bother with decorators parent == root means that they are on child branches
			if (bLoopExecution && UpdateInfo.AuxNode->GetParentNode() == InstanceStack[0].RootNode &&
				UpdateInfo.AuxNode->IsA(UBTService::StaticClass()))
			{
				if (UpdateInfo.Mode == EBTNodeUpdateMode::Remove ||
					InstanceStack[0].ActiveAuxNodes.Contains(UpdateInfo.AuxNode))
				{
					UE_VLOG(GetOwner(), LogBehaviorTree, Verbose, TEXT("> skip [looped execution]"));
					continue;
				}
			}

			uint8* NodeMemory = (uint8*)UpdateNode->GetNodeMemory<uint8>(UpdateInstance);
			if (UpdateInfo.Mode == EBTNodeUpdateMode::Remove)
			{
				UpdateInstance.ActiveAuxNodes.RemoveSingleSwap(UpdateInfo.AuxNode);
				UpdateInfo.AuxNode->WrappedOnCeaseRelevant(*this, NodeMemory);
			}
			else
			{
				UpdateInstance.ActiveAuxNodes.Add(UpdateInfo.AuxNode);
				UpdateInfo.AuxNode->WrappedOnBecomeRelevant(*this, NodeMemory);
			}
		}
		else if (UpdateInfo.TaskNode)
		{
			if (UpdateInfo.Mode == EBTNodeUpdateMode::Remove)
			{
				// remove all message observers from node to abort to avoid calling OnTaskFinished from AbortTask
				UnregisterMessageObserversFrom(UpdateInfo.TaskNode);

				uint8* NodeMemory = (uint8*)UpdateNode->GetNodeMemory<uint8>(UpdateInstance);
				EBTNodeResult::Type NodeResult = UpdateInfo.TaskNode->WrappedAbortTask(*this, NodeMemory);

				UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("Parallel task aborted: %s (%s)"),
					*UBehaviorTreeTypes::DescribeNodeHelper(UpdateInfo.TaskNode),
					(NodeResult == EBTNodeResult::InProgress) ? TEXT("in progress") : TEXT("instant"));

				// check if task node is still valid, could've received LatentAbortFinished during AbortTask call
				const bool bStillValid = InstanceStack.IsValidIndex(UpdateInfo.InstanceIndex) &&
					InstanceStack[UpdateInfo.InstanceIndex].ParallelTasks.IsValidIndex(ParallelTaskIdx) &&
					InstanceStack[UpdateInfo.InstanceIndex].ParallelTasks[ParallelTaskIdx] == UpdateInfo.TaskNode;
				
				if (bStillValid)
				{
					// mark as pending abort
					if (NodeResult == EBTNodeResult::InProgress)
					{
						UpdateInstance.ParallelTasks[ParallelTaskIdx].Status = EBTTaskStatus::Aborting;
						bWaitingForAbortingTasks = true;
					}

					OnTaskFinished(UpdateInfo.TaskNode, NodeResult);
				}
			}
			else
			{
				UE_VLOG(GetOwner(), LogBehaviorTree, Verbose, TEXT("Parallel task: %s added to active list"),
					*UBehaviorTreeTypes::DescribeNodeHelper(UpdateInfo.TaskNode));

				UpdateInstance.ParallelTasks.Add(FBehaviorTreeParallelTask(UpdateInfo.TaskNode, EBTTaskStatus::Active));
			}
		}
	}
}

void UBehaviorTreeComponent::ApplySearchData(UBTNode* NewActiveNode)
{
	const int32 NewNodeExecutionIndex = NewActiveNode ? NewActiveNode->GetExecutionIndex() : 0;

	ApplySearchUpdates(SearchData.PendingUpdates, NewNodeExecutionIndex);
	ApplySearchUpdates(SearchData.PendingUpdates, NewNodeExecutionIndex, true);

	SearchData.PendingUpdates.Reset();
}

void UBehaviorTreeComponent::ApplyDiscardedSearch()
{
	TArray<FBehaviorTreeSearchUpdate> UpdateList;
	for (int32 Idx = 0; Idx < SearchData.PendingUpdates.Num(); Idx++)
	{
		const FBehaviorTreeSearchUpdate& UpdateInfo = SearchData.PendingUpdates[Idx];
		if (UpdateInfo.Mode != EBTNodeUpdateMode::Remove && 
			UpdateInfo.AuxNode && UpdateInfo.AuxNode->IsA(UBTDecorator::StaticClass()))
		{
			const FBTNodeIndex UpdateIdx(UpdateInfo.InstanceIndex, UpdateInfo.AuxNode->GetExecutionIndex());
			if (UpdateIdx.TakesPriorityOver(SearchData.SearchEnd))
			{
				UpdateList.Add(UpdateInfo);
			}
		}
	}

	// apply new observing decorators
	// use MAX_uint16 to ignore execution index checks, building UpdateList checks if observer should be relevant
	ApplySearchUpdates(UpdateList, MAX_uint16);

	// remove everything else
	SearchData.PendingUpdates.Reset();
}

void UBehaviorTreeComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	SCOPE_CYCLE_COUNTER(STAT_AI_BehaviorTree_Tick);
	SCOPE_CYCLE_COUNTER(STAT_AI_Overall);

	check(this != nullptr && this->IsPendingKill() == false);

	if (bRequestedFlowUpdate)
	{
		ProcessExecutionRequest();
	}

	if (InstanceStack.Num() == 0 || !bIsRunning || bIsPaused)
	{
		return;
	}

	// tick active auxiliary nodes and parallel tasks (in execution order, before task)
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceStack.Num(); InstanceIndex++)
	{
		FBehaviorTreeInstance& InstanceInfo = InstanceStack[InstanceIndex];
		for (int32 AuxIndex = 0; AuxIndex < InstanceInfo.ActiveAuxNodes.Num(); AuxIndex++)
		{
			const UBTAuxiliaryNode* AuxNode = InstanceInfo.ActiveAuxNodes[AuxIndex];
			uint8* NodeMemory = AuxNode->GetNodeMemory<uint8>(InstanceInfo);
			AuxNode->WrappedTickNode(*this, NodeMemory, DeltaTime);
		}

		for (int32 TaskIndex = 0; TaskIndex < InstanceInfo.ParallelTasks.Num(); TaskIndex++)
		{
			const UBTTaskNode* ParallelTask = InstanceInfo.ParallelTasks[TaskIndex].TaskNode;
			uint8* NodeMemory = ParallelTask->GetNodeMemory<uint8>(InstanceInfo);
			ParallelTask->WrappedTickTask(*this, NodeMemory, DeltaTime);
		}
	}

	// tick active task
	if (InstanceStack.IsValidIndex(ActiveInstanceIdx))
	{
		FBehaviorTreeInstance& ActiveInstance = InstanceStack[ActiveInstanceIdx];
		if (ActiveInstance.ActiveNodeType == EBTActiveNode::ActiveTask ||
			ActiveInstance.ActiveNodeType == EBTActiveNode::AbortingTask)
		{
			UBTTaskNode* ActiveTask = (UBTTaskNode*)ActiveInstance.ActiveNode;
			uint8* NodeMemory = ActiveTask->GetNodeMemory<uint8>(ActiveInstance);
			ActiveTask->WrappedTickTask(*this, NodeMemory, DeltaTime);
		}
	}

	// tick aborting task from abandoned subtree
	if (InstanceStack.IsValidIndex(ActiveInstanceIdx + 1))
	{
		FBehaviorTreeInstance& LastInstance = InstanceStack.Last();
		if (LastInstance.ActiveNodeType == EBTActiveNode::AbortingTask)
		{
			UBTTaskNode* ActiveTask = (UBTTaskNode*)LastInstance.ActiveNode;
			uint8* NodeMemory = ActiveTask->GetNodeMemory<uint8>(LastInstance);
			ActiveTask->WrappedTickTask(*this, NodeMemory, DeltaTime);
		}
	}
}

void UBehaviorTreeComponent::ProcessExecutionRequest()
{
	bRequestedFlowUpdate = false;
	if (!IsRegistered() || !InstanceStack.IsValidIndex(ActiveInstanceIdx))
	{
		// it shouldn't be called, component is no longer valid
		return;
	}

	if (bIsPaused)
	{
		UE_VLOG(GetOwner(), LogBehaviorTree, Verbose, TEXT("Ignoring ProcessExecutionRequest call due to BTComponent still being paused"));
		return;
	}

	if (PendingExecution.IsSet())
	{
		ProcessPendingExecution();
		return;
	}

	const uint16 PrevActiveInstanceIdx = ActiveInstanceIdx;
	bool bIsSearchValid = true;

	EBTNodeResult::Type NodeResult = ExecutionRequest.ContinueWithResult;
	UBTTaskNode* NextTask = NULL;

	{
		SCOPE_CYCLE_COUNTER(STAT_AI_BehaviorTree_SearchTime);

		// copy current memory in case we need to rollback search
		CopyInstanceMemoryToPersistent();

		// deactivate up to ExecuteNode
		if (InstanceStack[ActiveInstanceIdx].ActiveNode != ExecutionRequest.ExecuteNode)
		{
			const bool bDeactivated = DeactivateUpTo(ExecutionRequest.ExecuteNode, ExecutionRequest.ExecuteInstanceIdx, NodeResult);
			if (!bDeactivated)
			{
				return;
			}
		}

		FBehaviorTreeInstance& ActiveInstance = InstanceStack[ActiveInstanceIdx];
		UBTCompositeNode* TestNode = ExecutionRequest.ExecuteNode;
		SearchData.AssignSearchId();
		SearchData.bPostponeSearch = false;
		SearchData.bSearchInProgress = true;

		// activate root node if needed (can't be handled by parent composite...)
		if (ActiveInstance.ActiveNode == NULL)
		{
			ActiveInstance.ActiveNode = InstanceStack[ActiveInstanceIdx].RootNode;
			ActiveInstance.RootNode->OnNodeActivation(SearchData);
			BT_SEARCHLOG(SearchData, Verbose, TEXT("Activated root node: %s"), *UBehaviorTreeTypes::DescribeNodeHelper(ActiveInstance.RootNode));
		}

		// additional operations for restarting:
		if (!ExecutionRequest.bTryNextChild)
		{
			// mark all decorators less important than current search start node for removal
			const FBTNodeIndex DeactivateIdx(ExecutionRequest.SearchStart.InstanceIndex, ExecutionRequest.SearchStart.ExecutionIndex - 1);
			UnregisterAuxNodesUpTo(ExecutionRequest.SearchStart.ExecutionIndex ? DeactivateIdx : ExecutionRequest.SearchStart);

			// reactivate top search node, so it could use search range correctly
			BT_SEARCHLOG(SearchData, Verbose, TEXT("Reactivate node: %s [restart]"), *UBehaviorTreeTypes::DescribeNodeHelper(TestNode));
			ExecutionRequest.ExecuteNode->OnNodeRestart(SearchData);

			SearchData.SearchStart = ExecutionRequest.SearchStart;
			SearchData.SearchEnd = ExecutionRequest.SearchEnd;

			BT_SEARCHLOG(SearchData, Verbose, TEXT("Clamping search range: %s .. %s"),
				*SearchData.SearchStart.Describe(), *SearchData.SearchEnd.Describe());
		}
		else
		{
			// make sure it's reset before starting new search
			SearchData.SearchStart = FBTNodeIndex();
			SearchData.SearchEnd = FBTNodeIndex();
		}

		// store blackboard values from search start (can be changed by aux node removal/adding)
#if USE_BEHAVIORTREE_DEBUGGER
		StoreDebuggerBlackboard(SearchStartBlackboard);
#endif

		// start looking for next task
		while (TestNode && NextTask == NULL)
		{
			BT_SEARCHLOG(SearchData, Verbose, TEXT("Testing node: %s"), *UBehaviorTreeTypes::DescribeNodeHelper(TestNode));
			const int32 ChildBranchIdx = TestNode->FindChildToExecute(SearchData, NodeResult);
			UBTNode* StoreNode = TestNode;

			if (SearchData.bPostponeSearch)
			{
				// break out of current search loop
				TestNode = NULL;
				bIsSearchValid = false;
			}
			else if (ChildBranchIdx == BTSpecialChild::ReturnToParent)
			{
				UBTCompositeNode* ChildNode = TestNode;
				TestNode = TestNode->GetParentNode();

				// does it want to move up the tree?
				if (TestNode == NULL)
				{
					// special case for leaving instance: deactivate root manually
					ChildNode->OnNodeDeactivation(SearchData, NodeResult);

					// don't remove top instance from stack, so it could be looped
					if (ActiveInstanceIdx > 0)
					{
						StoreDebuggerSearchStep(InstanceStack[ActiveInstanceIdx].ActiveNode, ActiveInstanceIdx, NodeResult);
						StoreDebuggerRemovedInstance(ActiveInstanceIdx);
						InstanceStack[ActiveInstanceIdx].DeactivateNodes(SearchData, ActiveInstanceIdx);

						// and leave subtree
						ActiveInstanceIdx--;

						StoreDebuggerSearchStep(InstanceStack[ActiveInstanceIdx].ActiveNode, ActiveInstanceIdx, NodeResult);
						TestNode = InstanceStack[ActiveInstanceIdx].ActiveNode->GetParentNode();
					}
				}

				if (TestNode)
				{
					TestNode->OnChildDeactivation(SearchData, *ChildNode, NodeResult);
				}
			}
			else if (TestNode->Children.IsValidIndex(ChildBranchIdx))
			{
				// was new task found?
				NextTask = TestNode->Children[ChildBranchIdx].ChildTask;

				// or it wants to move down the tree?
				TestNode = TestNode->Children[ChildBranchIdx].ChildComposite;
			}

			// store after node deactivation had chance to modify result
			StoreDebuggerSearchStep(StoreNode, ActiveInstanceIdx, NodeResult);
		}

		// is search within requested bounds?
		if (NextTask)
		{
			const FBTNodeIndex NextTaskIdx(ActiveInstanceIdx, NextTask->GetExecutionIndex());
			bIsSearchValid = NextTaskIdx.TakesPriorityOver(ExecutionRequest.SearchEnd);
			
			// is new task is valid, but wants to ignore rerunning itself
			// check it's the same as active node (or any of active parallel tasks)
			if (bIsSearchValid && NextTask->ShouldIgnoreRestartSelf())
			{
				const bool bIsTaskRunning = InstanceStack[ActiveInstanceIdx].HasActiveNode(NextTaskIdx.ExecutionIndex);
				if (bIsTaskRunning)
				{
					BT_SEARCHLOG(SearchData, Verbose, TEXT("Task doesn't allow restart and it's already running! Discaring search."));
					bIsSearchValid = false;
				}
			}
		}

		if (bIsSearchValid)
		{
			// collect all aux nodes that have lower priority than new task
			// occurs when normal execution is forced to revisit lower priority nodes (e.g. loop decorator)
			const FBTNodeIndex NextTaskIdx = NextTask ? FBTNodeIndex(ActiveInstanceIdx, NextTask->GetExecutionIndex()) : FBTNodeIndex(0, 0);
			UnregisterAuxNodesUpTo(NextTaskIdx);

			// change aux nodes
			ApplySearchData(NextTask);
		}
		else
		{
			// rollback search
			ActiveInstanceIdx = PrevActiveInstanceIdx;
			CopyInstanceMemoryFromPersistent();

			// apply new observer changes
			ApplyDiscardedSearch();
		}

		SearchData.bSearchInProgress = false;
		// finish timer scope
	}

	if (SearchData.bPostponeSearch)
	{
		ScheduleExecutionUpdate();
	}
	else
	{
		ExecutionRequest = FBTNodeExecutionInfo();
	}

	// unlock execution data, can get locked again if AbortCurrentTask starts any new requests
	PendingExecution.Unlock();

	if (bIsSearchValid)
	{
		// abort task if needed
		if (InstanceStack.Last().ActiveNodeType == EBTActiveNode::ActiveTask)
		{
			PendingExecution.OnAbortStart();

			AbortCurrentTask();

			PendingExecution.OnAbortProcessed();
		}

		// set next task to execute
		PendingExecution.NextTask = NextTask;
		PendingExecution.bOutOfNodes = (NextTask == NULL);
	}
	else
	{
		UE_VLOG(GetOwner(), LogBehaviorTree, Verbose, TEXT("Search result is not valid, reverted all changes."));
	}

	ProcessPendingExecution();
}

void UBehaviorTreeComponent::ProcessPendingExecution()
{
	// can't continue if current task is still aborting
	if (bWaitingForAbortingTasks || !PendingExecution.IsSet())
	{
		return;
	}

	FBTPendingExecutionInfo SavedInfo = PendingExecution;
	PendingExecution = FBTPendingExecutionInfo();

	// make sure that we don't have any additional instances on stack
	if (InstanceStack.Num() > (ActiveInstanceIdx + 1))
	{
		for (int32 InstanceIndex = ActiveInstanceIdx + 1; InstanceIndex < InstanceStack.Num(); InstanceIndex++)
		{
			InstanceStack[InstanceIndex].Cleanup(*this, EBTMemoryClear::StoreSubtree);
		}

		InstanceStack.SetNum(ActiveInstanceIdx + 1);
	}

	// execute next task / notify out of nodes
	if (SavedInfo.NextTask)
	{
		ExecuteTask(SavedInfo.NextTask);
	}
	else
	{
		OnTreeFinished();
	}
}

bool UBehaviorTreeComponent::DeactivateUpTo(UBTCompositeNode* Node, uint16 NodeInstanceIdx, EBTNodeResult::Type& NodeResult)
{
	UBTNode* DeactivatedChild = InstanceStack[ActiveInstanceIdx].ActiveNode;
	bool bDeactivateRoot = true;

	if (DeactivatedChild == NULL && ActiveInstanceIdx > NodeInstanceIdx)
	{
		// use tree's root node if instance didn't activated itself yet
		DeactivatedChild = InstanceStack[ActiveInstanceIdx].RootNode;
		bDeactivateRoot = false;
	}

	while (DeactivatedChild)
	{
		UBTCompositeNode* NotifyParent = DeactivatedChild->GetParentNode();
		if (NotifyParent)
		{
			NotifyParent->OnChildDeactivation(SearchData, *DeactivatedChild, NodeResult);

			BT_SEARCHLOG(SearchData, Verbose, TEXT("Deactivate node: %s"), *UBehaviorTreeTypes::DescribeNodeHelper(DeactivatedChild));
			StoreDebuggerSearchStep(DeactivatedChild, ActiveInstanceIdx, NodeResult);
			DeactivatedChild = NotifyParent;
		}
		else
		{
			// special case for leaving instance: deactivate root manually
			if (bDeactivateRoot)
			{
				InstanceStack[ActiveInstanceIdx].RootNode->OnNodeDeactivation(SearchData, NodeResult);
			}

			BT_SEARCHLOG(SearchData, Verbose, TEXT("%s node: %s [leave subtree]"),
				bDeactivateRoot ? TEXT("Deactivate") : TEXT("Skip over"),
				*UBehaviorTreeTypes::DescribeNodeHelper(InstanceStack[ActiveInstanceIdx].RootNode));

			// clear flag, it's valid only for newest instance
			bDeactivateRoot = true;

			// shouldn't happen, but it's better to have built in failsafe just in case
			if (ActiveInstanceIdx == 0)
			{
				BT_SEARCHLOG(SearchData, Error, TEXT("Execution path does NOT contain common parent node, restarting tree! AI:%s"),
					*GetNameSafe(SearchData.OwnerComp.GetOwner()));

				RestartTree();
				return false;
			}

			ActiveInstanceIdx--;
			DeactivatedChild = InstanceStack[ActiveInstanceIdx].ActiveNode;
		}

		if (DeactivatedChild == Node)
		{
			break;
		}
	}

	return true;
}

void UBehaviorTreeComponent::UnregisterAuxNodesUpTo(const FBTNodeIndex& Index)
{
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceStack.Num(); InstanceIndex++)
	{
		FBehaviorTreeInstance& InstanceInfo = InstanceStack[InstanceIndex];
		for (int32 AuxIndex = 0; AuxIndex < InstanceInfo.ActiveAuxNodes.Num(); AuxIndex++)
		{
			FBTNodeIndex AuxIdx(InstanceIndex, InstanceInfo.ActiveAuxNodes[AuxIndex]->GetExecutionIndex());
			if (Index.TakesPriorityOver(AuxIdx))
			{
				SearchData.AddUniqueUpdate(FBehaviorTreeSearchUpdate(InstanceInfo.ActiveAuxNodes[AuxIndex], InstanceIndex, EBTNodeUpdateMode::Remove));
			}
		}
	}
}

void UBehaviorTreeComponent::UnregisterAuxNodesInBranch(const UBTCompositeNode* Node)
{
	const int32 InstanceIdx = FindInstanceContainingNode(Node);
	if (InstanceIdx != INDEX_NONE)
	{
		check(Node);

		TArray<FBehaviorTreeSearchUpdate> UpdateList;

		for (int32 Idx = 0; Idx < InstanceStack[InstanceIdx].ActiveAuxNodes.Num(); Idx++)
		{
			UBTAuxiliaryNode* AuxNode = InstanceStack[InstanceIdx].ActiveAuxNodes[Idx];
			if (AuxNode->GetTreeDepth() > Node->GetTreeDepth())
			{
				// check if aux node belongs to given branch
				UBTCompositeNode* TestNode = AuxNode->GetParentNode();
				while (TestNode && TestNode != Node)
				{
					TestNode = TestNode->GetParentNode();
				}

				if (TestNode == Node)
				{
					UpdateList.Add(FBehaviorTreeSearchUpdate(AuxNode, InstanceIdx, EBTNodeUpdateMode::Remove));
				}
			}
		}

		ApplySearchUpdates(UpdateList, 0);
	}
}

void UBehaviorTreeComponent::ExecuteTask(UBTTaskNode* TaskNode)
{
	SCOPE_CYCLE_COUNTER(STAT_AI_BehaviorTree_ExecutionTime);

	FBehaviorTreeInstance& ActiveInstance = InstanceStack[ActiveInstanceIdx];

	// task service activation is not part of search update (although deactivation is, through DeactivateUpTo), start them before execution
	for (int32 ServiceIndex = 0; ServiceIndex < TaskNode->Services.Num(); ServiceIndex++)
	{
		UBTService* ServiceNode = TaskNode->Services[ServiceIndex];
		uint8* NodeMemory = (uint8*)ServiceNode->GetNodeMemory<uint8>(ActiveInstance);

		ActiveInstance.ActiveAuxNodes.Add(ServiceNode);

		UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("Activating task service: %s"), *UBehaviorTreeTypes::DescribeNodeHelper(ServiceNode));
		ServiceNode->WrappedOnBecomeRelevant(*this, NodeMemory);
	}

	ActiveInstance.ActiveNode = TaskNode;
	ActiveInstance.ActiveNodeType = EBTActiveNode::ActiveTask;

	// make a snapshot for debugger
	StoreDebuggerExecutionStep(EBTExecutionSnap::Regular);

	UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("Execute task: %s"), *UBehaviorTreeTypes::DescribeNodeHelper(TaskNode));

	// store instance before execution, it could result in pushing a subtree
	uint16 InstanceIdx = ActiveInstanceIdx;

	uint8* NodeMemory = (uint8*)(TaskNode->GetNodeMemory<uint8>(ActiveInstance));
	EBTNodeResult::Type TaskResult = TaskNode->WrappedExecuteTask(*this, NodeMemory);

	// pass task finished if wasn't already notified (FinishLatentTask)
	const UBTNode* ActiveNodeAfterExecution = GetActiveNode();
	if (ActiveNodeAfterExecution == TaskNode)
	{
		// update task's runtime values after it had a chance to initialize memory
		UpdateDebuggerAfterExecution(TaskNode, InstanceIdx);

		OnTaskFinished(TaskNode, TaskResult);
	}
}

void UBehaviorTreeComponent::AbortCurrentTask()
{
	const int32 CurrentInstanceIdx = InstanceStack.Num() - 1;
	FBehaviorTreeInstance& CurrentInstance = InstanceStack[CurrentInstanceIdx];
	CurrentInstance.ActiveNodeType = EBTActiveNode::AbortingTask;

	UBTTaskNode* CurrentTask = (UBTTaskNode*)CurrentInstance.ActiveNode;

	// remove all observers before requesting abort
	UnregisterMessageObserversFrom(CurrentTask);

	UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("Abort task: %s"), *UBehaviorTreeTypes::DescribeNodeHelper(CurrentTask));

	// abort task using current state of tree
	uint8* NodeMemory = (uint8*)(CurrentTask->GetNodeMemory<uint8>(CurrentInstance));
	EBTNodeResult::Type TaskResult = CurrentTask->WrappedAbortTask(*this, NodeMemory);

	// pass task finished if wasn't already notified (FinishLatentAbort)
	if (CurrentInstance.ActiveNodeType == EBTActiveNode::AbortingTask &&
		CurrentInstanceIdx == (InstanceStack.Num() - 1))
	{
		OnTaskFinished(CurrentTask, TaskResult);
	}
}

void UBehaviorTreeComponent::RegisterMessageObserver(const UBTTaskNode* TaskNode, FName MessageType)
{
	if (TaskNode)
	{
		FBTNodeIndex NodeIdx;
		NodeIdx.ExecutionIndex = TaskNode->GetExecutionIndex();
		NodeIdx.InstanceIndex = InstanceStack.Num() - 1;

		TaskMessageObservers.Add(NodeIdx,
			FAIMessageObserver::Create(this, MessageType, FOnAIMessage::CreateUObject(TaskNode, &UBTTaskNode::ReceivedMessage))
			);

		UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("Message[%s] observer added for %s"),
			*MessageType.ToString(), *UBehaviorTreeTypes::DescribeNodeHelper(TaskNode));
	}
}

void UBehaviorTreeComponent::RegisterMessageObserver(const UBTTaskNode* TaskNode, FName MessageType, FAIRequestID RequestID)
{
	if (TaskNode)
	{
		FBTNodeIndex NodeIdx;
		NodeIdx.ExecutionIndex = TaskNode->GetExecutionIndex();
		NodeIdx.InstanceIndex = InstanceStack.Num() - 1;

		TaskMessageObservers.Add(NodeIdx,
			FAIMessageObserver::Create(this, MessageType, RequestID, FOnAIMessage::CreateUObject(TaskNode, &UBTTaskNode::ReceivedMessage))
			);

		UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("Message[%s:%d] observer added for %s"),
			*MessageType.ToString(), RequestID, *UBehaviorTreeTypes::DescribeNodeHelper(TaskNode));
	}
}

void UBehaviorTreeComponent::UnregisterMessageObserversFrom(const FBTNodeIndex& TaskIdx)
{
	const int32 NumRemoved = TaskMessageObservers.Remove(TaskIdx);
	if (NumRemoved)
	{
		UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("Message observers removed for task[%d:%d] (num:%d)"),
			TaskIdx.InstanceIndex, TaskIdx.ExecutionIndex, NumRemoved);
	}
}

void UBehaviorTreeComponent::UnregisterMessageObserversFrom(const UBTTaskNode* TaskNode)
{
	if (TaskNode && InstanceStack.Num())
	{
		const FBehaviorTreeInstance& ActiveInstance = InstanceStack.Last();

		FBTNodeIndex NodeIdx;
		NodeIdx.ExecutionIndex = TaskNode->GetExecutionIndex();
		NodeIdx.InstanceIndex = FindInstanceContainingNode(TaskNode);
		
		UnregisterMessageObserversFrom(NodeIdx);
	}
}

#if STATS
#define AUX_NODE_WRAPPER(cmd) \
	DEC_MEMORY_STAT_BY(STAT_AI_BehaviorTree_InstanceMemory, InstanceInfo.GetAllocatedSize()); \
	cmd; \
	INC_MEMORY_STAT_BY(STAT_AI_BehaviorTree_InstanceMemory, InstanceInfo.GetAllocatedSize());

#else
#define AUX_NODE_WRAPPER(cmd) cmd;
#endif // STATS

void UBehaviorTreeComponent::RegisterParallelTask(const UBTTaskNode* TaskNode)
{
	if (InstanceStack.IsValidIndex(ActiveInstanceIdx))
	{
		FBehaviorTreeInstance& InstanceInfo = InstanceStack[ActiveInstanceIdx];
		AUX_NODE_WRAPPER(InstanceInfo.ParallelTasks.Add(FBehaviorTreeParallelTask(TaskNode, EBTTaskStatus::Active)));

		UE_VLOG(GetOwner(), LogBehaviorTree, Verbose, TEXT("Parallel task: %s added to active list"),
			*UBehaviorTreeTypes::DescribeNodeHelper(TaskNode));

		if (InstanceInfo.ActiveNode == TaskNode)
		{
			// switch to inactive state, so it could start background tree
			InstanceInfo.ActiveNodeType = EBTActiveNode::InactiveTask;
		}
	}
}

void UBehaviorTreeComponent::UnregisterParallelTask(const UBTTaskNode* TaskNode, uint16 InstanceIdx)
{
	bool bShouldUpdate = false;
	if (InstanceStack.IsValidIndex(InstanceIdx))
	{
		FBehaviorTreeInstance& InstanceInfo = InstanceStack[InstanceIdx];
		for (int32 TaskIndex = InstanceInfo.ParallelTasks.Num() - 1; TaskIndex >= 0; TaskIndex--)
		{
			FBehaviorTreeParallelTask& ParallelInfo = InstanceInfo.ParallelTasks[TaskIndex];
			if (ParallelInfo.TaskNode == TaskNode)
			{
				UE_VLOG(GetOwner(), LogBehaviorTree, Verbose, TEXT("Parallel task: %s removed from active list"),
					*UBehaviorTreeTypes::DescribeNodeHelper(TaskNode));

				InstanceInfo.ParallelTasks.RemoveAt(TaskIndex, /*Count=*/1, /*bAllowShrinking=*/false);
				bShouldUpdate = true;
				break;
			}
		}
	}

	if (bShouldUpdate)
	{
		UpdateAbortingTasks();
	}
}

#undef AUX_NODE_WRAPPER

void UBehaviorTreeComponent::UpdateAbortingTasks()
{
	bWaitingForAbortingTasks = InstanceStack.Num() ? (InstanceStack.Last().ActiveNodeType == EBTActiveNode::AbortingTask) : false;

	for (int32 InstanceIndex = 0; InstanceIndex < InstanceStack.Num() && !bWaitingForAbortingTasks; InstanceIndex++)
	{
		FBehaviorTreeInstance& InstanceInfo = InstanceStack[InstanceIndex];
		for (int32 TaskIndex = InstanceInfo.ParallelTasks.Num() - 1; TaskIndex >= 0; TaskIndex--)
		{
			FBehaviorTreeParallelTask& ParallelInfo = InstanceInfo.ParallelTasks[TaskIndex];
			if (ParallelInfo.Status == EBTTaskStatus::Aborting)
			{
				bWaitingForAbortingTasks = true;
				break;
			}
		}
	}
}

bool UBehaviorTreeComponent::PushInstance(UBehaviorTree& TreeAsset)
{
	// check if blackboard class match
	if (TreeAsset.BlackboardAsset && BlackboardComp && !BlackboardComp->IsCompatibleWith(TreeAsset.BlackboardAsset))
	{
		UE_VLOG(GetOwner(), LogBehaviorTree, Warning, TEXT("Failed to execute tree %s: blackboard %s is not compatibile with current: %s!"),
			*TreeAsset.GetName(), *GetNameSafe(TreeAsset.BlackboardAsset), *GetNameSafe(BlackboardComp->GetBlackboardAsset()));

		return false;
	}

	UBehaviorTreeManager* BTManager = UBehaviorTreeManager::GetCurrent(GetWorld());
	if (BTManager == NULL)
	{
		UE_VLOG(GetOwner(), LogBehaviorTree, Warning, TEXT("Failed to execute tree %s: behavior tree manager not found!"), *TreeAsset.GetName());
		return false;
	}

	// check if parent node allows it
	const UBTNode* ActiveNode = GetActiveNode();
	const UBTCompositeNode* ActiveParent = ActiveNode ? ActiveNode->GetParentNode() : NULL;
	if (ActiveParent)
	{
		uint8* ParentMemory = GetNodeMemory((UBTNode*)ActiveParent, InstanceStack.Num() - 1);
		int32 ChildIdx = ActiveNode ? ActiveParent->GetChildIndex(*ActiveNode) : INDEX_NONE;

		const bool bIsAllowed = ActiveParent->CanPushSubtree(*this, ParentMemory, ChildIdx);
		if (!bIsAllowed)
		{
			UE_VLOG(GetOwner(), LogBehaviorTree, Warning, TEXT("Failed to execute tree %s: parent of active node does not allow it! (%s)"),
				*TreeAsset.GetName(), *UBehaviorTreeTypes::DescribeNodeHelper(ActiveParent));
			return false;
		}
	}

	UBTCompositeNode* RootNode = NULL;
	uint16 InstanceMemorySize = 0;

	const bool bLoaded = BTManager->LoadTree(TreeAsset, RootNode, InstanceMemorySize);
	if (bLoaded)
	{
		FBehaviorTreeInstance NewInstance;
		NewInstance.InstanceIdIndex = UpdateInstanceId(&TreeAsset, ActiveNode, InstanceStack.Num() - 1);
		NewInstance.RootNode = RootNode;
		NewInstance.ActiveNode = NULL;
		NewInstance.ActiveNodeType = EBTActiveNode::Composite;

		// initialize memory and node instances
		FBehaviorTreeInstanceId& InstanceInfo = KnownInstances[NewInstance.InstanceIdIndex];
		int32 NodeInstanceIndex = InstanceInfo.FirstNodeInstance;
		const bool bFirstTime = (InstanceInfo.InstanceMemory.Num() != InstanceMemorySize);
		if (bFirstTime)
		{
			InstanceInfo.InstanceMemory.AddZeroed(InstanceMemorySize);
			InstanceInfo.RootNode = RootNode;
		}

		NewInstance.InstanceMemory = InstanceInfo.InstanceMemory;
		NewInstance.Initialize(*this, *RootNode, NodeInstanceIndex, bFirstTime ? EBTMemoryInit::Initialize : EBTMemoryInit::RestoreSubtree);

		INC_DWORD_STAT(STAT_AI_BehaviorTree_NumInstances);
		InstanceStack.Push(NewInstance);
		ActiveInstanceIdx = InstanceStack.Num() - 1;

		// start root level services now (they won't be removed on looping tree anyway)
		for (int32 ServiceIndex = 0; ServiceIndex < RootNode->Services.Num(); ServiceIndex++)
		{
			UBTService* ServiceNode = RootNode->Services[ServiceIndex];
			uint8* NodeMemory = (uint8*)ServiceNode->GetNodeMemory<uint8>(InstanceStack[ActiveInstanceIdx]);

			InstanceStack[ActiveInstanceIdx].ActiveAuxNodes.Add(ServiceNode);
			ServiceNode->WrappedOnBecomeRelevant(*this, NodeMemory);
		}

		FBehaviorTreeDelegates::OnTreeStarted.Broadcast(*this, TreeAsset);

		// start new task
		RequestExecution(RootNode, ActiveInstanceIdx, RootNode, 0, EBTNodeResult::InProgress);
		return true;
	}

	return false;
}

uint8 UBehaviorTreeComponent::UpdateInstanceId(UBehaviorTree* TreeAsset, const UBTNode* OriginNode, int32 OriginInstanceIdx)
{
	FBehaviorTreeInstanceId InstanceId;
	InstanceId.TreeAsset = TreeAsset;

	// build path from origin node
	{
		const uint16 ExecutionIndex = OriginNode ? OriginNode->GetExecutionIndex() : MAX_uint16;
		InstanceId.Path.Add(ExecutionIndex);
	}

	for (int32 InstanceIndex = OriginInstanceIdx - 1; InstanceIndex >= 0; InstanceIndex--)
	{
		const uint16 ExecutionIndex = InstanceStack[InstanceIndex].ActiveNode ? InstanceStack[InstanceIndex].ActiveNode->GetExecutionIndex() : MAX_uint16;
		InstanceId.Path.Add(ExecutionIndex);
	}

	// try to find matching existing Id
	for (int32 InstanceIndex = 0; InstanceIndex < KnownInstances.Num(); InstanceIndex++)
	{
		if (KnownInstances[InstanceIndex] == InstanceId)
		{
			return InstanceIndex;
		}
	}

	// add new one
	InstanceId.FirstNodeInstance = NodeInstances.Num();

	const int32 NewIndex = KnownInstances.Add(InstanceId);
	check(NewIndex < MAX_uint8);
	return NewIndex;
}

int32 UBehaviorTreeComponent::FindInstanceContainingNode(const UBTNode* Node) const
{
	int32 InstanceIdx = INDEX_NONE;

	const UBTNode* TemplateNode = FindTemplateNode(Node);
	if (TemplateNode && InstanceStack.Num())
	{
		if (InstanceStack[ActiveInstanceIdx].ActiveNode != TemplateNode)
		{
			const UBTNode* RootNode = TemplateNode;
			while (RootNode->GetParentNode())
			{
				RootNode = RootNode->GetParentNode();
			}

			for (int32 InstanceIndex = 0; InstanceIndex < InstanceStack.Num(); InstanceIndex++)
			{
				if (InstanceStack[InstanceIndex].RootNode == RootNode)
				{
					InstanceIdx = InstanceIndex;
					break;
				}
			}
		}
		else
		{
			InstanceIdx = ActiveInstanceIdx;
		}
	}

	return InstanceIdx;
}

UBTNode* UBehaviorTreeComponent::FindTemplateNode(const UBTNode* Node) const
{
	if (Node == NULL || !Node->IsInstanced() || Node->GetParentNode() == NULL)
	{
		return (UBTNode*)Node;
	}

	UBTCompositeNode* ParentNode = Node->GetParentNode();
	for (int32 ChildIndex = 0; ChildIndex < ParentNode->Children.Num(); ChildIndex++)
	{
		FBTCompositeChild& ChildInfo = ParentNode->Children[ChildIndex];

		if (ChildInfo.ChildTask)
		{
			if (ChildInfo.ChildTask->GetExecutionIndex() == Node->GetExecutionIndex())
			{
				return ChildInfo.ChildTask;
			}

			for (int32 ServiceIndex = 0; ServiceIndex < ChildInfo.ChildTask->Services.Num(); ServiceIndex++)
			{
				if (ChildInfo.ChildTask->Services[ServiceIndex]->GetExecutionIndex() == Node->GetExecutionIndex())
				{
					return ChildInfo.ChildTask->Services[ServiceIndex];
				}
			}
		}

		for (int32 DecoratorIndex = 0; DecoratorIndex < ChildInfo.Decorators.Num(); DecoratorIndex++)
		{
			if (ChildInfo.Decorators[DecoratorIndex]->GetExecutionIndex() == Node->GetExecutionIndex())
			{
				return ChildInfo.Decorators[DecoratorIndex];
			}
		}
	}

	for (int32 ServiceIndex = 0; ServiceIndex < ParentNode->Services.Num(); ServiceIndex++)
	{
		if (ParentNode->Services[ServiceIndex]->GetExecutionIndex() == Node->GetExecutionIndex())
		{
			return ParentNode->Services[ServiceIndex];
		}
	}

	return NULL;
}

uint8* UBehaviorTreeComponent::GetNodeMemory(UBTNode* Node, int32 InstanceIdx) const
{
	return InstanceStack.IsValidIndex(InstanceIdx) ? (uint8*)Node->GetNodeMemory<uint8>(InstanceStack[InstanceIdx]) : NULL;
}

void UBehaviorTreeComponent::RemoveAllInstances()
{
	if (InstanceStack.Num())
	{
		StopTree(EBTStopMode::Forced);
	}

	FBehaviorTreeInstance DummyInstance;
	for (int32 Idx = 0; Idx < KnownInstances.Num(); Idx++)
	{
		const FBehaviorTreeInstanceId& Info = KnownInstances[Idx];
		if (Info.InstanceMemory.Num())
		{
			// instance memory will be removed on Cleanup in EBTMemoryClear::Destroy mode
			// prevent from calling it multiple times - StopTree does it for current InstanceStack
			DummyInstance.InstanceMemory = Info.InstanceMemory;
			DummyInstance.InstanceIdIndex = Idx;
			DummyInstance.RootNode = Info.RootNode;

			DummyInstance.Cleanup(*this, EBTMemoryClear::Destroy);
		}
	}

	KnownInstances.Reset();
	NodeInstances.Reset();
}

void UBehaviorTreeComponent::CopyInstanceMemoryToPersistent()
{
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceStack.Num(); InstanceIndex++)
	{
		const FBehaviorTreeInstance& InstanceData = InstanceStack[InstanceIndex];
		FBehaviorTreeInstanceId& InstanceInfo = KnownInstances[InstanceData.InstanceIdIndex];

		InstanceInfo.InstanceMemory = InstanceData.InstanceMemory;
	}
}

void UBehaviorTreeComponent::CopyInstanceMemoryFromPersistent()
{
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceStack.Num(); InstanceIndex++)
	{
		FBehaviorTreeInstance& InstanceData = InstanceStack[InstanceIndex];
		const FBehaviorTreeInstanceId& InstanceInfo = KnownInstances[InstanceData.InstanceIdIndex];

		InstanceData.InstanceMemory = InstanceInfo.InstanceMemory;
	}
}

FString UBehaviorTreeComponent::GetDebugInfoString() const 
{ 
	FString DebugInfo;
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceStack.Num(); InstanceIndex++)
	{
		const FBehaviorTreeInstance& InstanceData = InstanceStack[InstanceIndex];
		const FBehaviorTreeInstanceId& InstanceInfo = KnownInstances[InstanceData.InstanceIdIndex];
		DebugInfo += FString::Printf(TEXT("Behavior tree: %s\n"), *GetNameSafe(InstanceInfo.TreeAsset));

		UBTNode* Node = InstanceData.ActiveNode;
		FString NodeTrace;

		while (Node)
		{
			uint8* NodeMemory = (uint8*)(Node->GetNodeMemory<uint8>(InstanceData));
			NodeTrace = FString::Printf(TEXT("  %s\n"), *Node->GetRuntimeDescription(*this, NodeMemory, EBTDescriptionVerbosity::Basic)) + NodeTrace;
			Node = Node->GetParentNode();
		}

		DebugInfo += NodeTrace;
	}

	return DebugInfo;
}

FString UBehaviorTreeComponent::DescribeActiveTasks() const
{
	FString ActiveTask(TEXT("None"));
	if (InstanceStack.Num())
	{
		const FBehaviorTreeInstance& LastInstance = InstanceStack.Last();
		if (LastInstance.ActiveNodeType == EBTActiveNode::ActiveTask)
		{
			ActiveTask = UBehaviorTreeTypes::DescribeNodeHelper(LastInstance.ActiveNode);
		}
	}

	FString ParallelTasks;
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceStack.Num(); InstanceIndex++)
	{
		const FBehaviorTreeInstance& InstanceInfo = InstanceStack[InstanceIndex];
		for (int32 ParallelNodeIndex = 0; ParallelNodeIndex < InstanceInfo.ParallelTasks.Num(); ParallelNodeIndex++)
		{
			const FBehaviorTreeParallelTask& ParallelInfo = InstanceInfo.ParallelTasks[ParallelNodeIndex];
			if (ParallelInfo.Status == EBTTaskStatus::Active)
			{
				ParallelTasks += UBehaviorTreeTypes::DescribeNodeHelper(ParallelInfo.TaskNode);
				ParallelTasks += TEXT(", ");
			}
		}
	}

	if (ParallelTasks.Len() > 0)
	{
		ActiveTask += TEXT(" (");
		ActiveTask += ParallelTasks.LeftChop(2);
		ActiveTask += TEXT(')');
	}

	return ActiveTask;
}

FString UBehaviorTreeComponent::DescribeActiveTrees() const
{
	FString Assets;
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceStack.Num(); InstanceIndex++)
	{
		const FBehaviorTreeInstanceId& InstanceInfo = KnownInstances[InstanceStack[InstanceIndex].InstanceIdIndex];
		Assets += InstanceInfo.TreeAsset->GetName();
		Assets += TEXT(", ");
	}

	return Assets.Len() ? Assets.LeftChop(2) : TEXT("None");
}

float UBehaviorTreeComponent::GetTagCooldownEndTime(FGameplayTag CooldownTag) const
{
	const float CooldownEndTime = CooldownTagsMap.FindRef(CooldownTag);
	return CooldownEndTime;
}

void UBehaviorTreeComponent::AddCooldownTagDuration(FGameplayTag CooldownTag, float CooldownDuration, bool bAddToExistingDuration)
{
	if (CooldownTag.IsValid())
	{
		float* CurrentEndTime = CooldownTagsMap.Find(CooldownTag);

		// If we are supposed to add to an existing duration, do that, otherwise we set a new value.
		if (bAddToExistingDuration && (CurrentEndTime != nullptr))
		{
			*CurrentEndTime += CooldownDuration;
		}
		else
		{
			CooldownTagsMap.Add(CooldownTag, (GetWorld()->GetTimeSeconds() + CooldownDuration));
		}
	}
}

bool SetDynamicSubtreeHelper(const UBTCompositeNode* TestComposite,
	const FBehaviorTreeInstance& InstanceInfo, const UBehaviorTreeComponent* OwnerComp,
	const FGameplayTag& InjectTag, UBehaviorTree* BehaviorAsset)
{
	bool bInjected = false;

	for (int32 Idx = 0; Idx < TestComposite->Children.Num(); Idx++)
	{
		const FBTCompositeChild& ChildInfo = TestComposite->Children[Idx];
		if (ChildInfo.ChildComposite)
		{
			bInjected = (SetDynamicSubtreeHelper(ChildInfo.ChildComposite, InstanceInfo, OwnerComp, InjectTag, BehaviorAsset) || bInjected);
		}
		else
		{
			UBTTask_RunBehaviorDynamic* SubtreeTask = Cast<UBTTask_RunBehaviorDynamic>(ChildInfo.ChildTask);
			if (SubtreeTask && SubtreeTask->HasMatchingTag(InjectTag))
			{
				const uint8* NodeMemory = SubtreeTask->GetNodeMemory<uint8>(InstanceInfo);
				UBTTask_RunBehaviorDynamic* InstancedNode = Cast<UBTTask_RunBehaviorDynamic>(SubtreeTask->GetNodeInstance(*OwnerComp, (uint8*)NodeMemory));
				if (InstancedNode)
				{
					const bool bAssetChanged = InstancedNode->SetBehaviorAsset(BehaviorAsset);
					if (bAssetChanged)
					{
						UE_VLOG(OwnerComp->GetOwner(), LogBehaviorTree, Log, TEXT("Replaced subtree in %s with %s (tag: %s)"),
							*UBehaviorTreeTypes::DescribeNodeHelper(SubtreeTask), *GetNameSafe(BehaviorAsset), *InjectTag.ToString());
						bInjected = true;
					}
				}
			}
		}
	}

	return bInjected;
}

void UBehaviorTreeComponent::SetDynamicSubtree(FGameplayTag InjectTag, UBehaviorTree* BehaviorAsset)
{
	bool bInjected = false;
	// replace at matching injection points
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceStack.Num(); InstanceIndex++)
	{
		const FBehaviorTreeInstance& InstanceInfo = InstanceStack[InstanceIndex];
		bInjected = (SetDynamicSubtreeHelper(InstanceInfo.RootNode, InstanceInfo, this, InjectTag, BehaviorAsset) || bInjected);
	}

	// restart subtree if it was replaced
	if (bInjected)
	{
		for (int32 InstanceIndex = 0; InstanceIndex < InstanceStack.Num(); InstanceIndex++)
		{
			const FBehaviorTreeInstance& InstanceInfo = InstanceStack[InstanceIndex];
			if (InstanceInfo.ActiveNodeType == EBTActiveNode::ActiveTask)
			{
				const UBTTask_RunBehaviorDynamic* SubtreeTask = Cast<const UBTTask_RunBehaviorDynamic>(InstanceInfo.ActiveNode);
				if (SubtreeTask && SubtreeTask->HasMatchingTag(InjectTag))
				{
					UBTCompositeNode* RestartNode = SubtreeTask->GetParentNode();
					int32 RestartChildIdx = RestartNode->GetChildIndex(*SubtreeTask);

					RequestExecution(RestartNode, InstanceIndex, SubtreeTask, RestartChildIdx, EBTNodeResult::Aborted);
					break;
				}
			}
		}
	}
	else
	{
		UE_VLOG(GetOwner(), LogBehaviorTree, Log, TEXT("Failed to inject subtree %s at tag %s"), *GetNameSafe(BehaviorAsset), *InjectTag.ToString());
	}
}

#if ENABLE_VISUAL_LOG
void UBehaviorTreeComponent::DescribeSelfToVisLog(FVisualLogEntry* Snapshot) const
{
	if (IsPendingKill())
	{
		return;
	}
	
	Super::DescribeSelfToVisLog(Snapshot);

	for (int32 InstanceIndex = 0; InstanceIndex < InstanceStack.Num(); InstanceIndex++)
	{
		const FBehaviorTreeInstance& InstanceInfo = InstanceStack[InstanceIndex];
		const FBehaviorTreeInstanceId& InstanceId = KnownInstances[InstanceInfo.InstanceIdIndex];

		FVisualLogStatusCategory StatusCategory;
		StatusCategory.Category = FString::Printf(TEXT("BehaviorTree %d (asset: %s)"), InstanceIndex, *GetNameSafe(InstanceId.TreeAsset));

		if (InstanceInfo.ActiveAuxNodes.Num() > 0)
		{
			FString ObserversDesc;
			for (auto AuxNode : InstanceInfo.ActiveAuxNodes)
			{
				ObserversDesc += FString::Printf(TEXT("%d. %s\n"), AuxNode->GetExecutionIndex(), *AuxNode->GetNodeName(), *AuxNode->GetStaticDescription());
			}
			StatusCategory.Add(TEXT("Observers"), ObserversDesc);
		}

		TArray<FString> Descriptions;
		UBTNode* Node = InstanceInfo.ActiveNode;
		while (Node)
		{
			uint8* NodeMemory = (uint8*)(Node->GetNodeMemory<uint8>(InstanceInfo));
			Descriptions.Add(Node->GetRuntimeDescription(*this, NodeMemory, EBTDescriptionVerbosity::Detailed));
		
			Node = Node->GetParentNode();
		}

		for (int32 DescriptionIndex = Descriptions.Num() - 1; DescriptionIndex >= 0; DescriptionIndex--)
		{
			int32 SplitIdx = INDEX_NONE;
			if (Descriptions[DescriptionIndex].FindChar(TEXT(','), SplitIdx))
			{
				const FString KeyDesc = Descriptions[DescriptionIndex].Left(SplitIdx);
				const FString ValueDesc = Descriptions[DescriptionIndex].Mid(SplitIdx + 1).TrimStart();

				StatusCategory.Add(KeyDesc, ValueDesc);
			}
			else
			{
				StatusCategory.Add(Descriptions[DescriptionIndex], TEXT(""));
			}
		}

		if (StatusCategory.Data.Num() == 0)
		{
			StatusCategory.Add(TEXT("root"), TEXT("not initialized"));
		}

		Snapshot->Status.Add(StatusCategory);
	}
}
#endif // ENABLE_VISUAL_LOG

void UBehaviorTreeComponent::StoreDebuggerExecutionStep(EBTExecutionSnap::Type SnapType)
{
#if USE_BEHAVIORTREE_DEBUGGER
	if (!IsDebuggerActive())
	{
		return;
	}

	FBehaviorTreeExecutionStep CurrentStep;
	CurrentStep.StepIndex = DebuggerSteps.Num() ? DebuggerSteps.Last().StepIndex + 1 : 0;
	CurrentStep.TimeStamp = GetWorld()->GetTimeSeconds();
	CurrentStep.BlackboardValues = SearchStartBlackboard;

	for (int32 InstanceIndex = 0; InstanceIndex < InstanceStack.Num(); InstanceIndex++)
	{
		const FBehaviorTreeInstance& ActiveInstance = InstanceStack[InstanceIndex];
		
		FBehaviorTreeDebuggerInstance StoreInfo;
		StoreDebuggerInstance(StoreInfo, InstanceIndex, SnapType);
		CurrentStep.InstanceStack.Add(StoreInfo);
	}

	for (int32 InstanceIndex = RemovedInstances.Num() - 1; InstanceIndex >= 0; InstanceIndex--)
	{
		CurrentStep.InstanceStack.Add(RemovedInstances[InstanceIndex]);
	}

	CurrentSearchFlow.Reset();
	CurrentRestarts.Reset();
	RemovedInstances.Reset();

	UBehaviorTreeManager* ManagerCDO = (UBehaviorTreeManager*)UBehaviorTreeManager::StaticClass()->GetDefaultObject();
	while (DebuggerSteps.Num() >= ManagerCDO->MaxDebuggerSteps)
	{
		DebuggerSteps.RemoveAt(0, /*Count=*/1, /*bAllowShrinking=*/false);
	}
	DebuggerSteps.Add(CurrentStep);
#endif
}

void UBehaviorTreeComponent::StoreDebuggerInstance(FBehaviorTreeDebuggerInstance& InstanceInfo, uint16 InstanceIdx, EBTExecutionSnap::Type SnapType) const
{
#if USE_BEHAVIORTREE_DEBUGGER
	if (!InstanceStack.IsValidIndex(InstanceIdx))
	{
		return;
	}

	const FBehaviorTreeInstance& ActiveInstance = InstanceStack[InstanceIdx];
	const FBehaviorTreeInstanceId& ActiveInstanceInfo = KnownInstances[ActiveInstance.InstanceIdIndex];
	InstanceInfo.TreeAsset = ActiveInstanceInfo.TreeAsset;
	InstanceInfo.RootNode = ActiveInstance.RootNode;

	if (SnapType == EBTExecutionSnap::Regular)
	{
		// traverse execution path
		UBTNode* StoreNode = ActiveInstance.ActiveNode ? ActiveInstance.ActiveNode : ActiveInstance.RootNode;
		while (StoreNode)
		{
			InstanceInfo.ActivePath.Add(StoreNode->GetExecutionIndex());
			StoreNode = StoreNode->GetParentNode();
		}

		// add aux nodes
		for (int32 NodeIndex = 0; NodeIndex < ActiveInstance.ActiveAuxNodes.Num(); NodeIndex++)
		{
			InstanceInfo.AdditionalActiveNodes.Add(ActiveInstance.ActiveAuxNodes[NodeIndex]->GetExecutionIndex());
		}

		// add active parallels
		for (int32 TaskIndex = 0; TaskIndex < ActiveInstance.ParallelTasks.Num(); TaskIndex++)
		{
			const FBehaviorTreeParallelTask& TaskInfo = ActiveInstance.ParallelTasks[TaskIndex];
			InstanceInfo.AdditionalActiveNodes.Add(TaskInfo.TaskNode->GetExecutionIndex());
		}

		// runtime values
		StoreDebuggerRuntimeValues(InstanceInfo.RuntimeDesc, ActiveInstance.RootNode, InstanceIdx);
	}

	// handle restart triggers
	if (CurrentRestarts.IsValidIndex(InstanceIdx))
	{
		InstanceInfo.PathFromPrevious = CurrentRestarts[InstanceIdx];
	}

	// store search flow, but remove nodes on execution path
	if (CurrentSearchFlow.IsValidIndex(InstanceIdx))
	{
		for (int32 FlowIndex = 0; FlowIndex < CurrentSearchFlow[InstanceIdx].Num(); FlowIndex++)
		{
			if (!InstanceInfo.ActivePath.Contains(CurrentSearchFlow[InstanceIdx][FlowIndex].ExecutionIndex))
			{
				InstanceInfo.PathFromPrevious.Add(CurrentSearchFlow[InstanceIdx][FlowIndex]);
			}
		}
	}
#endif
}

void UBehaviorTreeComponent::StoreDebuggerRemovedInstance(uint16 InstanceIdx) const
{
#if USE_BEHAVIORTREE_DEBUGGER
	if (!IsDebuggerActive())
	{
		return;
	}

	FBehaviorTreeDebuggerInstance StoreInfo;
	StoreDebuggerInstance(StoreInfo, InstanceIdx, EBTExecutionSnap::OutOfNodes);

	RemovedInstances.Add(StoreInfo);
#endif
}

void UBehaviorTreeComponent::StoreDebuggerSearchStep(const UBTNode* Node, uint16 InstanceIdx, EBTNodeResult::Type NodeResult) const
{
#if USE_BEHAVIORTREE_DEBUGGER
	if (!IsDebuggerActive())
	{
		return;
	}

	if (Node && NodeResult != EBTNodeResult::InProgress && NodeResult != EBTNodeResult::Aborted)
	{
		FBehaviorTreeDebuggerInstance::FNodeFlowData FlowInfo;
		FlowInfo.ExecutionIndex = Node->GetExecutionIndex();
		FlowInfo.bPassed = (NodeResult == EBTNodeResult::Succeeded);
		
		if (CurrentSearchFlow.Num() < (InstanceIdx + 1))
		{
			CurrentSearchFlow.SetNum(InstanceIdx + 1);
		}

		if (CurrentSearchFlow[InstanceIdx].Num() == 0 || CurrentSearchFlow[InstanceIdx].Last().ExecutionIndex != FlowInfo.ExecutionIndex)
		{
			CurrentSearchFlow[InstanceIdx].Add(FlowInfo);
		}
	}
#endif
}

void UBehaviorTreeComponent::StoreDebuggerSearchStep(const UBTNode* Node, uint16 InstanceIdx, bool bPassed) const
{
#if USE_BEHAVIORTREE_DEBUGGER
	if (!IsDebuggerActive())
	{
		return;
	}

	if (Node && !bPassed)
	{
		FBehaviorTreeDebuggerInstance::FNodeFlowData FlowInfo;
		FlowInfo.ExecutionIndex = Node->GetExecutionIndex();
		FlowInfo.bPassed = bPassed;

		if (CurrentSearchFlow.Num() < (InstanceIdx + 1))
		{
			CurrentSearchFlow.SetNum(InstanceIdx + 1);
		}

		CurrentSearchFlow[InstanceIdx].Add(FlowInfo);
	}
#endif
}

void UBehaviorTreeComponent::StoreDebuggerRestart(const UBTNode* Node, uint16 InstanceIdx, bool bAllowed)
{
#if USE_BEHAVIORTREE_DEBUGGER
	if (!IsDebuggerActive())
	{
		return;
	}

	if (Node)
	{
		FBehaviorTreeDebuggerInstance::FNodeFlowData FlowInfo;
		FlowInfo.ExecutionIndex = Node->GetExecutionIndex();
		FlowInfo.bTrigger = bAllowed;
		FlowInfo.bDiscardedTrigger = !bAllowed;

		if (CurrentRestarts.Num() < (InstanceIdx + 1))
		{
			CurrentRestarts.SetNum(InstanceIdx + 1);
		}

		CurrentRestarts[InstanceIdx].Add(FlowInfo);
	}
#endif
}

void UBehaviorTreeComponent::StoreDebuggerRuntimeValues(TArray<FString>& RuntimeDescriptions, UBTNode* RootNode, uint16 InstanceIdx) const
{
#if USE_BEHAVIORTREE_DEBUGGER
	if (!InstanceStack.IsValidIndex(InstanceIdx))
	{
		return;
	}

	const FBehaviorTreeInstance& InstanceInfo = InstanceStack[InstanceIdx];

	TArray<FString> RuntimeValues;
	for (UBTNode* Node = RootNode; Node; Node = Node->GetNextNode())
	{
		uint8* NodeMemory = (uint8*)Node->GetNodeMemory<uint8>(InstanceInfo);

		RuntimeValues.Reset();
		Node->DescribeRuntimeValues(*this, NodeMemory, EBTDescriptionVerbosity::Basic, RuntimeValues);

		FString ComposedDesc;
		for (int32 ValueIndex = 0; ValueIndex < RuntimeValues.Num(); ValueIndex++)
		{
			if (ComposedDesc.Len())
			{
				ComposedDesc.AppendChar(TEXT('\n'));
			}

			ComposedDesc += RuntimeValues[ValueIndex];
		}

		RuntimeDescriptions.SetNum(Node->GetExecutionIndex() + 1);
		RuntimeDescriptions[Node->GetExecutionIndex()] = ComposedDesc;
	}
#endif
}

void UBehaviorTreeComponent::UpdateDebuggerAfterExecution(const UBTTaskNode* TaskNode, uint16 InstanceIdx) const
{
#if USE_BEHAVIORTREE_DEBUGGER
	if (!IsDebuggerActive() || !InstanceStack.IsValidIndex(InstanceIdx))
	{
		return;
	}

	FBehaviorTreeExecutionStep& CurrentStep = DebuggerSteps.Last();

	// store runtime values
	TArray<FString> RuntimeValues;
	const FBehaviorTreeInstance& InstanceToUpdate = InstanceStack[InstanceIdx];
	uint8* NodeMemory = (uint8*)TaskNode->GetNodeMemory<uint8>(InstanceToUpdate);
	TaskNode->DescribeRuntimeValues(*this, NodeMemory, EBTDescriptionVerbosity::Basic, RuntimeValues);

	FString ComposedDesc;
	for (int32 ValueIndex = 0; ValueIndex < RuntimeValues.Num(); ValueIndex++)
	{
		if (ComposedDesc.Len())
		{
			ComposedDesc.AppendChar(TEXT('\n'));
		}

		ComposedDesc += RuntimeValues[ValueIndex];
	}

	// accessing RuntimeDesc should never be out of bounds (active task MUST be part of active instance)
	const uint16& ExecutionIndex = TaskNode->GetExecutionIndex();
	if (CurrentStep.InstanceStack[InstanceIdx].RuntimeDesc.IsValidIndex(ExecutionIndex))
	{
		CurrentStep.InstanceStack[InstanceIdx].RuntimeDesc[ExecutionIndex] = ComposedDesc;
	}
	else
	{
		UE_VLOG(GetOwner(), LogBehaviorTree, Error, TEXT("Incomplete debugger data! No runtime description for executed task, instance %d has only %d entries!"),
			InstanceIdx, CurrentStep.InstanceStack[InstanceIdx].RuntimeDesc.Num());
	}
#endif
}

void UBehaviorTreeComponent::StoreDebuggerBlackboard(TMap<FName, FString>& BlackboardValueDesc) const
{
#if USE_BEHAVIORTREE_DEBUGGER
	if (!IsDebuggerActive())
	{
		return;
	}

	if (BlackboardComp && BlackboardComp->HasValidAsset())
	{
		const int32 NumKeys = BlackboardComp->GetNumKeys();
		BlackboardValueDesc.Empty(NumKeys);

		for (int32 KeyIndex = 0; KeyIndex < NumKeys; KeyIndex++)
		{
			FString Value = BlackboardComp->DescribeKeyValue(KeyIndex, EBlackboardDescription::OnlyValue);
			if (Value.Len() == 0)
			{
				Value = TEXT("n/a");
			}

			BlackboardValueDesc.Add(BlackboardComp->GetKeyName(KeyIndex), Value);
		}
	}
#endif
}

bool UBehaviorTreeComponent::IsDebuggerActive()
{
#if USE_BEHAVIORTREE_DEBUGGER
	if (ActiveDebuggerCounter <= 0)
	{
		static bool bAlwaysGatherData = false;
		static uint64 PrevFrameCounter = 0;

		if (GFrameCounter != PrevFrameCounter)
		{
			GConfig->GetBool(TEXT("/Script/UnrealEd.EditorPerProjectUserSettings"), TEXT("bAlwaysGatherBehaviorTreeDebuggerData"), bAlwaysGatherData, GEditorPerProjectIni);
			PrevFrameCounter = GFrameCounter;
		}

		return bAlwaysGatherData;
	}

	return true;
#else
	return false;
#endif
}

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/PoseAsset.h"
#include "AnimationRuntime.h"
#include "Animation/BlendSpaceBase.h"
#include "AnimationUtils.h"
#include "Logging/MessageLog.h"
#include "Animation/AnimNode_AssetPlayerBase.h"
#include "Animation/AnimNode_StateMachine.h"
#include "Animation/AnimNode_TransitionResult.h"
#include "Animation/AnimNode_SaveCachedPose.h"
#include "Animation/AnimNode_SubInput.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

#define DO_ANIMSTAT_PROCESSING(StatName) DEFINE_STAT(STAT_ ## StatName)
#include "AnimMTStats.h"
#undef DO_ANIMSTAT_PROCESSING

#define DO_ANIMSTAT_PROCESSING(StatName) DEFINE_STAT(STAT_ ## StatName ## _WorkerThread)
#include "AnimMTStats.h"
#undef DO_ANIMSTAT_PROCESSING

#define LOCTEXT_NAMESPACE "AnimInstance"

void FAnimInstanceProxy::UpdateAnimationNode(float DeltaSeconds)
{
#if WITH_EDITORONLY_DATA
	UpdatedNodesThisFrame.Reset();
#endif

	if(RootNode != nullptr)
	{
		UpdateCounter.Increment();
		RootNode->Update_AnyThread(FAnimationUpdateContext(this, DeltaSeconds));

		// We've updated the graph, now update the fractured saved pose sections
		for(FAnimNode_SaveCachedPose* PoseNode : SavedPoseQueue)
		{
			PoseNode->PostGraphUpdate();
		}
	}
}

void FAnimInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	// copy anim instance object if it has not already been set up
	AnimInstanceObject = InAnimInstance;

	AnimClassInterface = IAnimClassInterface::GetFromClass(InAnimInstance->GetClass());

	InitializeObjects(InAnimInstance);

	if (AnimClassInterface)
	{
		// Grab a pointer to the root node
		if (UStructProperty* RootAnimNodeProperty = AnimClassInterface->GetRootAnimNodeProperty())
		{
			RootNode = RootAnimNodeProperty->ContainerPtrToValuePtr<FAnimNode_Base>(InAnimInstance);
		}
		else
		{
			RootNode = nullptr;
		}

		// Initialise the pose node list
		const TArray<int32>& PoseNodeIndices = AnimClassInterface->GetOrderedSavedPoseNodeIndices();
		const TArray<UStructProperty*>& AnimNodeProperties = AnimClassInterface->GetAnimNodeProperties();
		SavedPoseQueue.Empty(PoseNodeIndices.Num());
		for(int32 Idx : PoseNodeIndices)
		{
			int32 ActualPropertyIdx = AnimNodeProperties.Num() - 1 - Idx;
			FAnimNode_SaveCachedPose* ActualPoseNode = AnimNodeProperties[ActualPropertyIdx]->ContainerPtrToValuePtr<FAnimNode_SaveCachedPose>(InAnimInstance);
			SavedPoseQueue.Add(ActualPoseNode);
		}

		// if no mesh, use Blueprint Skeleton
		if (Skeleton == nullptr)
		{
			Skeleton = AnimClassInterface->GetTargetSkeleton();
		}

		// Initialize state buffers
		int32 NumStates = 0;
		if(IAnimClassInterface* Interface = GetAnimClassInterface())
		{
			const TArray<FBakedAnimationStateMachine>& BakedMachines = Interface->GetBakedStateMachines();
			const int32 NumMachines = BakedMachines.Num();
			for(int32 MachineClassIndex = 0; MachineClassIndex < NumMachines; ++MachineClassIndex)
			{
				const FBakedAnimationStateMachine& Machine = BakedMachines[MachineClassIndex];
				StateMachineClassIndexToWeightOffset.Add(MachineClassIndex, NumStates);
				NumStates += Machine.States.Num();
			}
			StateWeightArrays[0].Reset(NumStates);
			StateWeightArrays[0].AddZeroed(NumStates);
			StateWeightArrays[1].Reset(NumStates);
			StateWeightArrays[1].AddZeroed(NumStates);

			MachineWeightArrays[0].Reset(NumMachines);
			MachineWeightArrays[0].AddZeroed(NumMachines);
			MachineWeightArrays[1].Reset(NumMachines);
			MachineWeightArrays[1].AddZeroed(NumMachines);
		}

#if WITH_EDITORONLY_DATA
		if (UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(InAnimInstance->GetClass()->ClassGeneratedBy))
		{
			if (Blueprint->Status == BS_Error)
			{
				RootNode = nullptr;
			}
		}
#endif
	}
	else
	{
		RootNode = (FAnimNode_Base*) GetCustomRootNode();
	}

#if !NO_LOGGING
	ActorName = GetNameSafe(InAnimInstance->GetOwningActor());
#endif

#if DO_CHECK
	AnimInstanceName = *InAnimInstance->GetFullName();
#endif

	UpdateCounter.Reset();
	ReinitializeSlotNodes();

	if (const USkeletalMeshComponent* SkelMeshComp = InAnimInstance->GetOwningComponent())
	{
		ComponentTransform = SkelMeshComp->GetComponentTransform();
		ComponentRelativeTransform = SkeletalMeshComponent->GetRelativeTransform();

		const AActor* OwningActor = SkeletalMeshComponent->GetOwner();
		ActorTransform = OwningActor ? OwningActor->GetActorTransform() : FTransform::Identity;
	}
	else
	{
		ComponentTransform = FTransform::Identity;
		ComponentRelativeTransform = FTransform::Identity;
		ActorTransform = FTransform::Identity;
	}
}

void FAnimInstanceProxy::InitializeRootNode()
{
	if (RootNode != nullptr)
	{
		GameThreadPreUpdateNodes.Reset();
		DynamicResetNodes.Reset();

		auto InitializeNode = [this](FAnimNode_Base* AnimNode)
		{
			AnimNode->OnInitializeAnimInstance(this, CastChecked<UAnimInstance>(GetAnimInstanceObject()));

			// Force our functions to be re-evaluated - this reinitialization may have been a 
			// consequence of our class being recompiled and functions will be invalid in that
			// case.
			AnimNode->EvaluateGraphExposedInputs.bInitialized = false;
			AnimNode->EvaluateGraphExposedInputs.Initialize(AnimNode, AnimInstanceObject);

			if (AnimNode->HasPreUpdate())
			{
				GameThreadPreUpdateNodes.Add(AnimNode);
			}

			if (AnimNode->NeedsDynamicReset())
			{
				DynamicResetNodes.Add(AnimNode);
			}
		};

		if(AnimClassInterface)
		{
			// cache any state machine descriptions we have
			for (UStructProperty* Property : AnimClassInterface->GetAnimNodeProperties())
			{
				if (Property->Struct->IsChildOf(FAnimNode_Base::StaticStruct()))
				{
					FAnimNode_Base* AnimNode = Property->ContainerPtrToValuePtr<FAnimNode_Base>(AnimInstanceObject);
					if (AnimNode)
					{
						InitializeNode(AnimNode);

						if (Property->Struct->IsChildOf(FAnimNode_StateMachine::StaticStruct()))
						{
							FAnimNode_StateMachine* StateMachine = static_cast<FAnimNode_StateMachine*>(AnimNode);
							StateMachine->CacheMachineDescription(AnimClassInterface);
						}

						if (Property->Struct->IsChildOf(FAnimNode_SubInput::StaticStruct()))
						{
							check(!SubInstanceInputNode); // Should only ever have one
							SubInstanceInputNode = static_cast<FAnimNode_SubInput*>(AnimNode);
						}
					}
				}
			}
		}
		else
		{
			//We have a custom root node, so get the associated nodes and initialize them
			TArray<FAnimNode_Base*> CustomNodes;
			GetCustomNodes(CustomNodes);
			for(FAnimNode_Base* Node : CustomNodes)
			{
				if(Node)
				{
					InitializeNode(Node);
				}
			}
		}
		

		InitializationCounter.Increment();
		FAnimationInitializeContext InitContext(this);
		RootNode->Initialize_AnyThread(InitContext);
	}
}

void FAnimInstanceProxy::Uninitialize(UAnimInstance* InAnimInstance)
{
	MontageEvaluationData.Reset();
	SubInstanceInputNode = nullptr;
}

void FAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	CurrentDeltaSeconds = DeltaSeconds;
	RootMotionMode = InAnimInstance->RootMotionMode;
	bShouldExtractRootMotion = InAnimInstance->ShouldExtractRootMotion();

	InitializeObjects(InAnimInstance);

	if (USkeletalMeshComponent* SkelMeshComp = InAnimInstance->GetSkelMeshComponent())
	{
		// Save off LOD level that we're currently using.
		LODLevel = SkelMeshComp->PredictedLODLevel;

		// Cache these transforms, so nodes don't have to pull it off the gamethread manually.
		SkelMeshCompLocalToWorld = SkelMeshComp->GetComponentTransform();
		if (const AActor* Owner = SkelMeshComp->GetOwner())
		{
			SkelMeshCompOwnerTransform = Owner->GetTransform();
		}
	}

	NotifyQueue.Reset(InAnimInstance->GetSkelMeshComponent());

#if ENABLE_ANIM_DRAW_DEBUG
	QueuedDrawDebugItems.Reset();
#endif

	ClearSlotNodeWeights();

	// Reset the player tick list (but keep it presized)
	TArray<FAnimTickRecord>& UngroupedActivePlayers = UngroupedActivePlayerArrays[GetSyncGroupWriteIndex()];
	UngroupedActivePlayers.Reset();

	TArray<FAnimGroupInstance>& SyncGroups = SyncGroupArrays[GetSyncGroupWriteIndex()];
	for (int32 GroupIndex = 0; GroupIndex < SyncGroups.Num(); ++GroupIndex)
	{
		SyncGroups[GroupIndex].Reset();
	}

	TArray<float>& StateWeights = StateWeightArrays[GetSyncGroupWriteIndex()];
	FMemory::Memset(StateWeights.GetData(), 0, StateWeights.Num() * sizeof(float));

	TArray<float>& MachineWeights = MachineWeightArrays[GetSyncGroupWriteIndex()];
	FMemory::Memset(MachineWeights.GetData(), 0, MachineWeights.Num() * sizeof(float));

#if WITH_EDITORONLY_DATA
	bIsBeingDebugged = false;
	if (UAnimBlueprint* AnimBlueprint = GetAnimBlueprint())
	{
		bIsBeingDebugged = (AnimBlueprint->GetObjectBeingDebugged() == InAnimInstance);
		if(bIsBeingDebugged)
		{
			UAnimBlueprintGeneratedClass* AnimBlueprintGeneratedClass = Cast<UAnimBlueprintGeneratedClass>(InAnimInstance->GetClass());
			FAnimBlueprintDebugData& DebugData = AnimBlueprintGeneratedClass->GetAnimBlueprintDebugData();
			PoseWatchEntriesForThisFrame = DebugData.AnimNodePoseWatch;
		}
	}
#endif

	ComponentTransform = SkeletalMeshComponent->GetComponentTransform();
	ComponentRelativeTransform = SkeletalMeshComponent->GetRelativeTransform();
	ActorTransform = SkeletalMeshComponent->GetOwner() ? SkeletalMeshComponent->GetOwner()->GetActorTransform() : FTransform::Identity;

	// run preupdate calls
	for (FAnimNode_Base* Node : GameThreadPreUpdateNodes)
	{
		Node->PreUpdate(InAnimInstance);
	}
}

void FAnimInstanceProxy::SavePoseSnapshot(USkeletalMeshComponent* InSkeletalMeshComponent, FName SnapshotName)
{
	FPoseSnapshot* PoseSnapshot = PoseSnapshots.FindByPredicate([SnapshotName](const FPoseSnapshot& PoseData) { return PoseData.SnapshotName == SnapshotName; });
	if (PoseSnapshot == nullptr)
	{
		PoseSnapshot = &PoseSnapshots[PoseSnapshots.AddDefaulted()];
		PoseSnapshot->SnapshotName = SnapshotName;
	}

	InSkeletalMeshComponent->SnapshotPose(*PoseSnapshot);
}

void FAnimInstanceProxy::PostUpdate(UAnimInstance* InAnimInstance) const
{
#if WITH_EDITORONLY_DATA
	if(bIsBeingDebugged)
	{
		UAnimBlueprintGeneratedClass* AnimBlueprintGeneratedClass = Cast<UAnimBlueprintGeneratedClass>(InAnimInstance->GetClass());
		FAnimBlueprintDebugData& DebugData = AnimBlueprintGeneratedClass->GetAnimBlueprintDebugData();
		DebugData.RecordNodeVisitArray(UpdatedNodesThisFrame);
		DebugData.AnimNodePoseWatch = PoseWatchEntriesForThisFrame;
	}
#endif

	InAnimInstance->NotifyQueue.Append(NotifyQueue);
	InAnimInstance->NotifyQueue.ApplyMontageNotifies(*this);

	// Send Queued DrawDebug Commands.
#if ENABLE_ANIM_DRAW_DEBUG
	for (const FQueuedDrawDebugItem& DebugItem : QueuedDrawDebugItems)
	{
		switch (DebugItem.ItemType)
		{
		case EDrawDebugItemType::OnScreenMessage: GEngine->AddOnScreenDebugMessage(INDEX_NONE, 0.f, DebugItem.Color, DebugItem.Message, false, DebugItem.TextScale); break;
		case EDrawDebugItemType::DirectionalArrow: DrawDebugDirectionalArrow(InAnimInstance->GetSkelMeshComponent()->GetWorld(), DebugItem.StartLoc, DebugItem.EndLoc, DebugItem.Size, DebugItem.Color, DebugItem.bPersistentLines, DebugItem.LifeTime, 0, DebugItem.Thickness); break;
		case EDrawDebugItemType::Sphere : DrawDebugSphere(InAnimInstance->GetSkelMeshComponent()->GetWorld(), DebugItem.Center, DebugItem.Radius, DebugItem.Segments, DebugItem.Color, DebugItem.bPersistentLines, DebugItem.LifeTime, 0, DebugItem.Thickness); break;
		case EDrawDebugItemType::Line: DrawDebugLine(InAnimInstance->GetSkelMeshComponent()->GetWorld(), DebugItem.StartLoc, DebugItem.EndLoc, DebugItem.Color, DebugItem.bPersistentLines, DebugItem.LifeTime, 0, DebugItem.Thickness); break;
		case EDrawDebugItemType::CoordinateSystem : DrawDebugCoordinateSystem(InAnimInstance->GetSkelMeshComponent()->GetWorld(), DebugItem.StartLoc, DebugItem.Rotation, DebugItem.Size, DebugItem.bPersistentLines, DebugItem.LifeTime, 0, DebugItem.Thickness); break;
		}
	}
#endif
}

void FAnimInstanceProxy::InitializeObjects(UAnimInstance* InAnimInstance)
{
	SkeletalMeshComponent = InAnimInstance->GetSkelMeshComponent();
	if (SkeletalMeshComponent->SkeletalMesh != nullptr)
	{
		Skeleton = SkeletalMeshComponent->SkeletalMesh->Skeleton;
	}
	else
	{
		Skeleton = nullptr;
	}
}

void FAnimInstanceProxy::ClearObjects()
{
	SkeletalMeshComponent = nullptr;
	Skeleton = nullptr;
}

FAnimTickRecord& FAnimInstanceProxy::CreateUninitializedTickRecord(int32 GroupIndex, FAnimGroupInstance*& OutSyncGroupPtr)
{
	// Find or create the sync group if there is one
	OutSyncGroupPtr = NULL;
	if (GroupIndex >= 0)
	{
		TArray<FAnimGroupInstance>& SyncGroups = SyncGroupArrays[GetSyncGroupWriteIndex()];
		while (SyncGroups.Num() <= GroupIndex)
		{
			new (SyncGroups) FAnimGroupInstance();
		}
		OutSyncGroupPtr = &(SyncGroups[GroupIndex]);
	}

	// Create the record
	FAnimTickRecord* TickRecord = new ((OutSyncGroupPtr != NULL) ? OutSyncGroupPtr->ActivePlayers : UngroupedActivePlayerArrays[GetSyncGroupWriteIndex()]) FAnimTickRecord();
	return *TickRecord;
}

void FAnimInstanceProxy::MakeSequenceTickRecord(FAnimTickRecord& TickRecord, class UAnimSequenceBase* Sequence, bool bLooping, float PlayRate, float FinalBlendWeight, float& CurrentTime, FMarkerTickRecord& MarkerTickRecord) const
{
	TickRecord.SourceAsset = Sequence;
	TickRecord.TimeAccumulator = &CurrentTime;
	TickRecord.MarkerTickRecord = &MarkerTickRecord;
	TickRecord.PlayRateMultiplier = PlayRate;
	TickRecord.EffectiveBlendWeight = FinalBlendWeight;
	TickRecord.bLooping = bLooping;
}

void FAnimInstanceProxy::MakeBlendSpaceTickRecord(FAnimTickRecord& TickRecord, class UBlendSpaceBase* BlendSpace, const FVector& BlendInput, TArray<FBlendSampleData>& BlendSampleDataCache, FBlendFilter& BlendFilter, bool bLooping, float PlayRate, float FinalBlendWeight, float& CurrentTime, FMarkerTickRecord& MarkerTickRecord) const
{
	TickRecord.SourceAsset = BlendSpace;
	TickRecord.BlendSpace.BlendSpacePositionX = BlendInput.X;
	TickRecord.BlendSpace.BlendSpacePositionY = BlendInput.Y;
	TickRecord.BlendSpace.BlendSampleDataCache = &BlendSampleDataCache;
	TickRecord.BlendSpace.BlendFilter = &BlendFilter;
	TickRecord.TimeAccumulator = &CurrentTime;
	TickRecord.MarkerTickRecord = &MarkerTickRecord;
	TickRecord.PlayRateMultiplier = PlayRate;
	TickRecord.EffectiveBlendWeight = FinalBlendWeight;
	TickRecord.bLooping = bLooping;
}

/** Helper function: make a tick record for a pose asset*/
void FAnimInstanceProxy::MakePoseAssetTickRecord(FAnimTickRecord& TickRecord, class UPoseAsset* PoseAsset, float FinalBlendWeight) const
{
	TickRecord.SourceAsset = PoseAsset;
	TickRecord.EffectiveBlendWeight = FinalBlendWeight;
}

void FAnimInstanceProxy::SequenceAdvanceImmediate(UAnimSequenceBase* Sequence, bool bLooping, float PlayRate, float DeltaSeconds, float& CurrentTime, FMarkerTickRecord& MarkerTickRecord)
{
	FAnimTickRecord TickRecord;
	MakeSequenceTickRecord(TickRecord, Sequence, bLooping, PlayRate, /*FinalBlendWeight=*/ 1.0f, CurrentTime, MarkerTickRecord);

	FAnimAssetTickContext TickContext(DeltaSeconds, RootMotionMode, true);
	TickRecord.SourceAsset->TickAssetPlayer(TickRecord, NotifyQueue, TickContext);
}

void FAnimInstanceProxy::BlendSpaceAdvanceImmediate(class UBlendSpaceBase* BlendSpace, const FVector& BlendInput, TArray<FBlendSampleData>& BlendSampleDataCache, FBlendFilter& BlendFilter, bool bLooping, float PlayRate, float DeltaSeconds, float& CurrentTime, FMarkerTickRecord& MarkerTickRecord)
{
	FAnimTickRecord TickRecord;
	MakeBlendSpaceTickRecord(TickRecord, BlendSpace, BlendInput, BlendSampleDataCache, BlendFilter, bLooping, PlayRate, /*FinalBlendWeight=*/ 1.0f, CurrentTime, MarkerTickRecord);
	
	FAnimAssetTickContext TickContext(DeltaSeconds, RootMotionMode, true);
	TickRecord.SourceAsset->TickAssetPlayer(TickRecord, NotifyQueue, TickContext);
}

void FAnimInstanceProxy::TickAssetPlayerInstances(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_TickAssetPlayerInstances);

	// Handle all players inside sync groups
	TArray<FAnimGroupInstance>& SyncGroups = SyncGroupArrays[GetSyncGroupWriteIndex()];
	const TArray<FAnimGroupInstance>& PreviousSyncGroups = SyncGroupArrays[GetSyncGroupReadIndex()];
	TArray<FAnimTickRecord>& UngroupedActivePlayers = UngroupedActivePlayerArrays[GetSyncGroupWriteIndex()];

	for (int32 GroupIndex = 0; GroupIndex < SyncGroups.Num(); ++GroupIndex)
	{
		FAnimGroupInstance& SyncGroup = SyncGroups[GroupIndex];
	
		if (SyncGroup.ActivePlayers.Num() > 0)
		{
			const FAnimGroupInstance* PreviousGroup = PreviousSyncGroups.IsValidIndex(GroupIndex) ? &PreviousSyncGroups[GroupIndex] : nullptr;
			SyncGroup.Prepare(PreviousGroup);

			UE_LOG(LogAnimMarkerSync, Log, TEXT("Ticking Group [%d] GroupLeader [%d]"), GroupIndex, SyncGroup.GroupLeaderIndex);

			const bool bOnlyOneAnimationInGroup = SyncGroup.ActivePlayers.Num() == 1;

			// Tick the group leader
			FAnimAssetTickContext TickContext(DeltaSeconds, RootMotionMode, bOnlyOneAnimationInGroup, SyncGroup.ValidMarkers);
			// initialize to invalidate first
			ensureMsgf(SyncGroup.GroupLeaderIndex == INDEX_NONE, TEXT("SyncGroup with GroupIndex=%d had a non -1 group leader index of %d in asset %s"), GroupIndex, SyncGroup.GroupLeaderIndex, *GetNameSafe(SkeletalMeshComponent));
			int32 GroupLeaderIndex = 0;
			for (; GroupLeaderIndex < SyncGroup.ActivePlayers.Num(); ++GroupLeaderIndex)
			{
				FAnimTickRecord& GroupLeader = SyncGroup.ActivePlayers[GroupLeaderIndex];
				// if it has leader score
				SCOPE_CYCLE_COUNTER(STAT_TickAssetPlayerInstance);
				FScopeCycleCounterUObject Scope(GroupLeader.SourceAsset);
				GroupLeader.SourceAsset->TickAssetPlayer(GroupLeader, NotifyQueue, TickContext);

				if (RootMotionMode == ERootMotionMode::RootMotionFromEverything && TickContext.RootMotionMovementParams.bHasRootMotion)
				{
					ExtractedRootMotion.AccumulateWithBlend(TickContext.RootMotionMovementParams, GroupLeader.GetRootMotionWeight());
				}

				// if we're not using marker based sync, we don't care, get out
				if (TickContext.CanUseMarkerPosition() == false)
				{
					SyncGroup.GroupLeaderIndex = GroupLeaderIndex;
					break;
				}
				// otherwise, the new position should contain the valid position for end, otherwise, we don't know where to sync to
				else if (TickContext.MarkerTickContext.IsMarkerSyncEndValid())
				{
					// if this leader contains correct position, break
					SyncGroup.MarkerTickContext = TickContext.MarkerTickContext;
					SyncGroup.GroupLeaderIndex = GroupLeaderIndex;
					UE_LOG(LogAnimMarkerSync, Log, TEXT("Previous Sync Group Marker Tick Context :\n%s"), *SyncGroup.MarkerTickContext.ToString());
					UE_LOG(LogAnimMarkerSync, Log, TEXT("New Sync Group Marker Tick Context :\n%s"), *TickContext.MarkerTickContext.ToString());
					break;
				}
				else
				{
					SyncGroup.GroupLeaderIndex = GroupLeaderIndex;
					UE_LOG(LogAnimMarkerSync, Log, TEXT("Invalid position from Leader %d. Trying next leader"), GroupLeaderIndex);
				}
			} 

			check(SyncGroup.GroupLeaderIndex != INDEX_NONE);
			// we found leader
			SyncGroup.Finalize(PreviousGroup);

			if (TickContext.CanUseMarkerPosition())
			{
				const FMarkerSyncAnimPosition& MarkerStart = TickContext.MarkerTickContext.GetMarkerSyncStartPosition();
				FName SyncGroupName = GetAnimClassInterface()->GetSyncGroupNames()[GroupIndex];
				FAnimTickRecord& GroupLeader = SyncGroup.ActivePlayers[SyncGroup.GroupLeaderIndex];
				FString LeaderAnimName = GroupLeader.SourceAsset->GetName();

				checkf(MarkerStart.PreviousMarkerName == NAME_None || SyncGroup.ValidMarkers.Contains(MarkerStart.PreviousMarkerName), TEXT("Prev Marker name not valid for sync group. Marker %s : SyncGroupName %s : Leader %s (Added to help debug Jira OR-9675)"), *MarkerStart.PreviousMarkerName.ToString(), *SyncGroupName.ToString(), *LeaderAnimName);
				checkf(MarkerStart.NextMarkerName == NAME_None || SyncGroup.ValidMarkers.Contains(MarkerStart.NextMarkerName), TEXT("Next Marker name not valid for sync group. Marker %s : SyncGroupName %s : Leader %s (Added to help debug Jira OR-9675)"), *MarkerStart.PreviousMarkerName.ToString(), *SyncGroupName.ToString(), *LeaderAnimName);
			}

			// Update everything else to follow the leader, if there is more followers
			if (SyncGroup.ActivePlayers.Num() > GroupLeaderIndex + 1)
			{
				// if we don't have a good leader, no reason to convert to follower
				// tick as leader
				TickContext.ConvertToFollower();
	
				for (int32 TickIndex = GroupLeaderIndex + 1; TickIndex < SyncGroup.ActivePlayers.Num(); ++TickIndex)
				{
					FAnimTickRecord& AssetPlayer = SyncGroup.ActivePlayers[TickIndex];
					{
						SCOPE_CYCLE_COUNTER(STAT_TickAssetPlayerInstance);
						FScopeCycleCounterUObject Scope(AssetPlayer.SourceAsset);
						TickContext.RootMotionMovementParams.Clear();
						AssetPlayer.SourceAsset->TickAssetPlayer(AssetPlayer, NotifyQueue, TickContext);
					}
					if (RootMotionMode == ERootMotionMode::RootMotionFromEverything && TickContext.RootMotionMovementParams.bHasRootMotion)
					{
						ExtractedRootMotion.AccumulateWithBlend(TickContext.RootMotionMovementParams, AssetPlayer.GetRootMotionWeight());
					}
				}
			}
		}
	}

	// Handle the remaining ungrouped animation players
	for (int32 TickIndex = 0; TickIndex < UngroupedActivePlayers.Num(); ++TickIndex)
	{
		FAnimTickRecord& AssetPlayerToTick = UngroupedActivePlayers[TickIndex];
		static TArray<FName> TempNames;
		const TArray<FName>* UniqueNames = AssetPlayerToTick.SourceAsset->GetUniqueMarkerNames();
		const TArray<FName>& ValidMarkers = UniqueNames ? *UniqueNames : TempNames;

		const bool bOnlyOneAnimationInGroup = true;
		FAnimAssetTickContext TickContext(DeltaSeconds, RootMotionMode, bOnlyOneAnimationInGroup, ValidMarkers);
		{
			SCOPE_CYCLE_COUNTER(STAT_TickAssetPlayerInstance);
			FScopeCycleCounterUObject Scope(AssetPlayerToTick.SourceAsset);
			AssetPlayerToTick.SourceAsset->TickAssetPlayer(AssetPlayerToTick, NotifyQueue, TickContext);
		}
		if (RootMotionMode == ERootMotionMode::RootMotionFromEverything && TickContext.RootMotionMovementParams.bHasRootMotion)
		{
			ExtractedRootMotion.AccumulateWithBlend(TickContext.RootMotionMovementParams, AssetPlayerToTick.GetRootMotionWeight());
		}
	}
}

void FAnimInstanceProxy::AddAnimNotifies(const TArray<const FAnimNotifyEvent*>& NewNotifies, const float InstanceWeight)
{
	NotifyQueue.AddAnimNotifies(NewNotifies, InstanceWeight);
}

int32 FAnimInstanceProxy::GetSyncGroupIndexFromName(FName SyncGroupName) const
{
	if (AnimClassInterface)
	{
		return AnimClassInterface->GetSyncGroupIndex(SyncGroupName);
	}
	return INDEX_NONE;
}

bool FAnimInstanceProxy::GetTimeToClosestMarker(FName SyncGroup, FName MarkerName, float& OutMarkerTime) const
{
	const int32 SyncGroupIndex = GetSyncGroupIndexFromName(SyncGroup);
	const TArray<FAnimGroupInstance>& SyncGroups = SyncGroupArrays[GetSyncGroupReadIndex()];

	if (SyncGroups.IsValidIndex(SyncGroupIndex))
	{
		const FAnimGroupInstance& SyncGroupInstance = SyncGroups[SyncGroupIndex];
		if (SyncGroupInstance.bCanUseMarkerSync && SyncGroupInstance.ActivePlayers.IsValidIndex(SyncGroupInstance.GroupLeaderIndex))
		{
			const FMarkerSyncAnimPosition& EndPosition = SyncGroupInstance.MarkerTickContext.GetMarkerSyncEndPosition();
			const FAnimTickRecord& Leader = SyncGroupInstance.ActivePlayers[SyncGroupInstance.GroupLeaderIndex];
			if (EndPosition.PreviousMarkerName == MarkerName)
			{
				OutMarkerTime = Leader.MarkerTickRecord->PreviousMarker.TimeToMarker;
				return true;
			}
			else if (EndPosition.NextMarkerName == MarkerName)
			{
				OutMarkerTime = Leader.MarkerTickRecord->NextMarker.TimeToMarker;
				return true;
			}
		}
	}
	return false;
}

void FAnimInstanceProxy::AddAnimNotifyFromGeneratedClass(int32 NotifyIndex)
{
	if(NotifyIndex==INDEX_NONE)
	{
		return;
	}

	if (AnimClassInterface)
	{
		check(AnimClassInterface->GetAnimNotifies().IsValidIndex(NotifyIndex));
		const FAnimNotifyEvent* Notify = &AnimClassInterface->GetAnimNotifies()[NotifyIndex];
		NotifyQueue.AnimNotifies.Add(Notify);
	}
}

bool FAnimInstanceProxy::HasMarkerBeenHitThisFrame(FName SyncGroup, FName MarkerName) const
{
	const int32 SyncGroupIndex = GetSyncGroupIndexFromName(SyncGroup);
	const TArray<FAnimGroupInstance>& SyncGroups = SyncGroupArrays[GetSyncGroupReadIndex()];

	if (SyncGroups.IsValidIndex(SyncGroupIndex))
	{
		const FAnimGroupInstance& SyncGroupInstance = SyncGroups[SyncGroupIndex];
		if (SyncGroupInstance.bCanUseMarkerSync)
		{
			return SyncGroupInstance.MarkerTickContext.MarkersPassedThisTick.ContainsByPredicate([&MarkerName](const FPassedMarker& PassedMarker) -> bool
			{
				return PassedMarker.PassedMarkerName == MarkerName;
			});
		}
	}
	return false;
}

bool FAnimInstanceProxy::IsSyncGroupBetweenMarkers(FName InSyncGroupName, FName PreviousMarker, FName NextMarker, bool bRespectMarkerOrder) const
{
	const FMarkerSyncAnimPosition& SyncGroupPosition = GetSyncGroupPosition(InSyncGroupName);
	if ((SyncGroupPosition.PreviousMarkerName == PreviousMarker) && (SyncGroupPosition.NextMarkerName == NextMarker))
	{
		return true;
	}

	if (!bRespectMarkerOrder)
	{
		return ((SyncGroupPosition.PreviousMarkerName == NextMarker) && (SyncGroupPosition.NextMarkerName == PreviousMarker));
	}

	return false;
}

FMarkerSyncAnimPosition FAnimInstanceProxy::GetSyncGroupPosition(FName InSyncGroupName) const
{
	const int32 SyncGroupIndex = GetSyncGroupIndexFromName(InSyncGroupName);
	const TArray<FAnimGroupInstance>& SyncGroups = SyncGroupArrays[GetSyncGroupReadIndex()];

	if (SyncGroups.IsValidIndex(SyncGroupIndex))
	{
		const FAnimGroupInstance& SyncGroupInstance = SyncGroups[SyncGroupIndex];
		if (SyncGroupInstance.bCanUseMarkerSync && SyncGroupInstance.MarkerTickContext.IsMarkerSyncEndValid())
		{
			return SyncGroupInstance.MarkerTickContext.GetMarkerSyncEndPosition();
		}
	}

	return FMarkerSyncAnimPosition();
}

void FAnimInstanceProxy::ReinitializeSlotNodes()
{
	SlotNameToTrackerIndex.Reset();
	SlotWeightTracker[0].Reset();
	SlotWeightTracker[1].Reset();
	
	// Increment counter
	SlotNodeInitializationCounter.Increment();
}

void FAnimInstanceProxy::RegisterSlotNodeWithAnimInstance(const FName& SlotNodeName)
{
	// verify if same slot node name exists
	// then warn users, this is invalid
	if (SlotNameToTrackerIndex.Contains(SlotNodeName))
	{
		UClass* ActualAnimClass = IAnimClassInterface::GetActualAnimClass(GetAnimClassInterface());
		FString ClassNameString = ActualAnimClass ? ActualAnimClass->GetName() : FString("Unavailable");
		if (IsInGameThread())
		{
			// message log access means we need to run this in the game thread
		FMessageLog("AnimBlueprint").Warning(FText::Format(LOCTEXT("AnimInstance_SlotNode", "SLOTNODE: '{0}' in animation instance class {1} already exists. Remove duplicates from the animation graph for this class."), FText::FromString(SlotNodeName.ToString()), FText::FromString(ClassNameString)));
		}
		else
		{
			UE_LOG(LogAnimation, Warning, TEXT("SLOTNODE: '%s' in animation instance class %s already exists. Remove duplicates from the animation graph for this class."), *SlotNodeName.ToString(), *ClassNameString);
		}
		return;
	}

	int32 SlotIndex = SlotWeightTracker[0].Num();

	SlotNameToTrackerIndex.Add(SlotNodeName, SlotIndex);
	SlotWeightTracker[0].Add(FMontageActiveSlotTracker());
	SlotWeightTracker[1].Add(FMontageActiveSlotTracker());
}

void FAnimInstanceProxy::UpdateSlotNodeWeight(const FName& SlotNodeName, float InMontageLocalWeight, float InNodeGlobalWeight)
{
	const int32* TrackerIndexPtr = SlotNameToTrackerIndex.Find(SlotNodeName);
	if (TrackerIndexPtr)
	{
		FMontageActiveSlotTracker& Tracker = SlotWeightTracker[GetSyncGroupWriteIndex()][*TrackerIndexPtr];
		Tracker.MontageLocalWeight = InMontageLocalWeight;
		Tracker.NodeGlobalWeight = InNodeGlobalWeight;

		// Count as relevant if we are weighted in
		Tracker.bIsRelevantThisTick = Tracker.bIsRelevantThisTick || FAnimWeight::IsRelevant(InMontageLocalWeight);
	}
}

void FAnimInstanceProxy::ClearSlotNodeWeights()
{
	TArray<FMontageActiveSlotTracker>& SlotWeightTracker_Read = SlotWeightTracker[GetSyncGroupReadIndex()];
	TArray<FMontageActiveSlotTracker>& SlotWeightTracker_Write = SlotWeightTracker[GetSyncGroupWriteIndex()];

	for (int32 TrackerIndex = 0; TrackerIndex < SlotWeightTracker_Write.Num(); TrackerIndex++)
	{
		SlotWeightTracker_Write[TrackerIndex] = FMontageActiveSlotTracker();
		SlotWeightTracker_Write[TrackerIndex].bWasRelevantOnPreviousTick = SlotWeightTracker_Read[TrackerIndex].bIsRelevantThisTick;
	}
}

bool FAnimInstanceProxy::IsSlotNodeRelevantForNotifies(const FName& SlotNodeName) const
{
	const int32* TrackerIndexPtr = SlotNameToTrackerIndex.Find(SlotNodeName);
	if (TrackerIndexPtr)
	{
		const FMontageActiveSlotTracker& Tracker = SlotWeightTracker[GetSyncGroupReadIndex()][*TrackerIndexPtr];
		return (Tracker.bIsRelevantThisTick || Tracker.bWasRelevantOnPreviousTick);
	}

	return false;
}

float FAnimInstanceProxy::GetSlotNodeGlobalWeight(const FName& SlotNodeName) const
{
	const int32* TrackerIndexPtr = SlotNameToTrackerIndex.Find(SlotNodeName);
	if (TrackerIndexPtr)
	{
		const FMontageActiveSlotTracker& Tracker = SlotWeightTracker[GetSyncGroupReadIndex()][*TrackerIndexPtr];
		return Tracker.NodeGlobalWeight;
	}

	return 0.f;
}

float FAnimInstanceProxy::GetSlotMontageGlobalWeight(const FName& SlotNodeName) const
{
	const int32* TrackerIndexPtr = SlotNameToTrackerIndex.Find(SlotNodeName);
	if (TrackerIndexPtr)
	{
		const FMontageActiveSlotTracker& Tracker = SlotWeightTracker[GetSyncGroupReadIndex()][*TrackerIndexPtr];
		return Tracker.MontageLocalWeight * Tracker.NodeGlobalWeight;
	}

	return 0.f;
}

float FAnimInstanceProxy::GetSlotMontageLocalWeight(const FName& SlotNodeName) const
{
	const int32* TrackerIndexPtr = SlotNameToTrackerIndex.Find(SlotNodeName);
	if (TrackerIndexPtr)
	{
		const FMontageActiveSlotTracker& Tracker = SlotWeightTracker[GetSyncGroupReadIndex()][*TrackerIndexPtr];
		return Tracker.MontageLocalWeight;
	}

	return 0.f;
}

float FAnimInstanceProxy::CalcSlotMontageLocalWeight(const FName& SlotNodeName) const
{
	float out_SlotNodeLocalWeight, out_SourceWeight, out_TotalNodeWeight;
	GetSlotWeight(SlotNodeName, out_SlotNodeLocalWeight, out_SourceWeight, out_TotalNodeWeight);

	return out_SlotNodeLocalWeight;
}

FAnimNode_Base* FAnimInstanceProxy::GetCheckedNodeFromIndexUntyped(int32 NodeIdx, UScriptStruct* RequiredStructType)
{
	FAnimNode_Base* NodePtr = nullptr;
	if (AnimClassInterface)
	{
		const TArray<UStructProperty*>& AnimNodeProperties = AnimClassInterface->GetAnimNodeProperties();
		const int32 InstanceIdx = AnimNodeProperties.Num() - 1 - NodeIdx;

		if (AnimNodeProperties.IsValidIndex(InstanceIdx))
		{
			UStructProperty* NodeProperty = AnimNodeProperties[InstanceIdx];

			if (NodeProperty->Struct->IsChildOf(RequiredStructType))
			{
				NodePtr = NodeProperty->ContainerPtrToValuePtr<FAnimNode_Base>(AnimInstanceObject);
			}
			else
			{
				checkfSlow(false, TEXT("Requested a node of type %s but found node of type %s"), *RequiredStructType->GetName(), *NodeProperty->Struct->GetName());
			}
		}
		else
		{
			checkfSlow(false, TEXT("Requested node of type %s at index %d/%d, index out of bounds."), *RequiredStructType->GetName(), NodeIdx, InstanceIdx);
		}
	}

	checkfSlow(NodePtr, TEXT("Requested node at index %d not found!"), NodeIdx);

	return NodePtr;
}

FAnimNode_Base* FAnimInstanceProxy::GetNodeFromIndexUntyped(int32 NodeIdx, UScriptStruct* RequiredStructType)
{
	FAnimNode_Base* NodePtr = nullptr;
	if (AnimClassInterface)
	{
		const TArray<UStructProperty*>& AnimNodeProperties = AnimClassInterface->GetAnimNodeProperties();
		const int32 InstanceIdx = AnimNodeProperties.Num() - 1 - NodeIdx;

		if (AnimNodeProperties.IsValidIndex(InstanceIdx))
		{
			UStructProperty* NodeProperty = AnimNodeProperties[InstanceIdx];

			if (NodeProperty->Struct->IsChildOf(RequiredStructType))
			{
				NodePtr = NodeProperty->ContainerPtrToValuePtr<FAnimNode_Base>(AnimInstanceObject);
			}
		}
	}

	return NodePtr;
}

void FAnimInstanceProxy::RecalcRequiredBones(USkeletalMeshComponent* Component, UObject* Asset)
{
	RequiredBones.InitializeTo(Component->RequiredBones, FCurveEvaluationOption(Component->GetAllowedAnimCurveEvaluate(), &Component->GetDisallowedAnimCurvesEvaluation(), Component->PredictedLODLevel), *Asset);

	// If there is a ref pose override, we want to replace ref pose in RequiredBones
	const FSkelMeshRefPoseOverride* RefPoseOverride = Component->GetRefPoseOverride();
	if (RefPoseOverride)
	{
		// Get ref pose override info
		// Get indices of required bones
		const TArray<FBoneIndexType>& BoneIndicesArray = RequiredBones.GetBoneIndicesArray();
		// Get number of required bones
		int32 NumReqBones = BoneIndicesArray.Num();

		// Build new array of ref pose transforms for required bones
		TArray<FTransform> NewCompactRefPose;
		NewCompactRefPose.AddUninitialized(NumReqBones);

		for (int32 CompactBoneIndex = 0; CompactBoneIndex < NumReqBones; ++CompactBoneIndex)
		{
			FBoneIndexType MeshPoseIndex = BoneIndicesArray[CompactBoneIndex];

			if (RefPoseOverride->RefBonePoses.IsValidIndex(MeshPoseIndex))
			{
				NewCompactRefPose[CompactBoneIndex] = RefPoseOverride->RefBonePoses[MeshPoseIndex];
			}
			else
			{
				NewCompactRefPose[CompactBoneIndex] = FTransform::Identity;
			}
		}

		// Update ref pose in required bones structure
		RequiredBones.SetRefPoseCompactArray(NewCompactRefPose);
	}

	// If this instance can accept input poses, initialise the input pose container
	if(SubInstanceInputNode)
	{
		SubInstanceInputNode->InputPose.SetBoneContainer(&RequiredBones);
	}

	// When RequiredBones mapping has changed, AnimNodes need to update their bones caches. 
	bBoneCachesInvalidated = true;
}

void FAnimInstanceProxy::RecalcRequiredCurves(const FCurveEvaluationOption& CurveEvalOption)
{
	RequiredBones.CacheRequiredAnimCurveUids(CurveEvalOption);
}

void FAnimInstanceProxy::UpdateAnimation()
{
	ANIM_MT_SCOPE_CYCLE_COUNTER(ProxyUpdateAnimation, !IsInGameThread());
	FScopeCycleCounterUObject AnimScope(GetAnimInstanceObject());

	// update native update
	{
		SCOPE_CYCLE_COUNTER(STAT_NativeUpdateAnimation);
		Update(CurrentDeltaSeconds);
	}

	// update all nodes
	UpdateAnimationNode(CurrentDeltaSeconds);

	// tick all our active asset players
	TickAssetPlayerInstances(CurrentDeltaSeconds);
}

void FAnimInstanceProxy::PreEvaluateAnimation(UAnimInstance* InAnimInstance)
{
	InitializeObjects(InAnimInstance);
}

void FAnimInstanceProxy::EvaluateAnimation(FPoseContext& Output)
{
	ANIM_MT_SCOPE_CYCLE_COUNTER(EvaluateAnimInstance, !IsInGameThread());

	CacheBones();

	// Evaluate native code if implemented, otherwise evaluate the node graph
	if (!Evaluate(Output))
	{
		EvaluateAnimationNode(Output);
	}
}

void FAnimInstanceProxy::CacheBones()
{
	// If bone caches have been invalidated, have AnimNodes refresh those.
	if (bBoneCachesInvalidated && RootNode)
	{
		bBoneCachesInvalidated = false;

		CachedBonesCounter.Increment();
		FAnimationCacheBonesContext Proxy(this);
		RootNode->CacheBones_AnyThread(Proxy);
	}
}

void FAnimInstanceProxy::EvaluateAnimationNode(FPoseContext& Output)
{
	if (RootNode != NULL)
	{
		ANIM_MT_SCOPE_CYCLE_COUNTER(EvaluateAnimGraph, !IsInGameThread());
		EvaluationCounter.Increment();
		RootNode->Evaluate_AnyThread(Output);
	}
	else
	{
		Output.ResetToRefPose();
	}
}

// for now disable because it will not work with single node instance
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
#define DEBUG_MONTAGEINSTANCE_WEIGHT 0
#else
#define DEBUG_MONTAGEINSTANCE_WEIGHT 1
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

void FAnimInstanceProxy::SlotEvaluatePose(const FName& SlotNodeName, const FCompactPose& SourcePose, const FBlendedCurve& SourceCurve, float InSourceWeight, FCompactPose& BlendedPose, FBlendedCurve& BlendedCurve, float InBlendWeight, float InTotalNodeWeight)
{
	//Accessing MontageInstances from this function is not safe (as this can be called during Parallel Anim Evaluation!
	//Any montage data you need to add should be part of MontageEvaluationData
	// nothing to blend, just get it out
	if (InBlendWeight <= ZERO_ANIMWEIGHT_THRESH)
	{
		BlendedPose = SourcePose;
		BlendedCurve = SourceCurve;
		return;
	}

	// Split our data into additive and non additive.
	TArray<FSlotEvaluationPose> AdditivePoses;
	TArray<FSlotEvaluationPose> NonAdditivePoses;

	// first pass we go through collect weights and valid montages. 
#if DEBUG_MONTAGEINSTANCE_WEIGHT
	float TotalWeight = 0.f;
#endif // DEBUG_MONTAGEINSTANCE_WEIGHT

	for (const FMontageEvaluationState& EvalState : MontageEvaluationData)
	{
		// If MontageEvaluationData is not valid anymore, pass-through AnimSlot.
		// This can happen if InitAnim pushes a RefreshBoneTransforms when not rendered,
		// with EMeshComponentUpdateFlag::OnlyTickMontagesWhenNotRendered set.
		if (!EvalState.Montage.IsValid())
		{
			BlendedPose = SourcePose;
			BlendedCurve = SourceCurve;
			return;
		}

		const UAnimMontage* const Montage = EvalState.Montage.Get();
		if (Montage->IsValidSlot(SlotNodeName))
		{
			FAnimTrack const* const AnimTrack = Montage->GetAnimationData(SlotNodeName);

			// Find out additive type for pose.
			EAdditiveAnimationType const AdditiveAnimType = AnimTrack->IsAdditive() 
				? (AnimTrack->IsRotationOffsetAdditive() ? AAT_RotationOffsetMeshSpace : AAT_LocalSpaceBase)
				: AAT_None;

			FSlotEvaluationPose NewPose(EvalState.MontageWeight, AdditiveAnimType);
			
			// Bone array has to be allocated prior to calling GetPoseFromAnimTrack
			NewPose.Pose.SetBoneContainer(&RequiredBones);
			NewPose.Curve.InitFrom(RequiredBones);

			// Extract pose from Track
			FAnimExtractContext ExtractionContext(EvalState.MontagePosition, Montage->HasRootMotion() && RootMotionMode != ERootMotionMode::NoRootMotionExtraction);
			AnimTrack->GetAnimationPose(NewPose.Pose, NewPose.Curve, ExtractionContext);

			// add montage curves 
			FBlendedCurve MontageCurve;
			MontageCurve.InitFrom(RequiredBones);
			Montage->EvaluateCurveData(MontageCurve, EvalState.MontagePosition);
			NewPose.Curve.Combine(MontageCurve);

#if DEBUG_MONTAGEINSTANCE_WEIGHT
			TotalWeight += EvalState.MontageWeight;
#endif // DEBUG_MONTAGEINSTANCE_WEIGHT
			if (AdditiveAnimType == AAT_None)
			{
				NonAdditivePoses.Add(NewPose);
			}
			else
			{
				AdditivePoses.Add(NewPose);
			}
		}
	}

	// allocate for blending
	// If source has any weight, add it to the blend array.
	float const SourceWeight = FMath::Clamp<float>(InSourceWeight, 0.f, 1.f);

#if DEBUG_MONTAGEINSTANCE_WEIGHT
	ensure (FMath::IsNearlyEqual(InTotalNodeWeight, TotalWeight, KINDA_SMALL_NUMBER));
#endif // DEBUG_MONTAGEINSTANCE_WEIGHT
	ensure (InTotalNodeWeight > ZERO_ANIMWEIGHT_THRESH);

	if (InTotalNodeWeight > (1.f + ZERO_ANIMWEIGHT_THRESH))
	{
		// Re-normalize additive poses
		for (int32 Index = 0; Index < AdditivePoses.Num(); Index++)
		{
			AdditivePoses[Index].Weight /= InTotalNodeWeight;
		}
		// Re-normalize non-additive poses
		for (int32 Index = 0; Index < NonAdditivePoses.Num(); Index++)
		{
			NonAdditivePoses[Index].Weight /= InTotalNodeWeight;
		}
	}

	// Make sure we have at least one montage here.
	check((AdditivePoses.Num() > 0) || (NonAdditivePoses.Num() > 0));

	// Second pass, blend non additive poses together
	{
		// If we're only playing additive animations, just copy source for base pose.
		if (NonAdditivePoses.Num() == 0)
		{
			BlendedPose = SourcePose;
			BlendedCurve = SourceCurve;
		}
		// Otherwise we need to blend non additive poses together
		else
		{
			int32 const NumPoses = NonAdditivePoses.Num() + ((SourceWeight > ZERO_ANIMWEIGHT_THRESH) ? 1 : 0);

			TArray<const FCompactPose*, TInlineAllocator<8>> BlendingPoses;
			BlendingPoses.AddUninitialized(NumPoses);

			TArray<float, TInlineAllocator<8>> BlendWeights;
			BlendWeights.AddUninitialized(NumPoses);

			TArray<const FBlendedCurve*, TInlineAllocator<8>> BlendingCurves;
			BlendingCurves.AddUninitialized(NumPoses);

			for (int32 Index = 0; Index < NonAdditivePoses.Num(); Index++)
			{
				BlendingPoses[Index] = &NonAdditivePoses[Index].Pose;
				BlendingCurves[Index] = &NonAdditivePoses[Index].Curve;
				BlendWeights[Index] = NonAdditivePoses[Index].Weight;
			}

			if (SourceWeight > ZERO_ANIMWEIGHT_THRESH)
			{
				int32 const SourceIndex = BlendWeights.Num() - 1;
				BlendingPoses[SourceIndex] = &SourcePose;
				BlendingCurves[SourceIndex] = &SourceCurve;
				BlendWeights[SourceIndex] = SourceWeight;
			}

			// now time to blend all montages
			FAnimationRuntime::BlendPosesTogetherIndirect(BlendingPoses, BlendingCurves, BlendWeights, BlendedPose, BlendedCurve);
		}
	}

	// Third pass, layer on weighted additive poses.
	if (AdditivePoses.Num() > 0)
	{
		for (int32 Index = 0; Index < AdditivePoses.Num(); Index++)
		{
			FSlotEvaluationPose const& AdditivePose = AdditivePoses[Index];
			FAnimationRuntime::AccumulateAdditivePose(BlendedPose, AdditivePose.Pose, BlendedCurve, AdditivePose.Curve, AdditivePose.Weight, AdditivePose.AdditiveType);
		}
	}

	// Normalize rotations after blending/accumulation
	BlendedPose.NormalizeRotations();
}

//to debug montage weight
#define DEBUGMONTAGEWEIGHT 0

void FAnimInstanceProxy::GetSlotWeight(const FName& SlotNodeName, float& out_SlotNodeWeight, float& out_SourceWeight, float& out_TotalNodeWeight) const
{
	// node total weight 
	float NewSlotNodeWeight = 0.f;
	// this is required to track, because it will be 1-SourceWeight
	// if additive, it can be applied more
	float NonAdditiveTotalWeight = 0.f;

#if DEBUGMONTAGEWEIGHT
	float TotalDesiredWeight = 0.f;
#endif
	// first get all the montage instance weight this slot node has
	for (const FMontageEvaluationState& EvalState : MontageEvaluationData)
	{
		if (EvalState.Montage.IsValid())
		{
			const UAnimMontage* const Montage = EvalState.Montage.Get();
			if (Montage->IsValidSlot(SlotNodeName))
			{
				NewSlotNodeWeight += EvalState.MontageWeight;
				if (!Montage->IsValidAdditiveSlot(SlotNodeName))
				{
					NonAdditiveTotalWeight += EvalState.MontageWeight;
				}

#if DEBUGMONTAGEWEIGHT			
				TotalDesiredWeight += EvalState->DesiredWeight;
#endif
#if !NO_LOGGING
			UE_LOG(LogAnimation, Verbose, TEXT("GetSlotWeight : Owner: %s, AnimMontage: %s,  (DesiredWeight:%0.2f, Weight:%0.2f)"),
						*GetActorName(), *EvalState.Montage->GetName(), EvalState.DesiredWeight, EvalState.MontageWeight);
#endif
			}
		}
	}

	// save the total node weight, it can be more than 1
	// we need this so that when we eval, we normalized by this weight
	// calculating there can cause inconsistency if some data changes
	out_TotalNodeWeight = NewSlotNodeWeight;

	// this can happen when it's blending in OR when newer animation comes in with shorter blendtime
	// say #1 animation was blending out time with current blendtime 1.0 #2 animation was blending in with 1.0 (old) but got blend out with new blendtime 0.2f
	// #3 animation was blending in with the new blendtime 0.2f, you'll have sum of #1, 2, 3 exceeds 1.f
	if (NewSlotNodeWeight > 1.f)
	{
		// you don't want to change weight of montage instance since it can play multiple slots
		// if you change one, it will apply to all slots in that montage
		// instead we should renormalize when we eval
		// this should happen in the eval phase
		NonAdditiveTotalWeight /= NewSlotNodeWeight;
		// since we normalized, we reset
		NewSlotNodeWeight = 1.f;
	}
#if DEBUGMONTAGEWEIGHT
	else if (TotalDesiredWeight == 1.f && TotalSum < 1.f - ZERO_ANIMWEIGHT_THRESH)
	{
		// this can happen when it's blending in OR when newer animation comes in with longer blendtime
		// say #1 animation was blending out time with current blendtime 0.2 #2 animation was blending in with 0.2 (old) but got blend out with new blendtime 1.f
		// #3 animation was blending in with the new blendtime 1.f, you'll have sum of #1, 2, 3 doesn't meet 1.f
		UE_LOG(LogAnimation, Warning, TEXT("[%s] Montage has less weight. Blending in?(%f)"), *SlotNodeName.ToString(), TotalSum);
	}
#endif

	out_SlotNodeWeight = NewSlotNodeWeight;
	out_SourceWeight = 1.f - NonAdditiveTotalWeight;
}

const FMontageEvaluationState* FAnimInstanceProxy::GetActiveMontageEvaluationState() const
{
	// Start from end, as most recent instances are added at the end of the queue.
	int32 const NumInstances = MontageEvaluationData.Num();
	for (int32 InstanceIndex = NumInstances - 1; InstanceIndex >= 0; InstanceIndex--)
	{
		const FMontageEvaluationState& EvaluationData = MontageEvaluationData[InstanceIndex];
		if (EvaluationData.bIsActive)
		{
			return &EvaluationData;
		}
	}

	return nullptr;
}

void FAnimInstanceProxy::GatherDebugData(FNodeDebugData& DebugData)
{
	// Gather debug data for Root Node
	if(RootNode != nullptr)
	{
		 RootNode->GatherDebugData(DebugData); 
	}

	// Gather debug data for Cached Poses.
	for (FAnimNode_SaveCachedPose* PoseNode : SavedPoseQueue)
	{
		PoseNode->GatherDebugData(DebugData);
	}
}

#if ENABLE_ANIM_DRAW_DEBUG

void FAnimInstanceProxy::AnimDrawDebugOnScreenMessage(const FString& DebugMessage, const FColor& Color, const FVector2D& TextScale)
{
	FQueuedDrawDebugItem DrawDebugItem;

	DrawDebugItem.ItemType = EDrawDebugItemType::OnScreenMessage;
	DrawDebugItem.Message = DebugMessage;
	DrawDebugItem.Color = Color;
	DrawDebugItem.TextScale = TextScale;

	QueuedDrawDebugItems.Add(DrawDebugItem);
}

void FAnimInstanceProxy::AnimDrawDebugDirectionalArrow(const FVector& LineStart, const FVector& LineEnd, float ArrowSize, const FColor& Color, bool bPersistentLines, float LifeTime, float Thickness)
{
	FQueuedDrawDebugItem DrawDebugItem;

	DrawDebugItem.ItemType = EDrawDebugItemType::DirectionalArrow;
	DrawDebugItem.StartLoc = LineStart;
	DrawDebugItem.EndLoc = LineEnd;
	DrawDebugItem.Size = ArrowSize;
	DrawDebugItem.Color = Color;
	DrawDebugItem.bPersistentLines = bPersistentLines;
	DrawDebugItem.LifeTime = LifeTime;
	DrawDebugItem.Thickness = Thickness;

	QueuedDrawDebugItems.Add(DrawDebugItem);
}

void FAnimInstanceProxy::AnimDrawDebugSphere(const FVector& Center, float Radius, int32 Segments, const FColor& Color, bool bPersistentLines, float LifeTime, float Thickness)
{
	FQueuedDrawDebugItem DrawDebugItem;

	DrawDebugItem.ItemType = EDrawDebugItemType::Sphere;
	DrawDebugItem.Center = Center;
	DrawDebugItem.Radius = Radius;
	DrawDebugItem.Segments = Segments;
	DrawDebugItem.Color = Color;
	DrawDebugItem.bPersistentLines = bPersistentLines;
	DrawDebugItem.LifeTime = LifeTime;
	DrawDebugItem.Thickness = Thickness;

	QueuedDrawDebugItems.Add(DrawDebugItem);
}

void FAnimInstanceProxy::AnimDrawDebugCoordinateSystem(FVector const& AxisLoc, FRotator const& AxisRot, float Scale, bool bPersistentLines, float LifeTime, float Thickness)
{
	FQueuedDrawDebugItem DrawDebugItem;

	DrawDebugItem.ItemType = EDrawDebugItemType::CoordinateSystem;
	DrawDebugItem.StartLoc = AxisLoc;
	DrawDebugItem.Rotation = AxisRot;
	DrawDebugItem.Size = Scale;
	DrawDebugItem.bPersistentLines = bPersistentLines;
	DrawDebugItem.LifeTime = LifeTime;
	DrawDebugItem.Thickness = Thickness;

	QueuedDrawDebugItems.Add(DrawDebugItem);
}

void FAnimInstanceProxy::AnimDrawDebugLine(const FVector& StartLoc, const FVector& EndLoc, const FColor& Color, bool bPersistentLines, float LifeTime, float Thickness)
{
	FQueuedDrawDebugItem DrawDebugItem;

	DrawDebugItem.ItemType = EDrawDebugItemType::Line;
	DrawDebugItem.StartLoc = StartLoc;
	DrawDebugItem.EndLoc = EndLoc;
	DrawDebugItem.Color = Color;
	DrawDebugItem.bPersistentLines = bPersistentLines;
	DrawDebugItem.LifeTime = LifeTime;
	DrawDebugItem.Thickness = Thickness;

	QueuedDrawDebugItems.Add(DrawDebugItem);
}

#endif // ENABLE_ANIM_DRAW_DEBUG

float FAnimInstanceProxy::GetInstanceAssetPlayerLength(int32 AssetPlayerIndex)
{
	if(FAnimNode_AssetPlayerBase* PlayerNode = GetNodeFromIndex<FAnimNode_AssetPlayerBase>(AssetPlayerIndex))
	{
		return PlayerNode->GetCurrentAssetLength();
	}

	return 0.0f;
}

float FAnimInstanceProxy::GetInstanceAssetPlayerTime(int32 AssetPlayerIndex)
{
	if(FAnimNode_AssetPlayerBase* PlayerNode = GetNodeFromIndex<FAnimNode_AssetPlayerBase>(AssetPlayerIndex))
	{
		return PlayerNode->GetCurrentAssetTimePlayRateAdjusted();
	}

	return 0.0f;
}

float FAnimInstanceProxy::GetInstanceAssetPlayerTimeFraction(int32 AssetPlayerIndex)
{
	if(FAnimNode_AssetPlayerBase* PlayerNode = GetNodeFromIndex<FAnimNode_AssetPlayerBase>(AssetPlayerIndex))
	{
		float Length = PlayerNode->GetCurrentAssetLength();

		if(Length > 0.0f)
		{
			return PlayerNode->GetCurrentAssetTimePlayRateAdjusted() / Length;
		}
	}

	return 0.0f;
}

float FAnimInstanceProxy::GetInstanceAssetPlayerTimeFromEndFraction(int32 AssetPlayerIndex)
{
	if(FAnimNode_AssetPlayerBase* PlayerNode = GetNodeFromIndex<FAnimNode_AssetPlayerBase>(AssetPlayerIndex))
	{
		float Length = PlayerNode->GetCurrentAssetLength();

		if(Length > 0.f)
		{
			return (Length - PlayerNode->GetCurrentAssetTimePlayRateAdjusted()) / Length;
		}
	}

	return 1.0f;
}

float FAnimInstanceProxy::GetInstanceAssetPlayerTimeFromEnd(int32 AssetPlayerIndex)
{
	if(FAnimNode_AssetPlayerBase* PlayerNode = GetNodeFromIndex<FAnimNode_AssetPlayerBase>(AssetPlayerIndex))
	{
		return PlayerNode->GetCurrentAssetLength() - PlayerNode->GetCurrentAssetTimePlayRateAdjusted();
	}

	return MAX_flt;
}

float FAnimInstanceProxy::GetInstanceMachineWeight(int32 MachineIndex)
{
	if (FAnimNode_StateMachine* MachineInstance = GetStateMachineInstance(MachineIndex))
	{
		return GetRecordedMachineWeight(MachineInstance->StateMachineIndexInClass);
	}

	return 0.0f;
}

float FAnimInstanceProxy::GetInstanceStateWeight(int32 MachineIndex, int32 StateIndex)
{
	if(FAnimNode_StateMachine* MachineInstance = GetStateMachineInstance(MachineIndex))
	{
		return GetRecordedStateWeight(MachineInstance->StateMachineIndexInClass, StateIndex);
	}
	
	return 0.0f;
}

float FAnimInstanceProxy::GetInstanceCurrentStateElapsedTime(int32 MachineIndex)
{
	if(FAnimNode_StateMachine* MachineInstance = GetStateMachineInstance(MachineIndex))
	{
		return MachineInstance->GetCurrentStateElapsedTime();
	}

	return 0.0f;
}

float FAnimInstanceProxy::GetInstanceTransitionCrossfadeDuration(int32 MachineIndex, int32 TransitionIndex)
{
	if(FAnimNode_StateMachine* MachineInstance = GetStateMachineInstance(MachineIndex))
	{
		if(MachineInstance->IsValidTransitionIndex(TransitionIndex))
		{
			return MachineInstance->GetTransitionInfo(TransitionIndex).CrossfadeDuration;
		}
	}

	return 0.0f;
}

float FAnimInstanceProxy::GetInstanceTransitionTimeElapsed(int32 MachineIndex, int32 TransitionIndex)
{
	// Just an alias for readability in the anim graph
	if(FAnimNode_StateMachine* MachineInstance = GetStateMachineInstance(MachineIndex))
	{
		if(MachineInstance->IsValidTransitionIndex(TransitionIndex))
		{
			for(FAnimationActiveTransitionEntry& ActiveTransition : MachineInstance->ActiveTransitionArray)
			{
				if(ActiveTransition.SourceTransitionIndices.Contains(TransitionIndex))
				{
					return ActiveTransition.ElapsedTime;
				}
			}
		}
	}

	return 0.0f;
}

float FAnimInstanceProxy::GetInstanceTransitionTimeElapsedFraction(int32 MachineIndex, int32 TransitionIndex)
{
	if(FAnimNode_StateMachine* MachineInstance = GetStateMachineInstance(MachineIndex))
	{
		if(MachineInstance->IsValidTransitionIndex(TransitionIndex))
		{
			for(FAnimationActiveTransitionEntry& ActiveTransition : MachineInstance->ActiveTransitionArray)
			{
				if(ActiveTransition.SourceTransitionIndices.Contains(TransitionIndex))
				{
					return ActiveTransition.ElapsedTime / ActiveTransition.CrossfadeDuration;
				}
			}
		}
	}

	return 0.0f;
}

float FAnimInstanceProxy::GetRelevantAnimTimeRemaining(int32 MachineIndex, int32 StateIndex)
{
	if(FAnimNode_AssetPlayerBase* AssetPlayer = GetRelevantAssetPlayerFromState(MachineIndex, StateIndex))
	{
		if(AssetPlayer->GetAnimAsset())
		{
			return AssetPlayer->GetCurrentAssetLength() - AssetPlayer->GetCurrentAssetTimePlayRateAdjusted();
		}
	}

	return MAX_flt;
}

float FAnimInstanceProxy::GetRelevantAnimTimeRemainingFraction(int32 MachineIndex, int32 StateIndex)
{
	if(FAnimNode_AssetPlayerBase* AssetPlayer = GetRelevantAssetPlayerFromState(MachineIndex, StateIndex))
	{
		if(AssetPlayer->GetAnimAsset())
		{
			float Length = AssetPlayer->GetCurrentAssetLength();
			if(Length > 0.0f)
			{
				return (Length - AssetPlayer->GetCurrentAssetTimePlayRateAdjusted()) / Length;
			}
		}
	}

	return 1.0f;
}

float FAnimInstanceProxy::GetRelevantAnimLength(int32 MachineIndex, int32 StateIndex)
{
	if(FAnimNode_AssetPlayerBase* AssetPlayer = GetRelevantAssetPlayerFromState(MachineIndex, StateIndex))
	{
		if(AssetPlayer->GetAnimAsset())
		{
			return AssetPlayer->GetCurrentAssetLength();
		}
	}

	return 0.0f;
}

float FAnimInstanceProxy::GetRelevantAnimTime(int32 MachineIndex, int32 StateIndex)
{
	if(FAnimNode_AssetPlayerBase* AssetPlayer = GetRelevantAssetPlayerFromState(MachineIndex, StateIndex))
	{
		return AssetPlayer->GetCurrentAssetTimePlayRateAdjusted();
	}

	return 0.0f;
}

float FAnimInstanceProxy::GetRelevantAnimTimeFraction(int32 MachineIndex, int32 StateIndex)
{
	if(FAnimNode_AssetPlayerBase* AssetPlayer = GetRelevantAssetPlayerFromState(MachineIndex, StateIndex))
	{
		float Length = AssetPlayer->GetCurrentAssetLength();
		if(Length > 0.0f)
		{
			return AssetPlayer->GetCurrentAssetTimePlayRateAdjusted() / Length;
		}
	}

	return 0.0f;
}

FAnimNode_AssetPlayerBase* FAnimInstanceProxy::GetRelevantAssetPlayerFromState(int32 MachineIndex, int32 StateIndex)
{
	FAnimNode_AssetPlayerBase* ResultPlayer = nullptr;
	if(FAnimNode_StateMachine* MachineInstance = GetStateMachineInstance(MachineIndex))
	{
		float MaxWeight = 0.0f;
		const FBakedAnimationState& State = MachineInstance->GetStateInfo(StateIndex);
		for(const int32& PlayerIdx : State.PlayerNodeIndices)
		{
			if(FAnimNode_AssetPlayerBase* Player = GetNodeFromIndex<FAnimNode_AssetPlayerBase>(PlayerIdx))
			{
				if(!Player->bIgnoreForRelevancyTest && Player->GetCachedBlendWeight() > MaxWeight)
				{
					MaxWeight = Player->GetCachedBlendWeight();
					ResultPlayer = Player;
				}
			}
		}
	}
	return ResultPlayer;
}

FAnimNode_StateMachine* FAnimInstanceProxy::GetStateMachineInstance(int32 MachineIndex)
{
	if (AnimClassInterface)
	{
		const TArray<UStructProperty*>& AnimNodeProperties = AnimClassInterface->GetAnimNodeProperties();
		if ((MachineIndex >= 0) && (MachineIndex < AnimNodeProperties.Num()))
		{
			const int32 InstancePropertyIndex = AnimNodeProperties.Num() - 1 - MachineIndex;

			UStructProperty* MachineInstanceProperty = AnimNodeProperties[InstancePropertyIndex];
			checkSlow(MachineInstanceProperty->Struct->IsChildOf(FAnimNode_StateMachine::StaticStruct()));

			return MachineInstanceProperty->ContainerPtrToValuePtr<FAnimNode_StateMachine>(AnimInstanceObject);
		}
	}

	return nullptr;
}

void FAnimInstanceProxy::AddNativeTransitionBinding(const FName& MachineName, const FName& PrevStateName, const FName& NextStateName, const FCanTakeTransition& NativeTransitionDelegate, const FName& TransitionName)
{
	NativeTransitionBindings.Add(FNativeTransitionBinding(MachineName, PrevStateName, NextStateName, NativeTransitionDelegate, TransitionName));
}

bool FAnimInstanceProxy::HasNativeTransitionBinding(const FName& MachineName, const FName& PrevStateName, const FName& NextStateName, FName& OutBindingName)
{
	for(const auto& Binding : NativeTransitionBindings)
	{
		if(Binding.MachineName == MachineName && Binding.PreviousStateName == PrevStateName && Binding.NextStateName == NextStateName)
		{
#if WITH_EDITORONLY_DATA
				OutBindingName = Binding.TransitionName;
#else
			OutBindingName = NAME_None;
#endif
			return true;
		}
	}

	return false;
}

void FAnimInstanceProxy::AddNativeStateEntryBinding(const FName& MachineName, const FName& StateName, const FOnGraphStateChanged& NativeEnteredDelegate, const FName& BindingName)
{
	NativeStateEntryBindings.Add(FNativeStateBinding(MachineName, StateName, NativeEnteredDelegate, BindingName));
}
	
bool FAnimInstanceProxy::HasNativeStateEntryBinding(const FName& MachineName, const FName& StateName, FName& OutBindingName)
{
	for(const auto& Binding : NativeStateEntryBindings)
	{
		if(Binding.MachineName == MachineName && Binding.StateName == StateName)
		{
#if WITH_EDITORONLY_DATA
			OutBindingName = Binding.BindingName;
#else
			OutBindingName = NAME_None;
#endif
			return true;
		}
	}

	return false;
}

void FAnimInstanceProxy::AddNativeStateExitBinding(const FName& MachineName, const FName& StateName, const FOnGraphStateChanged& NativeExitedDelegate, const FName& BindingName)
{
	NativeStateExitBindings.Add(FNativeStateBinding(MachineName, StateName, NativeExitedDelegate, BindingName));
}

bool FAnimInstanceProxy::HasNativeStateExitBinding(const FName& MachineName, const FName& StateName, FName& OutBindingName)
{
	for(const auto& Binding : NativeStateExitBindings)
	{
		if(Binding.MachineName == MachineName && Binding.StateName == StateName)
		{
#if WITH_EDITORONLY_DATA
			OutBindingName = Binding.BindingName;
#else
			OutBindingName = NAME_None;
#endif
			return true;
		}
	}

	return false;
}

void FAnimInstanceProxy::BindNativeDelegates()
{
	// if we have no root node, we are usually in error so early out
	if(RootNode == nullptr)
	{
		return;
	}

	auto ForEachStateLambda = [&](IAnimClassInterface* InAnimClassInterface, const FName& MachineName, const FName& StateName, TFunctionRef<void(FAnimNode_StateMachine*, const FBakedAnimationState&, int32)> Predicate)
	{
		for (UStructProperty* Property : InAnimClassInterface->GetAnimNodeProperties())
		{
			if(Property && Property->Struct == FAnimNode_StateMachine::StaticStruct())
			{
				FAnimNode_StateMachine* StateMachine = Property->ContainerPtrToValuePtr<FAnimNode_StateMachine>(AnimInstanceObject);
				if(StateMachine)
				{
					const FBakedAnimationStateMachine* MachineDescription = GetMachineDescription(InAnimClassInterface, StateMachine);
					if(MachineDescription && MachineName == MachineDescription->MachineName)
					{
						// check each state transition for a match
						int32 StateIndex = 0;
						for(const FBakedAnimationState& State : MachineDescription->States)
						{
							if(State.StateName == StateName)
							{
								Predicate(StateMachine, State, StateIndex);
							}
							StateIndex++;
						}
					}
				}
			}
		}
	};

	if (AnimClassInterface)
	{
		// transition delegates
		for(const auto& Binding : NativeTransitionBindings)
		{
			ForEachStateLambda(AnimClassInterface, Binding.MachineName, Binding.PreviousStateName,
				[&](FAnimNode_StateMachine* StateMachine, const FBakedAnimationState& State, int32 StateIndex)
				{
					for(const FBakedStateExitTransition& TransitionExit : State.Transitions)
					{
						if(TransitionExit.CanTakeDelegateIndex != INDEX_NONE)
						{
							// In case the state machine hasn't been initialized, we need to re-get the desc
							const FBakedAnimationStateMachine* MachineDesc = GetMachineDescription(AnimClassInterface, StateMachine);
							const FAnimationTransitionBetweenStates& Transition = MachineDesc->Transitions[TransitionExit.TransitionIndex];
							const FBakedAnimationState& BakedState = MachineDesc->States[Transition.NextState];

							if (BakedState.StateName == Binding.NextStateName)
							{
								FAnimNode_TransitionResult* ResultNode = GetNodeFromPropertyIndex<FAnimNode_TransitionResult>(AnimInstanceObject, AnimClassInterface, TransitionExit.CanTakeDelegateIndex);
								if(ResultNode)
								{
									ResultNode->NativeTransitionDelegate = Binding.NativeTransitionDelegate;
								}
							}
						}
					}
				});
		}

		// state entry delegates
		for(const auto& Binding : NativeStateEntryBindings)
		{
			ForEachStateLambda(AnimClassInterface, Binding.MachineName, Binding.StateName,
				[&](FAnimNode_StateMachine* StateMachine, const FBakedAnimationState& State, int32 StateIndex)
				{
					// allocate enough space for all our states we need so far
					StateMachine->OnGraphStatesEntered.SetNum(FMath::Max(StateIndex + 1, StateMachine->OnGraphStatesEntered.Num()));
					StateMachine->OnGraphStatesEntered[StateIndex] = Binding.NativeStateDelegate;
				});
		}

		// state exit delegates
		for(const auto& Binding : NativeStateExitBindings)
		{
			ForEachStateLambda(AnimClassInterface, Binding.MachineName, Binding.StateName,
				[&](FAnimNode_StateMachine* StateMachine, const FBakedAnimationState& State, int32 StateIndex)
				{
					// allocate enough space for all our states we need so far
					StateMachine->OnGraphStatesExited.SetNum(FMath::Max(StateIndex + 1, StateMachine->OnGraphStatesExited.Num()));
					StateMachine->OnGraphStatesExited[StateIndex] = Binding.NativeStateDelegate;
				});
		}
	}
}

const FBakedAnimationStateMachine* FAnimInstanceProxy::GetMachineDescription(IAnimClassInterface* AnimBlueprintClass, FAnimNode_StateMachine* MachineInstance)
{
	const TArray<FBakedAnimationStateMachine>& BakedStateMachines = AnimBlueprintClass->GetBakedStateMachines();
	return BakedStateMachines.IsValidIndex(MachineInstance->StateMachineIndexInClass) ? &(BakedStateMachines[MachineInstance->StateMachineIndexInClass]) : nullptr;
}

FAnimNode_StateMachine* FAnimInstanceProxy::GetStateMachineInstanceFromName(FName MachineName)
{
	if (AnimClassInterface)
	{
		const TArray<UStructProperty*>& AnimNodeProperties = AnimClassInterface->GetAnimNodeProperties();
		for (int32 MachineIndex = 0; MachineIndex < AnimNodeProperties.Num(); MachineIndex++)
		{
			UStructProperty* Property = AnimNodeProperties[AnimNodeProperties.Num() - 1 - MachineIndex];
			if (Property && Property->Struct == FAnimNode_StateMachine::StaticStruct())
			{
				FAnimNode_StateMachine* StateMachine = Property->ContainerPtrToValuePtr<FAnimNode_StateMachine>(AnimInstanceObject);
				if (StateMachine)
				{
					if (const FBakedAnimationStateMachine* MachineDescription = GetMachineDescription(AnimClassInterface, StateMachine))
					{
						if (MachineDescription->MachineName == MachineName)
						{
							return StateMachine;
						}
					}
				}
			}
		}
	}

	return nullptr;
}

const FBakedAnimationStateMachine* FAnimInstanceProxy::GetStateMachineInstanceDesc(FName MachineName)
{
	if (AnimClassInterface)
	{
		const TArray<UStructProperty*>& AnimNodeProperties = AnimClassInterface->GetAnimNodeProperties();
		for (int32 MachineIndex = 0; MachineIndex < AnimNodeProperties.Num(); MachineIndex++)
		{
			UStructProperty* Property = AnimNodeProperties[AnimNodeProperties.Num() - 1 - MachineIndex];
			if(Property && Property->Struct == FAnimNode_StateMachine::StaticStruct())
			{
				FAnimNode_StateMachine* StateMachine = Property->ContainerPtrToValuePtr<FAnimNode_StateMachine>(AnimInstanceObject);
				if(StateMachine)
				{
					if (const FBakedAnimationStateMachine* MachineDescription = GetMachineDescription(AnimClassInterface, StateMachine))
					{
						if(MachineDescription->MachineName == MachineName)
						{
							return MachineDescription;
						}
					}
				}
			}
		}
	}

	return nullptr;
}

int32 FAnimInstanceProxy::GetStateMachineIndex(FName MachineName)
{
	if (AnimClassInterface)
	{
		const TArray<UStructProperty*>& AnimNodeProperties = AnimClassInterface->GetAnimNodeProperties();
		for (int32 MachineIndex = 0; MachineIndex < AnimNodeProperties.Num(); MachineIndex++)
		{
			UStructProperty* Property = AnimNodeProperties[AnimNodeProperties.Num() - 1 - MachineIndex];
			if(Property && Property->Struct == FAnimNode_StateMachine::StaticStruct())
			{
				FAnimNode_StateMachine* StateMachine = Property->ContainerPtrToValuePtr<FAnimNode_StateMachine>(AnimInstanceObject);
				if(StateMachine)
				{
					if (const FBakedAnimationStateMachine* MachineDescription = GetMachineDescription(AnimClassInterface, StateMachine))
					{
						if(MachineDescription->MachineName == MachineName)
						{
							return MachineIndex;
						}
					}
				}
			}
		}
	}

	return INDEX_NONE;
}

void FAnimInstanceProxy::GetStateMachineIndexAndDescription(FName InMachineName, int32& OutMachineIndex, const FBakedAnimationStateMachine** OutMachineDescription)
{
	if (AnimClassInterface)
	{
		const TArray<UStructProperty*>& AnimNodeProperties = AnimClassInterface->GetAnimNodeProperties();
		for (int32 MachineIndex = 0; MachineIndex < AnimNodeProperties.Num(); MachineIndex++)
		{
			UStructProperty* Property = AnimNodeProperties[AnimNodeProperties.Num() - 1 - MachineIndex];
			if (Property && Property->Struct == FAnimNode_StateMachine::StaticStruct())
			{
				FAnimNode_StateMachine* StateMachine = Property->ContainerPtrToValuePtr<FAnimNode_StateMachine>(AnimInstanceObject);
				if (StateMachine)
				{
					if (const FBakedAnimationStateMachine* MachineDescription = GetMachineDescription(AnimClassInterface, StateMachine))
					{
						if (MachineDescription->MachineName == InMachineName)
						{
							OutMachineIndex = MachineIndex;
							if (OutMachineDescription)
							{
								*OutMachineDescription = MachineDescription;
							}
							return;
						}
					}
				}
			}
		}
	}

	OutMachineIndex = INDEX_NONE;
	if (OutMachineDescription)
	{
		*OutMachineDescription = nullptr;
	}
}

int32 FAnimInstanceProxy::GetInstanceAssetPlayerIndex(FName MachineName, FName StateName, FName AssetName)
{
	if (AnimClassInterface)
	{
		if(const FBakedAnimationStateMachine* MachineDescription = GetStateMachineInstanceDesc(MachineName))
		{
			const TArray<UStructProperty*>& AnimNodeProperties = AnimClassInterface->GetAnimNodeProperties();
			for(int32 StateIndex = 0; StateIndex < MachineDescription->States.Num(); StateIndex++)
			{
				const FBakedAnimationState& State = MachineDescription->States[StateIndex];
				if(State.StateName == StateName)
				{
					for(int32 PlayerIndex = 0; PlayerIndex < State.PlayerNodeIndices.Num(); PlayerIndex++)
					{
						checkSlow(State.PlayerNodeIndices[PlayerIndex] < AnimNodeProperties.Num());
						UStructProperty* AssetPlayerProperty = AnimNodeProperties[AnimNodeProperties.Num() - 1 - State.PlayerNodeIndices[PlayerIndex]];
						if(AssetPlayerProperty && AssetPlayerProperty->Struct->IsChildOf(FAnimNode_AssetPlayerBase::StaticStruct()))
						{
							FAnimNode_AssetPlayerBase* AssetPlayer = AssetPlayerProperty->ContainerPtrToValuePtr<FAnimNode_AssetPlayerBase>(AnimInstanceObject);
							if(AssetPlayer)
							{
								if(AssetName == NAME_None || AssetPlayer->GetAnimAsset()->GetFName() == AssetName)
								{
									return State.PlayerNodeIndices[PlayerIndex];
								}
							}
						}
					}
				}
			}
		}
	}

	return INDEX_NONE;
}

float FAnimInstanceProxy::GetRecordedMachineWeight(const int32 InMachineClassIndex) const
{
	return MachineWeightArrays[GetSyncGroupReadIndex()][InMachineClassIndex];
}

void FAnimInstanceProxy::RecordMachineWeight(const int32 InMachineClassIndex, const float InMachineWeight)
{
	MachineWeightArrays[GetSyncGroupWriteIndex()][InMachineClassIndex] = InMachineWeight;
}

float FAnimInstanceProxy::GetRecordedStateWeight(const int32 InMachineClassIndex, const int32 InStateIndex) const
{
	const int32* BaseIndexPtr = StateMachineClassIndexToWeightOffset.Find(InMachineClassIndex);

	if(BaseIndexPtr)
	{
		const int32 StateIndex = *BaseIndexPtr + InStateIndex;
		return StateWeightArrays[GetSyncGroupReadIndex()][StateIndex];
	}

	return 0.0f;
}

void FAnimInstanceProxy::RecordStateWeight(const int32 InMachineClassIndex, const int32 InStateIndex, const float InStateWeight)
{
	const int32* BaseIndexPtr = StateMachineClassIndexToWeightOffset.Find(InMachineClassIndex);

	if(BaseIndexPtr)
	{
		const int32 StateIndex = *BaseIndexPtr + InStateIndex;
		StateWeightArrays[GetSyncGroupWriteIndex()][StateIndex] = InStateWeight;
	}
}

void FAnimInstanceProxy::ResetDynamics()
{
	for(FAnimNode_Base* Node : DynamicResetNodes)
	{
		Node->ResetDynamics();
	}
}

#if WITH_EDITOR
void FAnimInstanceProxy::RegisterWatchedPose(const FCompactPose& Pose, int32 LinkID)
{
	if(bIsBeingDebugged)
	{
		for (FAnimNodePoseWatch& PoseWatch : PoseWatchEntriesForThisFrame)
		{
			if (PoseWatch.NodeID == LinkID)
			{
				PoseWatch.PoseInfo->CopyBonesFrom(Pose);
				break;
			}
		}
	}
}

void FAnimInstanceProxy::RegisterWatchedPose(const FCSPose<FCompactPose>& Pose, int32 LinkID)
{
	if (bIsBeingDebugged)
	{
		for (FAnimNodePoseWatch& PoseWatch : PoseWatchEntriesForThisFrame)
		{
			if (PoseWatch.NodeID == LinkID)
			{
				FCompactPose TempPose;
				Pose.ConvertToLocalPoses(TempPose);
				PoseWatch.PoseInfo->CopyBonesFrom(TempPose);
				break;
			}
		}
	}
}
#endif

const FPoseSnapshot* FAnimInstanceProxy::GetPoseSnapshot(FName SnapshotName) const
{
	return PoseSnapshots.FindByPredicate([SnapshotName](const FPoseSnapshot& PoseData) { return PoseData.SnapshotName == SnapshotName; });
}

#undef LOCTEXT_NAMESPACE

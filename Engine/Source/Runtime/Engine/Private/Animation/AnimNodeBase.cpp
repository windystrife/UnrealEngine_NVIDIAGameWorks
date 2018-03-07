// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNodeBase.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// FAnimationBaseContext

FAnimationBaseContext::FAnimationBaseContext(FAnimInstanceProxy* InAnimInstanceProxy)
	: AnimInstanceProxy(InAnimInstanceProxy)
{
}

FAnimationBaseContext::FAnimationBaseContext(const FAnimationBaseContext& InContext)
{
	AnimInstanceProxy = InContext.AnimInstanceProxy;
}

IAnimClassInterface* FAnimationBaseContext::GetAnimClass() const
{
	return AnimInstanceProxy ? AnimInstanceProxy->GetAnimClassInterface() : nullptr;
}

#if WITH_EDITORONLY_DATA
UAnimBlueprint* FAnimationBaseContext::GetAnimBlueprint() const
{
	return AnimInstanceProxy ? AnimInstanceProxy->GetAnimBlueprint() : nullptr;
}
#endif //WITH_EDITORONLY_DATA

/////////////////////////////////////////////////////
// FPoseContext

void FPoseContext::Initialize(FAnimInstanceProxy* InAnimInstanceProxy)
{
	checkSlow(AnimInstanceProxy && AnimInstanceProxy->GetRequiredBones().IsValid());
	const FBoneContainer& RequiredBone = AnimInstanceProxy->GetRequiredBones();
	Pose.SetBoneContainer(&RequiredBone);
	Curve.InitFrom(RequiredBone);
}

/////////////////////////////////////////////////////
// FComponentSpacePoseContext

void FComponentSpacePoseContext::ResetToRefPose()
{
	checkSlow(AnimInstanceProxy && AnimInstanceProxy->GetRequiredBones().IsValid());
	const FBoneContainer& RequiredBone = AnimInstanceProxy->GetRequiredBones();
	Pose.InitPose(&RequiredBone);
	Curve.InitFrom(RequiredBone);
}

/////////////////////////////////////////////////////
// FAnimNode_Base

PRAGMA_DISABLE_DEPRECATION_WARNINGS
void FAnimNode_Base::Initialize(const FAnimationInitializeContext& Context)
{
	EvaluateGraphExposedInputs.Initialize(this, Context.AnimInstanceProxy->GetAnimInstanceObject());	
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FAnimNode_Base::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	// Call legacy implementation for backwards compatibility
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Initialize(Context);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FAnimNode_Base::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	// Call legacy implementation for backwards compatibility
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	CacheBones(Context);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FAnimNode_Base::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	// Call legacy implementation for backwards compatibility
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Update(Context);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FAnimNode_Base::Evaluate_AnyThread(FPoseContext& Output)
{
	// Call legacy implementation for backwards compatibility
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Evaluate(Output);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FAnimNode_Base::Evaluate_AnyThread(FPoseContext& Output, bool bExpectsAdditivePose)
{
	// Should assert if not overridden but may break too many things
	Evaluate_AnyThread(Output);
}

void FAnimNode_Base::EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output)
{
	// Call legacy implementation for backwards compatibility
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	EvaluateComponentSpace(Output);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

bool FAnimNode_Base::IsLODEnabled(FAnimInstanceProxy* AnimInstanceProxy, int32 InLODThreshold)
{
	return ((InLODThreshold == INDEX_NONE) || (AnimInstanceProxy->GetLODLevel() <= InLODThreshold));
}

void FAnimNode_Base::OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance)
{
	// Call legacy implementation for backwards compatibility
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	RootInitialize(InProxy);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

/////////////////////////////////////////////////////
// FPoseLinkBase

void FPoseLinkBase::AttemptRelink(const FAnimationBaseContext& Context)
{
	// Do the linkage
	if ((LinkedNode == NULL) && (LinkID != INDEX_NONE))
	{
		IAnimClassInterface* AnimBlueprintClass = Context.GetAnimClass();
		check(AnimBlueprintClass);

		// adding ensure. We had a crash on here
		if ( ensure(AnimBlueprintClass->GetAnimNodeProperties().IsValidIndex(LinkID)) )
		{
			UProperty* LinkedProperty = AnimBlueprintClass->GetAnimNodeProperties()[LinkID];
			void* LinkedNodePtr = LinkedProperty->ContainerPtrToValuePtr<void>(Context.AnimInstanceProxy->GetAnimInstanceObject());
			LinkedNode = (FAnimNode_Base*)LinkedNodePtr;
		}
	}
}

void FPoseLinkBase::Initialize(const FAnimationInitializeContext& Context)
{
#if DO_CHECK
	checkf(!bProcessed, TEXT("Initialize already in progress, circular link for AnimInstance [%s] Blueprint [%s]"), \
		*Context.AnimInstanceProxy->GetAnimInstanceName(), *GetFullNameSafe(IAnimClassInterface::GetActualAnimClass(Context.AnimInstanceProxy->GetAnimClassInterface())));
	TGuardValue<bool> CircularGuard(bProcessed, true);
#endif

	AttemptRelink(Context);

#if ENABLE_ANIMGRAPH_TRAVERSAL_DEBUG
	InitializationCounter.SynchronizeWith(Context.AnimInstanceProxy->GetInitializationCounter());

	// Initialization will require update to be called before an evaluate.
	UpdateCounter.Reset();
#endif

	// Do standard initialization
	if (LinkedNode != NULL)
	{
		LinkedNode->Initialize_AnyThread(Context);
	}
}

void FPoseLinkBase::SetLinkNode(struct FAnimNode_Base* NewLinkNode)
{
	// this is custom interface, only should be used by native handlers
	LinkedNode = NewLinkNode;
}

FAnimNode_Base* FPoseLinkBase::GetLinkNode()
{
	return LinkedNode;
}

void FPoseLinkBase::CacheBones(const FAnimationCacheBonesContext& Context) 
{
#if DO_CHECK
	checkf( !bProcessed, TEXT( "CacheBones already in progress, circular link for AnimInstance [%s] Blueprint [%s]" ), \
		*Context.AnimInstanceProxy->GetAnimInstanceName(), *GetFullNameSafe(IAnimClassInterface::GetActualAnimClass(Context.AnimInstanceProxy->GetAnimClassInterface())));
	TGuardValue<bool> CircularGuard(bProcessed, true);
#endif

#if ENABLE_ANIMGRAPH_TRAVERSAL_DEBUG
	CachedBonesCounter.SynchronizeWith(Context.AnimInstanceProxy->GetCachedBonesCounter());
#endif

	if (LinkedNode != NULL)
	{
		LinkedNode->CacheBones_AnyThread(Context);
	}
}

void FPoseLinkBase::Update(const FAnimationUpdateContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FPoseLinkBase_Update);
#if DO_CHECK
	checkf( !bProcessed, TEXT( "Update already in progress, circular link for AnimInstance [%s] Blueprint [%s]" ), \
		*Context.AnimInstanceProxy->GetAnimInstanceName(), *GetFullNameSafe(IAnimClassInterface::GetActualAnimClass(Context.AnimInstanceProxy->GetAnimClassInterface())));
	TGuardValue<bool> CircularGuard(bProcessed, true);
#endif

#if WITH_EDITOR
	if (GIsEditor)
	{
		if (LinkedNode == NULL)
		{
			//@TODO: Should only do this when playing back
			AttemptRelink(Context);
		}

		// Record the node line activation
		if (LinkedNode != NULL)
		{
			if (Context.AnimInstanceProxy->IsBeingDebugged())
			{
				Context.AnimInstanceProxy->RecordNodeVisit(LinkID, SourceLinkID, Context.GetFinalBlendWeight());
			}
		}
	}
#endif

#if ENABLE_ANIMGRAPH_TRAVERSAL_DEBUG
	checkf(InitializationCounter.IsSynchronizedWith(Context.AnimInstanceProxy->GetInitializationCounter()), TEXT("Calling Update without initialization!"));
	UpdateCounter.SynchronizeWith(Context.AnimInstanceProxy->GetUpdateCounter());
#endif

	if (LinkedNode != NULL)
	{
		LinkedNode->Update_AnyThread(Context);
	}
}

void FPoseLinkBase::GatherDebugData(FNodeDebugData& DebugData)
{
	if(LinkedNode != NULL)
	{
		LinkedNode->GatherDebugData(DebugData);
	}
}

/////////////////////////////////////////////////////
// FPoseLink

void FPoseLink::Evaluate(FPoseContext& Output, bool bExpectsAdditivePose)
{
#if DO_CHECK
	checkf( !bProcessed, TEXT( "Evaluate already in progress, circular link for AnimInstance [%s] Blueprint [%s]" ), \
		*Output.AnimInstanceProxy->GetAnimInstanceName(), *GetFullNameSafe(IAnimClassInterface::GetActualAnimClass(Output.AnimInstanceProxy->GetAnimClassInterface())));
	TGuardValue<bool> CircularGuard(bProcessed, true);
#endif

#if WITH_EDITOR
	if ((LinkedNode == NULL) && GIsEditor)
	{
		//@TODO: Should only do this when playing back
		AttemptRelink(Output);
	}
#endif

#if ENABLE_ANIMGRAPH_TRAVERSAL_DEBUG
	checkf(InitializationCounter.IsSynchronizedWith(Output.AnimInstanceProxy->GetInitializationCounter()), TEXT("Calling Evaluate without initialization!"));
	checkf(UpdateCounter.IsSynchronizedWith(Output.AnimInstanceProxy->GetUpdateCounter()), TEXT("Calling Evaluate without Update for this node!"));
	checkf(CachedBonesCounter.IsSynchronizedWith(Output.AnimInstanceProxy->GetCachedBonesCounter()), TEXT("Calling Evaluate without CachedBones!"));
	EvaluationCounter.SynchronizeWith(Output.AnimInstanceProxy->GetEvaluationCounter());
#endif

	if (LinkedNode != NULL)
	{
#if ENABLE_ANIMNODE_POSE_DEBUG
		CurrentPose.ResetToAdditiveIdentity();
#endif
		LinkedNode->Evaluate_AnyThread(Output, bExpectsAdditivePose);
#if ENABLE_ANIMNODE_POSE_DEBUG
		CurrentPose.CopyBonesFrom(Output.Pose);
#endif

#if WITH_EDITOR
		Output.AnimInstanceProxy->RegisterWatchedPose(Output.Pose, LinkID);
#endif
	}
	else if (bExpectsAdditivePose)
	{
		Output.ResetToAdditiveIdentity();
	}
	else
	{
		//@TODO: Warning here?
		Output.ResetToRefPose();
	}

	// Detect non valid output
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (Output.ContainsNaN())
	{
		// Show bone transform with some useful debug info
		for (const FTransform& Bone : Output.Pose.GetBones())
		{
			if (Bone.ContainsNaN())
			{
				ensureMsgf(!Bone.ContainsNaN(), TEXT("Bone transform contains NaN from AnimInstance:[%s] Node:[%s] Value:[%s]")
					, *Output.AnimInstanceProxy->GetAnimInstanceName(), LinkedNode ? *LinkedNode->StaticStruct()->GetName() : TEXT("NULL"), *Bone.ToString());
			}
		}
	}

	if (!Output.IsNormalized())
	{
		// Show bone transform with some useful debug info
		for (const FTransform& Bone : Output.Pose.GetBones())
		{
			if (!Bone.IsRotationNormalized())
			{
				ensureMsgf(Bone.IsRotationNormalized(), TEXT("Bone Rotation not normalized from AnimInstance:[%s] Node:[%s] Rotation:[%s]")
					, *Output.AnimInstanceProxy->GetAnimInstanceName(), LinkedNode ? *LinkedNode->StaticStruct()->GetName() : TEXT("NULL"), *Bone.GetRotation().ToString());
			}
		}
	}
#endif
}

/////////////////////////////////////////////////////
// FComponentSpacePoseLink

void FComponentSpacePoseLink::EvaluateComponentSpace(FComponentSpacePoseContext& Output)
{
#if DO_CHECK
	checkf( !bProcessed, TEXT( "EvaluateComponentSpace already in progress, circular link for AnimInstance [%s] Blueprint [%s]" ), \
		*Output.AnimInstanceProxy->GetAnimInstanceName(), *GetFullNameSafe(IAnimClassInterface::GetActualAnimClass(Output.AnimInstanceProxy->GetAnimClassInterface())));
	TGuardValue<bool> CircularGuard(bProcessed, true);
#endif

#if ENABLE_ANIMGRAPH_TRAVERSAL_DEBUG
	checkf(InitializationCounter.IsSynchronizedWith(Output.AnimInstanceProxy->GetInitializationCounter()), TEXT("Calling EvaluateComponentSpace without initialization!"));
	checkf(CachedBonesCounter.IsSynchronizedWith(Output.AnimInstanceProxy->GetCachedBonesCounter()), TEXT("Calling EvaluateComponentSpace without CachedBones!"));
	checkf(UpdateCounter.IsSynchronizedWith(Output.AnimInstanceProxy->GetUpdateCounter()), TEXT("Calling EvaluateComponentSpace without Update for this node!"));
	EvaluationCounter.SynchronizeWith(Output.AnimInstanceProxy->GetEvaluationCounter());
#endif

	if (LinkedNode != NULL)
	{
		LinkedNode->EvaluateComponentSpace_AnyThread(Output);

#if WITH_EDITOR
		Output.AnimInstanceProxy->RegisterWatchedPose(Output.Pose, LinkID);
#endif
	}
	else
	{
		//@TODO: Warning here?
		Output.ResetToRefPose();
	}

	// Detect non valid output
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (Output.ContainsNaN())
	{
		// Show bone transform with some useful debug info
		for (const FTransform& Bone : Output.Pose.GetPose().GetBones())
		{
			if (Bone.ContainsNaN())
			{
				ensureMsgf(!Bone.ContainsNaN(), TEXT("Bone transform contains NaN from AnimInstance:[%s] Node:[%s] Value:[%s]")
					, *Output.AnimInstanceProxy->GetAnimInstanceName(), LinkedNode ? *LinkedNode->StaticStruct()->GetName() : TEXT("NULL"), *Bone.ToString());
			}
		}
	}

	if (!Output.IsNormalized())
	{
		// Show bone transform with some useful debug info
		for (const FTransform& Bone : Output.Pose.GetPose().GetBones())
		{
			if (!Bone.IsRotationNormalized())
			{
				ensureMsgf(Bone.IsRotationNormalized(), TEXT("Bone Rotation not normalized from AnimInstance:[%s] Node:[%s] Value:[%s]")
					, *Output.AnimInstanceProxy->GetAnimInstanceName(), LinkedNode ? *LinkedNode->StaticStruct()->GetName() : TEXT("NULL"), *Bone.ToString());
			}
		}
	}
#endif
}

/////////////////////////////////////////////////////
// FComponentSpacePoseContext

bool FComponentSpacePoseContext::ContainsNaN() const
{
	return Pose.GetPose().ContainsNaN();
}

bool FComponentSpacePoseContext::IsNormalized() const
{
	return Pose.GetPose().IsNormalized();
}

/////////////////////////////////////////////////////
// FNodeDebugData

void FNodeDebugData::AddDebugItem(FString DebugData, bool bPoseSource)
{
	check(NodeChain.Num() == 0 || NodeChain.Last().ChildNodeChain.Num() == 0); //Cannot add to this chain once we have branched

	NodeChain.Add( DebugItem(DebugData, bPoseSource) );
	NodeChain.Last().ChildNodeChain.Reserve(ANIM_NODE_DEBUG_MAX_CHILDREN);
}

FNodeDebugData& FNodeDebugData::BranchFlow(float BranchWeight, FString InNodeDescription)
{
	NodeChain.Last().ChildNodeChain.Add(FNodeDebugData(AnimInstance, BranchWeight*AbsoluteWeight, InNodeDescription, RootNodePtr));
	NodeChain.Last().ChildNodeChain.Last().NodeChain.Reserve(ANIM_NODE_DEBUG_MAX_CHAIN);
	return NodeChain.Last().ChildNodeChain.Last();
}

FNodeDebugData* FNodeDebugData::GetCachePoseDebugData(float GlobalWeight)
{
	check(RootNodePtr);

	RootNodePtr->SaveCachePoseNodes.Add( FNodeDebugData(AnimInstance, GlobalWeight, FString(), RootNodePtr) );
	RootNodePtr->SaveCachePoseNodes.Last().NodeChain.Reserve(ANIM_NODE_DEBUG_MAX_CHAIN);
	return &RootNodePtr->SaveCachePoseNodes.Last();
}

void FNodeDebugData::GetFlattenedDebugData(TArray<FFlattenedDebugData>& FlattenedDebugData, int32 Indent, int32& ChainID)
{
	int32 CurrChainID = ChainID;
	for(DebugItem& Item : NodeChain)
	{
		FlattenedDebugData.Add( FFlattenedDebugData(Item.DebugData, AbsoluteWeight, Indent, CurrChainID, Item.bPoseSource) );
		bool bMultiBranch = Item.ChildNodeChain.Num() > 1;
		int32 ChildIndent = bMultiBranch ? Indent + 1 : Indent;
		for(FNodeDebugData& Child : Item.ChildNodeChain)
		{
			if(bMultiBranch)
			{
				// If we only have one branch we treat it as the same really
				// as we may have only changed active status
				++ChainID;
			}
			Child.GetFlattenedDebugData(FlattenedDebugData, ChildIndent, ChainID);
		}
	}

	// Do CachePose nodes only from the root.
	if (RootNodePtr == this)
	{
		for (FNodeDebugData& CachePoseData : SaveCachePoseNodes)
		{
			++ChainID;
			CachePoseData.GetFlattenedDebugData(FlattenedDebugData, 0, ChainID);
		}
	}
}

void FExposedValueCopyRecord::PostSerialize(const FArchive& Ar)
{
	// backwards compatibility: check value of deprecated source property and patch up property name
	if(SourceProperty_DEPRECATED && SourcePropertyName == NAME_None)
	{
		SourcePropertyName = SourceProperty_DEPRECATED->GetFName();
	}
}

void FExposedValueHandler::Initialize(FAnimNode_Base* AnimNode, UObject* AnimInstanceObject) 
{
	if(bInitialized)
	{
		return;
	}

	if (BoundFunction != NAME_None)
	{
		// we cant call FindFunction on anything but the game thread as it accesses a shared map in the object's class
		check(IsInGameThread());
		Function = AnimInstanceObject->FindFunction(BoundFunction);
		check(Function);
	}
	else
	{
		Function = NULL;
	}

	// initialize copy records
	for(FExposedValueCopyRecord& CopyRecord : CopyRecords)
	{
		UProperty* SourceProperty = AnimInstanceObject->GetClass()->FindPropertyByName(CopyRecord.SourcePropertyName);
		check(SourceProperty);
		if(UArrayProperty* SourceArrayProperty = Cast<UArrayProperty>(SourceProperty))
		{
			// the compiler should not be generating any code that calls down this path at the moment - it is untested
			check(false);
		//	FScriptArrayHelper ArrayHelper(SourceArrayProperty, SourceProperty->ContainerPtrToValuePtr<uint8>(AnimInstanceObject));
		//	check(ArrayHelper.IsValidIndex(CopyRecord.SourceArrayIndex));
		//	CopyRecord.Source = ArrayHelper.GetRawPtr(CopyRecord.SourceArrayIndex);
		//	CopyRecord.Size = ArrayHelper.Num() * SourceArrayProperty->Inner->GetSize();
		}
		else
		{
			if(CopyRecord.SourceSubPropertyName != NAME_None)
			{
				void* Source = SourceProperty->ContainerPtrToValuePtr<uint8>(AnimInstanceObject, 0);
				UStructProperty* SourceStructProperty = CastChecked<UStructProperty>(SourceProperty);
				UProperty* SourceStructSubProperty = SourceStructProperty->Struct->FindPropertyByName(CopyRecord.SourceSubPropertyName);
				CopyRecord.Source = SourceStructSubProperty->ContainerPtrToValuePtr<uint8>(Source, CopyRecord.SourceArrayIndex);
				CopyRecord.Size = SourceStructSubProperty->GetSize();
				CopyRecord.CachedSourceProperty = SourceStructSubProperty;
				CopyRecord.CachedSourceContainer = Source;
			}
			else
			{
				CopyRecord.Source = SourceProperty->ContainerPtrToValuePtr<uint8>(AnimInstanceObject, CopyRecord.SourceArrayIndex);
				CopyRecord.Size = SourceProperty->GetSize();
				CopyRecord.CachedSourceProperty = SourceProperty;
				CopyRecord.CachedSourceContainer = AnimInstanceObject;
			}
		}

		if(UArrayProperty* DestArrayProperty = Cast<UArrayProperty>(CopyRecord.DestProperty))
		{
			FScriptArrayHelper ArrayHelper(DestArrayProperty, CopyRecord.DestProperty->ContainerPtrToValuePtr<uint8>(AnimNode));
			check(ArrayHelper.IsValidIndex(CopyRecord.DestArrayIndex));
			CopyRecord.Dest = ArrayHelper.GetRawPtr(CopyRecord.DestArrayIndex);

			if(CopyRecord.bInstanceIsTarget)
			{
				CopyRecord.CachedDestContainer = AnimInstanceObject;
			}
			else
			{
				CopyRecord.CachedDestContainer = AnimNode;
			}
		}
		else
		{
			CopyRecord.Dest = CopyRecord.DestProperty->ContainerPtrToValuePtr<uint8>(AnimNode, CopyRecord.DestArrayIndex);
			
			if(CopyRecord.bInstanceIsTarget)
			{
				CopyRecord.CachedDestContainer = AnimInstanceObject;
				CopyRecord.Dest = CopyRecord.DestProperty->ContainerPtrToValuePtr<uint8>(AnimInstanceObject, CopyRecord.DestArrayIndex);
			}
			else
			{
				CopyRecord.CachedDestContainer = AnimNode;
			}
		}

		if(UBoolProperty* BoolProperty = Cast<UBoolProperty>(CopyRecord.DestProperty))
		{
			CopyRecord.CopyType = ECopyType::BoolProperty;
		}
		else if(UStructProperty* StructProperty = Cast<UStructProperty>(CopyRecord.DestProperty))
		{
			CopyRecord.CopyType = ECopyType::StructProperty;
		}
		else if(UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(CopyRecord.DestProperty))
		{
			CopyRecord.CopyType = ECopyType::ObjectProperty;
		}
		else
		{
			CopyRecord.CopyType = ECopyType::MemCopy;
		}
	}

	bInitialized = true;
}

void FExposedValueHandler::Execute(const FAnimationBaseContext& Context) const
{
	if (Function != nullptr)
	{
		Context.AnimInstanceProxy->GetAnimInstanceObject()->ProcessEvent(Function, NULL);
	}

	for(const FExposedValueCopyRecord& CopyRecord : CopyRecords)
	{
		// if any of these checks fail then it's likely that Initialize has not been called.
		// has new anim node type been added that doesnt call the base class Initialize()?
		checkSlow(CopyRecord.Dest != nullptr);
		checkSlow(CopyRecord.Source != nullptr);
		checkSlow(CopyRecord.Size != 0);
		
		switch(CopyRecord.PostCopyOperation)
		{
		case EPostCopyOperation::None:
			{
				switch(CopyRecord.CopyType)
				{
				default:
				case ECopyType::MemCopy:
					FMemory::Memcpy(CopyRecord.Dest, CopyRecord.Source, CopyRecord.Size);
					break;
				case ECopyType::BoolProperty:
					{
						bool bValue = static_cast<UBoolProperty*>(CopyRecord.CachedSourceProperty)->GetPropertyValue_InContainer(CopyRecord.CachedSourceContainer);
						static_cast<UBoolProperty*>(CopyRecord.DestProperty)->SetPropertyValue_InContainer(CopyRecord.CachedDestContainer, bValue, CopyRecord.DestArrayIndex);
					}
					break;
				case ECopyType::StructProperty:
					static_cast<UStructProperty*>(CopyRecord.DestProperty)->Struct->CopyScriptStruct(CopyRecord.Dest, CopyRecord.Source);
					break;
				case ECopyType::ObjectProperty:
					{
						UObject* Value = static_cast<UObjectPropertyBase*>(CopyRecord.CachedSourceProperty)->GetObjectPropertyValue_InContainer(CopyRecord.CachedSourceContainer);
						static_cast<UObjectPropertyBase*>(CopyRecord.DestProperty)->SetObjectPropertyValue_InContainer(CopyRecord.CachedDestContainer, Value, CopyRecord.DestArrayIndex);
					}
					break;
				}
			}
			break;
		case EPostCopyOperation::LogicalNegateBool:
			{
				check(CopyRecord.CachedSourceProperty != nullptr && CopyRecord.DestProperty != nullptr);

				bool bValue = static_cast<UBoolProperty*>(CopyRecord.CachedSourceProperty)->GetPropertyValue_InContainer(CopyRecord.CachedSourceContainer);
				static_cast<UBoolProperty*>(CopyRecord.DestProperty)->SetPropertyValue_InContainer(CopyRecord.CachedDestContainer, !bValue, CopyRecord.DestArrayIndex);
			}
			break;
		}
	}
}

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimCurveTypes.h"
#include "BonePose.h"
#include "AnimNodeBase.generated.h"

class IAnimClassInterface;
class UAnimBlueprint;
class UAnimInstance;
struct FAnimInstanceProxy;
struct FAnimNode_Base;

/** Base class for update/evaluate contexts */
struct FAnimationBaseContext
{
public:
	FAnimInstanceProxy* AnimInstanceProxy;

protected:
	// DEPRECATED - Please use constructor that uses an FAnimInstanceProxy*
	ENGINE_API FAnimationBaseContext(UAnimInstance* InAnimInstance);

	ENGINE_API FAnimationBaseContext(FAnimInstanceProxy* InAnimInstanceProxy);

public:
	// we define a copy constructor here simply to avoid deprecation warnings with clang
	ENGINE_API FAnimationBaseContext(const FAnimationBaseContext& InContext);

public:
	// Get the Blueprint IAnimClassInterface associated with this context, if there is one.
	// Note: This can return NULL, so check the result.
	ENGINE_API IAnimClassInterface* GetAnimClass() const;

#if WITH_EDITORONLY_DATA
	// Get the AnimBlueprint associated with this context, if there is one.
	// Note: This can return NULL, so check the result.
	ENGINE_API UAnimBlueprint* GetAnimBlueprint() const;
#endif //WITH_EDITORONLY_DATA
};


/** Initialization context passed around during animation tree initialization */
struct FAnimationInitializeContext : public FAnimationBaseContext
{
public:
	FAnimationInitializeContext(FAnimInstanceProxy* InAnimInstanceProxy)
		: FAnimationBaseContext(InAnimInstanceProxy)
	{
	}
};

/**
 * Context passed around when RequiredBones array changed and cached bones indices have to be refreshed.
 * (RequiredBones array changed because of an LOD switch for example)
 */
struct FAnimationCacheBonesContext : public FAnimationBaseContext
{
public:
	FAnimationCacheBonesContext(FAnimInstanceProxy* InAnimInstanceProxy)
		: FAnimationBaseContext(InAnimInstanceProxy)
	{
	}
};

/** Update context passed around during animation tree update */
struct FAnimationUpdateContext : public FAnimationBaseContext
{
private:
	float CurrentWeight;
	float RootMotionWeightModifier;

	float DeltaTime;
public:
	FAnimationUpdateContext(FAnimInstanceProxy* InAnimInstanceProxy, float InDeltaTime)
		: FAnimationBaseContext(InAnimInstanceProxy)
		, CurrentWeight(1.0f)
		, RootMotionWeightModifier(1.f)
		, DeltaTime(InDeltaTime)
	{
	}

	FAnimationUpdateContext FractionalWeight(float Multiplier) const
	{
		FAnimationUpdateContext Result(AnimInstanceProxy, DeltaTime);
		Result.CurrentWeight = CurrentWeight * Multiplier;
		Result.RootMotionWeightModifier = RootMotionWeightModifier;
		return Result;
	}

	FAnimationUpdateContext FractionalWeightAndRootMotion(float WeightMultiplier, float RootMotionMultiplier) const
	{
		FAnimationUpdateContext Result(AnimInstanceProxy, DeltaTime);
		Result.CurrentWeight = CurrentWeight * WeightMultiplier;
		Result.RootMotionWeightModifier = RootMotionWeightModifier * RootMotionMultiplier;

		return Result;
	}

	FAnimationUpdateContext FractionalWeightAndTime(float WeightMultiplier, float TimeMultiplier) const
	{
		FAnimationUpdateContext Result(AnimInstanceProxy, DeltaTime * TimeMultiplier);
		Result.CurrentWeight = CurrentWeight * WeightMultiplier;
		Result.RootMotionWeightModifier = RootMotionWeightModifier;
		return Result;
	}

	FAnimationUpdateContext FractionalWeightTimeAndRootMotion(float WeightMultiplier, float TimeMultiplier, float RootMotionMultiplier) const
	{
		FAnimationUpdateContext Result(AnimInstanceProxy, DeltaTime * TimeMultiplier);
		Result.CurrentWeight = CurrentWeight * WeightMultiplier;
		Result.RootMotionWeightModifier = RootMotionWeightModifier * RootMotionMultiplier;

		return Result;
	}

	// Returns the final blend weight contribution for this stage
	float GetFinalBlendWeight() const { return CurrentWeight; }

	// Returns the weight modifier for root motion (as root motion weight wont always match blend weight)
	float GetRootMotionWeightModifier() const { return RootMotionWeightModifier; }

	// Returns the delta time for this update, in seconds
	float GetDeltaTime() const { return DeltaTime; }
};


/** Evaluation context passed around during animation tree evaluation */
struct FPoseContext : public FAnimationBaseContext
{
public:
	/* These Pose/Curve is stack allocator. You should not use it outside of stack. */
	FCompactPose	Pose;
	FBlendedCurve	Curve;

public:
	// This constructor allocates a new uninitialized pose for the specified anim instance
	FPoseContext(FAnimInstanceProxy* InAnimInstanceProxy)
		: FAnimationBaseContext(InAnimInstanceProxy)
	{
		Initialize(InAnimInstanceProxy);
	}

	// This constructor allocates a new uninitialized pose, copying non-pose state from the source context
	FPoseContext(const FPoseContext& SourceContext)
		: FAnimationBaseContext(SourceContext.AnimInstanceProxy)
	{
		Initialize(SourceContext.AnimInstanceProxy);
	}

	ENGINE_API void Initialize(FAnimInstanceProxy* InAnimInstanceProxy);

	void ResetToRefPose()
	{
		Pose.ResetToRefPose();	
	}

	void ResetToAdditiveIdentity()
	{
		Pose.ResetToAdditiveIdentity();
	}

	bool ContainsNaN() const
	{
		return Pose.ContainsNaN();
	}

	bool IsNormalized() const
	{
		return Pose.IsNormalized();
	}

	FPoseContext& operator=(const FPoseContext& Other)
	{
		if (AnimInstanceProxy != Other.AnimInstanceProxy)
		{
			Initialize(AnimInstanceProxy);
		}

		Pose = Other.Pose;
		Curve = Other.Curve;
		return *this;
	}
};


/** Evaluation context passed around during animation tree evaluation */
struct FComponentSpacePoseContext : public FAnimationBaseContext
{
public:
	FCSPose<FCompactPose>	Pose;
	FBlendedCurve			Curve;

public:
	// This constructor allocates a new uninitialized pose for the specified anim instance
	FComponentSpacePoseContext(FAnimInstanceProxy* InAnimInstanceProxy)
		: FAnimationBaseContext(InAnimInstanceProxy)
	{
		// No need to initialize, done through FA2CSPose::AllocateLocalPoses
	}

	// This constructor allocates a new uninitialized pose, copying non-pose state from the source context
	FComponentSpacePoseContext(const FComponentSpacePoseContext& SourceContext)
		: FAnimationBaseContext(SourceContext.AnimInstanceProxy)
	{
		// No need to initialize, done through FA2CSPose::AllocateLocalPoses
	}

	ENGINE_API void ResetToRefPose();

	ENGINE_API bool ContainsNaN() const;
	ENGINE_API bool IsNormalized() const;
};

/**
 * We pass array items by reference, which is scary as TArray can move items around in memory.
 * So we make sure to allocate enough here so it doesn't happen and crash on us.
 */
#define ANIM_NODE_DEBUG_MAX_CHAIN 50
#define ANIM_NODE_DEBUG_MAX_CHILDREN 12
#define ANIM_NODE_DEBUG_MAX_CACHEPOSE 20

struct ENGINE_API FNodeDebugData
{
private:
	struct DebugItem
	{
		DebugItem(FString Data, bool bInPoseSource) : DebugData(Data), bPoseSource(bInPoseSource) {}

		/** This node item's debug text to display. */
		FString DebugData;

		/** Whether we are supplying a pose instead of modifying one (e.g. an playing animation). */
		bool bPoseSource;

		/** Nodes that we are connected to. */
		TArray<FNodeDebugData> ChildNodeChain;
	};

	/** This nodes final contribution weight (based on its own weight and the weight of its parents). */
	float AbsoluteWeight;

	/** Nodes that we are dependent on. */
	TArray<DebugItem> NodeChain;

	/** Additional info provided, used in GetNodeName. States machines can provide the state names for the Root Nodes to use for example. */
	FString NodeDescription;

	/** Pointer to RootNode */
	FNodeDebugData* RootNodePtr;

	/** SaveCachePose Nodes */
	TArray<FNodeDebugData> SaveCachePoseNodes;

public:
	struct FFlattenedDebugData
	{
		FFlattenedDebugData(FString Line, float AbsWeight, int32 InIndent, int32 InChainID, bool bInPoseSource) : DebugLine(Line), AbsoluteWeight(AbsWeight), Indent(InIndent), ChainID(InChainID), bPoseSource(bInPoseSource){}
		FString DebugLine;
		float AbsoluteWeight;
		int32 Indent;
		int32 ChainID;
		bool bPoseSource;

		bool IsOnActiveBranch() { return FAnimWeight::IsRelevant(AbsoluteWeight); }
	};

	FNodeDebugData(const class UAnimInstance* InAnimInstance) 
		: AbsoluteWeight(1.f), RootNodePtr(this), AnimInstance(InAnimInstance)
	{
		SaveCachePoseNodes.Reserve(ANIM_NODE_DEBUG_MAX_CACHEPOSE);
	}
	
	FNodeDebugData(const class UAnimInstance* InAnimInstance, const float AbsWeight, FString InNodeDescription, FNodeDebugData* InRootNodePtr)
		: AbsoluteWeight(AbsWeight)
		, NodeDescription(InNodeDescription)
		, RootNodePtr(InRootNodePtr)
		, AnimInstance(InAnimInstance) 
	{}

	void AddDebugItem(FString DebugData, bool bPoseSource = false);
	FNodeDebugData& BranchFlow(float BranchWeight, FString InNodeDescription = FString());
	FNodeDebugData* GetCachePoseDebugData(float GlobalWeight);

	template<class Type>
	FString GetNodeName(Type* Node)
	{
		FString FinalString = FString::Printf(TEXT("%s<W:%.1f%%> %s"), *Node->StaticStruct()->GetName(), AbsoluteWeight*100.f, *NodeDescription);
		NodeDescription.Empty();
		return FinalString;
	}

	void GetFlattenedDebugData(TArray<FFlattenedDebugData>& FlattenedDebugData, int32 Indent, int32& ChainID);

	TArray<FFlattenedDebugData> GetFlattenedDebugData()
	{
		TArray<FFlattenedDebugData> Data;
		int32 ChainID = 0;
		GetFlattenedDebugData(Data, 0, ChainID);
		return Data;
	}

	// Anim instance that we are generating debug data for
	const UAnimInstance* AnimInstance;
};

/** The display mode of editable values on an animation node. */
UENUM()
namespace EPinHidingMode
{
	enum Type
	{
		/** Never show this property as a pin, it is only editable in the details panel (default for everything but FPoseLink properties). */
		NeverAsPin,

		/** Hide this property by default, but allow the user to expose it as a pin via the details panel. */
		PinHiddenByDefault,

		/** Show this property as a pin by default, but allow the user to hide it via the details panel. */
		PinShownByDefault,

		/** Always show this property as a pin; it never makes sense to edit it in the details panel (default for FPoseLink properties). */
		AlwaysAsPin
	};
}

#define ENABLE_ANIMGRAPH_TRAVERSAL_DEBUG 0

/** A pose link to another node */
USTRUCT(BlueprintInternalUseOnly)
struct ENGINE_API FPoseLinkBase
{
	GENERATED_USTRUCT_BODY()

	/** Serialized link ID, used to build the non-serialized pointer map. */
	UPROPERTY()
	int32 LinkID;

#if WITH_EDITORONLY_DATA
	/** The source link ID, used for debug visualization. */
	UPROPERTY()
	int32 SourceLinkID;
#endif

#if ENABLE_ANIMGRAPH_TRAVERSAL_DEBUG
	FGraphTraversalCounter InitializationCounter;
	FGraphTraversalCounter CachedBonesCounter;
	FGraphTraversalCounter UpdateCounter;
	FGraphTraversalCounter EvaluationCounter;
#endif

protected:
	/** The non serialized node pointer. */
	struct FAnimNode_Base* LinkedNode;

	/** Flag to prevent reentry when dealing with circular trees. */
	bool bProcessed;

public:
	FPoseLinkBase()
		: LinkID(INDEX_NONE)
#if WITH_EDITORONLY_DATA
		, SourceLinkID(INDEX_NONE)
#endif
		, LinkedNode(NULL)
		, bProcessed(false)
	{
	}

	// Interface

	void Initialize(const FAnimationInitializeContext& Context);
	void CacheBones(const FAnimationCacheBonesContext& Context) ;
	void Update(const FAnimationUpdateContext& Context);
	void GatherDebugData(FNodeDebugData& DebugData);

	/** Try to re-establish the linked node pointer. */
	void AttemptRelink(const FAnimationBaseContext& Context);
	/** This only used by custom handlers, and it is advanced feature. */
	void SetLinkNode(struct FAnimNode_Base* NewLinkNode);
	/** This only used by custom handlers, and it is advanced feature. */
	FAnimNode_Base* GetLinkNode();
};

#define ENABLE_ANIMNODE_POSE_DEBUG 0

/** A local-space pose link to another node */
USTRUCT(BlueprintInternalUseOnly)
struct ENGINE_API FPoseLink : public FPoseLinkBase
{
	GENERATED_USTRUCT_BODY()

public:
	// Interface
	void Evaluate(FPoseContext& Output, bool bExpectsAdditivePose = false);

#if ENABLE_ANIMNODE_POSE_DEBUG
private:
	// forwarded pose data from the wired node which current node's skeletal control is not applied yet
	FCompactHeapPose CurrentPose;
#endif //#if ENABLE_ANIMNODE_POSE_DEBUG
};

/** A component-space pose link to another node */
USTRUCT(BlueprintInternalUseOnly)
struct ENGINE_API FComponentSpacePoseLink : public FPoseLinkBase
{
	GENERATED_USTRUCT_BODY()

public:
	// Interface
	void EvaluateComponentSpace(FComponentSpacePoseContext& Output);
};

UENUM()
enum class EPostCopyOperation : uint8
{
	None,

	LogicalNegateBool,
};

UENUM()
enum class ECopyType : uint8
{
	// Just copy the memory
	MemCopy,

	// Read and write properties using bool property helpers, as source/dest could be bitfirld or boolean
	BoolProperty,
	
	// Use struct copy operation, as this needs to correctly handle CPP struct ops
	StructProperty,

	// Read and write properties using object property helpers, as source/dest could be regular/weak/lazy etc.
	ObjectProperty,
};


USTRUCT()
struct FExposedValueCopyRecord
{
	GENERATED_USTRUCT_BODY()

	FExposedValueCopyRecord()
		: SourceProperty_DEPRECATED(nullptr)
		, SourcePropertyName(NAME_None)
		, SourceSubPropertyName(NAME_None)
		, SourceArrayIndex(0)
		, DestProperty(nullptr)
		, DestArrayIndex(0)
		, Size(0)
		, bInstanceIsTarget(false)
		, PostCopyOperation(EPostCopyOperation::None)
		, CopyType(ECopyType::MemCopy)
		, CachedSourceProperty(nullptr)
		, CachedSourceContainer(nullptr)
		, CachedDestContainer(nullptr)
		, Source(nullptr)
		, Dest(nullptr)
	{}

	void PostSerialize(const FArchive& Ar);

	UPROPERTY()
	UProperty* SourceProperty_DEPRECATED;

	UPROPERTY()
	FName SourcePropertyName;

	UPROPERTY()
	FName SourceSubPropertyName;

	UPROPERTY()
	int32 SourceArrayIndex;

	UPROPERTY()
	UProperty* DestProperty;

	UPROPERTY()
	int32 DestArrayIndex;

	UPROPERTY()
	int32 Size;

	// Whether or not the anim instance object is the target for the copy instead of a node.
	UPROPERTY()
	bool bInstanceIsTarget;

	UPROPERTY()
	EPostCopyOperation PostCopyOperation;

	UPROPERTY(Transient)
	ECopyType CopyType;

	// cached source property
	UPROPERTY(Transient)
	UProperty* CachedSourceProperty;

	// cached source container for use with boolean operations
	void* CachedSourceContainer;

	// cached dest container for use with boolean operations
	void* CachedDestContainer;

	// Cached source copy ptr
	void* Source;

	// Cached dest copy ptr
	void* Dest;
};

template<>
struct TStructOpsTypeTraits< FExposedValueCopyRecord > : public TStructOpsTypeTraitsBase2< FExposedValueCopyRecord >
{
	enum
	{
		WithPostSerialize = true,
	};
};

// An exposed value updater
USTRUCT()
struct ENGINE_API FExposedValueHandler
{
	GENERATED_USTRUCT_BODY()

	FExposedValueHandler()
		: BoundFunction(NAME_None)
		, Function(nullptr)
		, bInitialized(false)
	{
	}

	// The function to call to update associated properties (can be NULL)
	UPROPERTY()
	FName BoundFunction;

	// Direct data access to property in anim instance
	UPROPERTY()
	TArray<FExposedValueCopyRecord> CopyRecords;

	// function pointer if BoundFunction != NAME_None
	UFunction* Function;

	// Prevent multiple initialization
	bool bInitialized;

	// Bind copy records and cache UFunction if necessary
	void Initialize(FAnimNode_Base* AnimNode, UObject* AnimInstanceObject);

	// Execute the function and copy records
	void Execute(const FAnimationBaseContext& Context) const;
};

/**
 * This is the base of all runtime animation nodes
 *
 * To create a new animation node:
 *   Create a struct derived from FAnimNode_Base - this is your runtime node
 *   Create a class derived from UAnimGraphNode_Base, containing an instance of your runtime node as a member - this is your visual/editor-only node
 */
USTRUCT()
struct ENGINE_API FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	// The default handler for graph-exposed inputs
	UPROPERTY(meta=(BlueprintCompilerGeneratedDefaults))
	FExposedValueHandler EvaluateGraphExposedInputs;

	/** 
	 * Called when the node first runs. If the node is inside a state machine or cached pose branch then this can be called multiple times. 
	 * This can be called on any thread.
	 * @param	Context		Context structure providing access to relevant data
	 */
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context);

	/** 
	 * Called to cache any bones that this node needs to track (e.g. in a FBoneReference). 
	 * This is usually called at startup when LOD switches occur.
	 * This can be called on any thread.
	 * @param	Context		Context structure providing access to relevant data
	 */
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context);

	/** 
	 * Called to update the state of the graph relative to this node.
	 * Generally this should configure any weights (etc.) that could affect the poses that
	 * will need to be evaluated. This function is what usually executes EvaluateGraphExposedInputs.
	 * This can be called on any thread.
	 * @param	Context		Context structure providing access to relevant data
	 */
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context);

	/** 
	 * Called to evaluate local-space bones transforms according to the weights set up in Update().
	 * You should implement either Evaluate or EvaluateComponentSpace, but not both of these.
	 * This can be called on any thread.
	 * @param	Output		Output structure to write pose or curve data to. Also provides access to relevant data as a context.
	 */
	virtual void Evaluate_AnyThread(FPoseContext& Output);
	virtual void Evaluate_AnyThread(FPoseContext& Output, bool bExpectsAdditivePose);

	/** 
	 * Called to evaluate component-space bone transforms according to the weights set up in Update().
	 * You should implement either Evaluate or EvaluateComponentSpace, but not both of these.
	 * This can be called on any thread.
	 * @param	Output		Output structure to write pose or curve data to. Also provides access to relevant data as a context.
	 */	
	virtual void EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output);
	/** 
	 * If a derived anim node should respond to asset overrides, OverrideAsset should be defined to handle changing the asset 
	 * This is called during anim blueprint compilation to handle child anim blueprints.
	 * @param	NewAsset	The new asset that is being set
	 */
	virtual void OverrideAsset(UAnimationAsset* NewAsset) {}

	/**
	 * Called to gather on-screen debug data. 
	 * This is called on the game thread.
	 * @param	DebugData	Debug data structure used to output any relevant data
	 */
	virtual void GatherDebugData(FNodeDebugData& DebugData)
	{ 
		DebugData.AddDebugItem(FString::Printf(TEXT("Non Overriden GatherDebugData! (%s)"), *DebugData.GetNodeName(this)));
	}

	/**
	 * Whether this node can run its Update() call on a worker thread.
	 * This is called on the game thread.
	 * If any node in a graph returns false from this function, then ALL nodes will update on the game thread.
	 */
	virtual bool CanUpdateInWorkerThread() const { return true; }

	/**
	 * Override this to indicate that PreUpdate() should be called on the game thread (usually to 
	 * This is called on the game thread.
	 * gather non-thread safe data) before Update() is called.
	 */
	virtual bool HasPreUpdate() const { return false; }

	/** Override this to perform game-thread work prior to non-game thread Update() being called */
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) {}

	/**
	 * For nodes that implement some kind of simulation, return true here so ResetDynamics() gets called
	 * when things like teleports, time skips etc. occur that might require special handling
	 * This is called on the game thread.
	 */
	virtual bool NeedsDynamicReset() const { return false; }

	/** Override this to perform game-thread work prior to non-game thread Update() being called */
	virtual void ResetDynamics() {}

	/** Called after compilation */
	virtual void PostCompile(const class USkeleton* InSkeleton) {}

	virtual ~FAnimNode_Base() {}

	/** Deprecated functions */
	DEPRECATED(4.17, "Please use Initialize_AnyThread instead")
	virtual void Initialize(const FAnimationInitializeContext& Context);
	DEPRECATED(4.17, "Please use CacheBones_AnyThread instead")
	virtual void CacheBones(const FAnimationCacheBonesContext& Context) {}
	DEPRECATED(4.17, "Please use Update_AnyThread instead")
	virtual void Update(const FAnimationUpdateContext& Context) {}
	DEPRECATED(4.17, "Please use Evaluate_AnyThread instead")
	virtual void Evaluate(FPoseContext& Output) { check(false); }
	DEPRECATED(4.17, "Please use EvaluateComponentSpace_AnyThread instead")
	virtual void EvaluateComponentSpace(FComponentSpacePoseContext& Output) { check(false); }

protected:
	/** return true if enabled, otherwise, return false. This is utility function that can be used per node level */
	bool IsLODEnabled(FAnimInstanceProxy* AnimInstanceProxy, int32 InLODThreshold);

	/** Deprecated function */
	DEPRECATED(4.17, "Please use OnInitializeAnimInstance instead")
	virtual void RootInitialize(const FAnimInstanceProxy* InProxy) {}

	/** Called once, from game thread as the parent anim instance is created */
	virtual void OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance);

	friend struct FAnimInstanceProxy;
};
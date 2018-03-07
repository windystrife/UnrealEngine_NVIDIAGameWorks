// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_SequenceEvaluator.h"
#include "AnimNodes/AnimNode_ApplyAdditive.h"
#include "AnimNodes/AnimNode_MultiWayBlend.h"
#include "AnimSequencerInstanceProxy.generated.h"

/** Base class for all 'players' that can attach to and be blended into a sequencer instance's output */
struct FSequencerPlayerBase
{
public:
	FSequencerPlayerBase()
		: PoseIndex(INDEX_NONE)
		, bAdditive(false)
	{}

	template<class TType> 
	bool IsOfType() const 
	{
		return IsOfTypeImpl(TType::GetTypeId());
	}

	/** Virtual destructor. */
	virtual ~FSequencerPlayerBase() { }

public:
	/** Index of this players pose in the set of blend slots */
	int32 PoseIndex;

	/** Whether this pose is additive or not */
	bool bAdditive;

protected:
	/**
	* Checks whether this drag and drop operation can cast safely to the specified type.
	*/
	virtual bool IsOfTypeImpl(const FName& Type) const
	{
		return false;
	}

};

/** Quick n dirty RTTI to allow for derived classes to insert nodes of different types */
#define SEQUENCER_INSTANCE_PLAYER_TYPE(TYPE, BASE) \
	static const FName& GetTypeId() { static FName Type(TEXT(#TYPE)); return Type; } \
	virtual bool IsOfTypeImpl(const FName& Type) const override { return GetTypeId() == Type || BASE::IsOfTypeImpl(Type); }

/** Player type that evaluates a sequence-specified UAnimSequence */
struct FSequencerPlayerAnimSequence : public FSequencerPlayerBase
{
	SEQUENCER_INSTANCE_PLAYER_TYPE(FSequencerPlayerAnimSequence, FSequencerPlayerBase)

	struct FAnimNode_SequenceEvaluator PlayerNode;
};

/** Proxy override for this UAnimInstance-derived class */
USTRUCT()
struct ANIMGRAPHRUNTIME_API FAnimSequencerInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

public:
	FAnimSequencerInstanceProxy()
	{
	}

	FAnimSequencerInstanceProxy(UAnimInstance* InAnimInstance)
		: FAnimInstanceProxy(InAnimInstance)
	{
	}

	virtual ~FAnimSequencerInstanceProxy();

	// FAnimInstanceProxy interface
	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual bool Evaluate(FPoseContext& Output) override;
	virtual void UpdateAnimationNode(float DeltaSeconds) override;

	/** Update an animation sequence player in this instance */
	void UpdateAnimTrack(UAnimSequenceBase* InAnimSequence, uint32 SequenceId, float InPosition, float Weight, bool bFireNotifies);

	/** Reset all nodes in this instance */
	virtual void ResetNodes();

protected:
	/** Find a player of a specified type */
	template<typename Type>
	Type* FindPlayer(uint32 SequenceId) const
	{
		FSequencerPlayerBase* Player = SequencerToPlayerMap.FindRef(SequenceId);
		if (Player && Player->IsOfType<Type>())
		{
			return static_cast<Type*>(Player);
		}

		return nullptr;
	}

	/** Construct and link the base part of the blend tree */
	void ConstructNodes();

	/** custom root node for this sequencer player. Didn't want to use RootNode in AnimInstance because it's used with lots of AnimBP functionality */
	struct FAnimNode_ApplyAdditive SequencerRootNode;
	struct FAnimNode_MultiWayBlend FullBodyBlendNode;
	struct FAnimNode_MultiWayBlend AdditiveBlendNode;

	/** mapping from sequencer index to internal player index */
	TMap<uint32, FSequencerPlayerBase*> SequencerToPlayerMap;

	void InitAnimTrack(UAnimSequenceBase* InAnimSequence, uint32 SequenceId);
	void EnsureAnimTrack(UAnimSequenceBase* InAnimSequence, uint32 SequenceId);
	void ClearSequencePlayerMap();
};
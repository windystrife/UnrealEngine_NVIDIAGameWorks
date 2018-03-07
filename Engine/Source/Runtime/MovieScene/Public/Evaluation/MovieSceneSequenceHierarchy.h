// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Evaluation/MovieSceneSequenceTransform.h"
#include "Evaluation/MovieSceneSectionParameters.h"
#include "MovieSceneSequenceID.h"
#include "MovieSceneSequenceHierarchy.generated.h"

class UMovieSceneSequence;
struct FMovieSceneSequenceID;

/**
 * Sub sequence data that is stored within an evaluation template as a backreference to the originating sequence, and section
 */
USTRUCT()
struct FMovieSceneSubSequenceData
{
	GENERATED_BODY()

	/**
	 * Default constructor for serialization
	 */
	FMovieSceneSubSequenceData()
		: Sequence(nullptr)
	{}

	/**
	 * Construction from a movie scene sequence, and a sub section name, and its valid play range
	 */
	FMovieSceneSubSequenceData(UMovieSceneSequence& InSequence, FMovieSceneSequenceID InDeterministicSequenceID
#if WITH_EDITORONLY_DATA
		, FName InSectionPath
		, TRange<float> InValidPlayRange
#endif
		)
		: Sequence(&InSequence)
		, SequenceKeyObject(nullptr)
		, DeterministicSequenceID(InDeterministicSequenceID)
		, PreRollRange(TRange<float>::Empty())
		, PostRollRange(TRange<float>::Empty())
		, HierarchicalBias(0)
#if WITH_EDITORONLY_DATA
		, SectionPath(InSectionPath)
		, ValidPlayRange(InValidPlayRange)
#endif
	{}

	/** The sequence that the sub section references */
	UPROPERTY()
	UMovieSceneSequence* Sequence;

	/** The key object that the sub section uses. Usually either the sequence or the section. */
	UPROPERTY()
	const UObject* SequenceKeyObject;

	/** Transform that transforms a given time from the sequences outer space, to its authored space. */
	UPROPERTY()
	FMovieSceneSequenceTransform RootToSequenceTransform;

	/** Cached signature of the evaluation template */
	UPROPERTY()
	FGuid SourceSequenceSignature;

	/** This sequence's deterministic sequence ID. Used in editor to reduce the risk of collisions on recompilation */ 
	UPROPERTY()
	FMovieSceneSequenceID DeterministicSequenceID;


	/** The sequence preroll range considering the start offset */
	UPROPERTY()
	FFloatRange PreRollRange;

	/** The sequence postroll range considering the start offset */
	UPROPERTY()
	FFloatRange PostRollRange;

	/** The accumulated hierarchical bias of this sequence. Higher bias will take precedence */
	UPROPERTY()
	int32 HierarchicalBias;

#if WITH_EDITORONLY_DATA

	/** This sequence's path within its movie scene */
	UPROPERTY()
	FName SectionPath;

	/** This sub sequence's valid bounds according to its parent sub section. Clamped recursively during template generation */
	UPROPERTY()
	FFloatRange ValidPlayRange;
#endif
};

/**
 * Simple structure specifying parent and child sequence IDs for any given sequences
 */
USTRUCT()
struct FMovieSceneSequenceHierarchyNode
{
	GENERATED_BODY()

	/**
	 * Default construction used by serialization
	 */
	FMovieSceneSequenceHierarchyNode()
	{}

	/**
	 * Construct this hierarchy node from the sequence's parent ID
	 */
	FMovieSceneSequenceHierarchyNode(FMovieSceneSequenceIDRef InParentID)
		: ParentID(InParentID)
	{}

	/** Movie scene sequence ID of this node's parent sequence */
	UPROPERTY()
	FMovieSceneSequenceID ParentID;

	/** Array of child sequences contained within this sequence */
	UPROPERTY()
	TArray<FMovieSceneSequenceID> Children;
};

/**
 * Structure that stores hierarchical information pertaining to all sequences contained within a master sequence
 */
USTRUCT()
struct FMovieSceneSequenceHierarchy
{
	GENERATED_BODY()

	FMovieSceneSequenceHierarchy()
	{
		Hierarchy.Add(MovieSceneSequenceID::Root.GetInternalValue(), FMovieSceneSequenceHierarchyNode(MovieSceneSequenceID::Invalid));
	}

	/**
	 * Find the structural information for the specified sequence ID
	 *
	 * @param SequenceID 			The ID of the sequence to lookup
	 * @return pointer to the structural information, or nullptr if the sequence ID does not exist in this hierarchy 
	 */
	const FMovieSceneSequenceHierarchyNode* FindNode(FMovieSceneSequenceIDRef SequenceID) const
	{
		return Hierarchy.Find(SequenceID.GetInternalValue());
	}

	/**
	 * Find the structural information for the specified sequence ID
	 *
	 * @param SequenceID 			The ID of the sequence to lookup
	 * @return pointer to the structural information, or nullptr if the sequence ID does not exist in this hierarchy 
	 */
	FMovieSceneSequenceHierarchyNode* FindNode(FMovieSceneSequenceIDRef SequenceID)
	{
		return Hierarchy.Find(SequenceID.GetInternalValue());
	}

	/**
	 * Find the sub sequence and section information for the specified sequence ID
	 *
	 * @param SequenceID 			The ID of the sequence to lookup
	 * @return pointer to the sequence/section information, or nullptr if the sequence ID does not exist in this hierarchy 
	 */
	const FMovieSceneSubSequenceData* FindSubData(FMovieSceneSequenceIDRef SequenceID) const
	{
		return SubSequences.Find(SequenceID.GetInternalValue());
	}

	/**
	 * Find the sub sequence and section information for the specified sequence ID
	 *
	 * @param SequenceID 			The ID of the sequence to lookup
	 * @return pointer to the sequence/section information, or nullptr if the sequence ID does not exist in this hierarchy 
	 */
	FMovieSceneSubSequenceData* FindSubData(FMovieSceneSequenceIDRef SequenceID)
	{
		return SubSequences.Find(SequenceID.GetInternalValue());
	}

	/**
	 * Add the specified sub sequence data to the hierarchy
	 *
	 * @param Data 					The data to add
	 * @param ThisSequenceID 		The sequence ID of the sequence the data relates to
	 * @param ParentID 				The parent ID of this sequence data
	 */
	void Add(const FMovieSceneSubSequenceData& Data, FMovieSceneSequenceIDRef ThisSequenceID, FMovieSceneSequenceIDRef ParentID)
	{
		SubSequences.Add(ThisSequenceID.GetInternalValue(), Data);

		FMovieSceneSequenceHierarchyNode Node(ParentID);
		Hierarchy.Add(ThisSequenceID.GetInternalValue(), Node);

		if (ParentID != MovieSceneSequenceID::Invalid)
		{
			FMovieSceneSequenceHierarchyNode* Parent = Hierarchy.Find(ParentID.GetInternalValue());
			check(Parent);
			Parent->Children.Add(ThisSequenceID);
		}
	}

	/** Access to all the subsequence data */
	const TMap<FMovieSceneSequenceID, FMovieSceneSubSequenceData>& AllSubSequenceData() const
	{
		return (const TMap<FMovieSceneSequenceID, FMovieSceneSubSequenceData>&)SubSequences;
	}

private:

	/** Map of all (recursive) sub sequences found in this template, keyed on sequence ID */
	UPROPERTY()
	TMap<uint32, FMovieSceneSubSequenceData> SubSequences;

	/** Structural information describing the structure of the sequence */
	UPROPERTY()
	TMap<uint32, FMovieSceneSequenceHierarchyNode> Hierarchy;
};

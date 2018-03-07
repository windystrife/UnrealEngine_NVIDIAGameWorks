// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/KeyHandle.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "DisplayNodes/SequencerSectionKeyAreaNode.h"
#include "ISequencerSection.h"

class FSequencerTrackNode;
class IKeyArea;

/** Enum of different types of entities that are available in the sequencer */
namespace ESequencerEntity
{
	enum Type
	{
		Key			= 1<<0,
		Section		= 1<<1,
	};

	static const uint32 Everything = (uint32)-1;
};

/** Visitor class used to handle specific sequencer entities */
struct ISequencerEntityVisitor
{
	ISequencerEntityVisitor(uint32 InEntityMask = ESequencerEntity::Everything) : EntityMask(InEntityMask) {}

	virtual void VisitKey(FKeyHandle KeyHandle, float KeyTime, const TSharedPtr<IKeyArea>& KeyArea, UMovieSceneSection* Section, TSharedRef<FSequencerDisplayNode>) const { }
	virtual void VisitSection(UMovieSceneSection* Section, TSharedRef<FSequencerDisplayNode>) const { }
	
	/** Check if the specified type of entity is applicable to this visitor */
	bool CheckEntityMask(ESequencerEntity::Type Type) const { return (EntityMask & Type) != 0; }

protected:
	virtual ~ISequencerEntityVisitor() { }

	/** Bitmask of allowable entities */
	uint32 EntityMask;
};

/** A range specifying time (and possibly vertical) bounds in the sequencer */
struct FSequencerEntityRange
{
	FSequencerEntityRange(const TRange<float>& InRange);
	FSequencerEntityRange(FVector2D TopLeft, FVector2D BottomRight);

	/** Check whether the specified section intersects this range */
	bool IntersectSection(const UMovieSceneSection* InSection, const TSharedRef<FSequencerTrackNode>& InTrackNode, int32 MaxRowIndex) const;

	/** Check whether the specified node intersects this range */
	bool IntersectNode(TSharedRef<FSequencerDisplayNode> InNode) const;

	/** Check whether the specified node's key area intersects this range */
	bool IntersectKeyArea(TSharedRef<FSequencerDisplayNode> InNode, float VirtualKeyHeight) const;

	/** Start/end times */
	float StartTime, EndTime;

	/** Optional vertical bounds */
	TOptional<float> VerticalTop, VerticalBottom;
};

/** Struct used to iterate a two dimensional *visible* range with a user-supplied visitor */
struct FSequencerEntityWalker
{
	/** Construction from the range itself, and an optional virtual key size, where key bounds must be taken into consideration */
	FSequencerEntityWalker(const FSequencerEntityRange& InRange, FVector2D InVirtualKeySize = FVector2D());

	/** Visit the specified nodes (recursively) with this range and a user-supplied visitor */
	void Traverse(const ISequencerEntityVisitor& Visitor, const TArray< TSharedRef<FSequencerDisplayNode> >& Nodes);

private:

	/** Handle visitation of a particular node */
	void HandleNode(const ISequencerEntityVisitor& Visitor, const TSharedRef<FSequencerDisplayNode>& InNode);
	/** Handle visitation of a track node, along with a set of sections */
	void HandleTrackNode(const ISequencerEntityVisitor& Visitor, const TSharedRef<FSequencerTrackNode>& InNode);
	/** Handle visitation of an arbitrary child node, with a filtered set of sections */
	void HandleChildNode(const ISequencerEntityVisitor& Visitor, const TSharedRef<FSequencerDisplayNode>& InNode, const TArray<TSharedRef<ISequencerSection>>& InSections);
	/** Handle a single node that is known to be within the range */
	void HandleSingleNode(const ISequencerEntityVisitor& Visitor, const TSharedRef<FSequencerDisplayNode>& InNode, const TArray<TSharedRef<ISequencerSection>>& InSections);
	/** Handle visitation of a key area node */
	void HandleKeyAreaNode(const ISequencerEntityVisitor& Visitor, const TSharedRef<FSequencerSectionKeyAreaNode>& InKeyAreaNode, const TSharedRef<FSequencerDisplayNode>& InOwnerNode, const TArray<TSharedRef<ISequencerSection>>& InSections);
	/** Handle visitation of a key area */
	void HandleKeyArea(const ISequencerEntityVisitor& Visitor, const TSharedRef<IKeyArea>& KeyArea, UMovieSceneSection* Section, const TSharedRef<FSequencerDisplayNode>& InNode);

	/** The bounds of the range */
	FSequencerEntityRange Range;

	/** Key size in virtual space */
	FVector2D VirtualKeySize;
};

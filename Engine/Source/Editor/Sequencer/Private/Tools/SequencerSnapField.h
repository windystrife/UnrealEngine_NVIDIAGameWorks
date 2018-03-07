// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/KeyHandle.h"
#include "Tools/SequencerEntityVisitor.h"

class IKeyArea;
class ISequencer;

/** Structure defining a point to snap to in the sequencer */
struct FSequencerSnapPoint
{
	enum ESnapType { Key, SectionBounds, CustomSection, PlaybackRange, CurrentTime, InOutRange };

	/** The type of snap */
	ESnapType Type;

	/** The time of the snap */
	float Time;
};

/** Interface that defines how to construct an FSequencerSnapField */
struct ISequencerSnapCandidate
{
	virtual ~ISequencerSnapCandidate() { }

	/** Return true to include the specified key in the snap field */
	virtual bool IsKeyApplicable(FKeyHandle KeyHandle, const TSharedPtr<IKeyArea>& KeyArea, UMovieSceneSection* Section) { return true; }

	/** Return true to include the specified section's bounds in the snap field */
	virtual bool AreSectionBoundsApplicable(UMovieSceneSection* Section) { return true; }

	/** Return true to include the specified section's custom snap points in the snap field */
	virtual bool AreSectionCustomSnapsApplicable(UMovieSceneSection* Section) { return true; }
};

/** A snapping field that provides efficient snapping calculations on a range of values */
class FSequencerSnapField
{
public:

	/** A snap result denoting the time that was snapped, and the resulting snapped time */
	struct FSnapResult
	{
		/** The time before it was snapped */
		float Original;
		/** The time after it was snapped */
		float Snapped;
	};

	/** Construction from a sequencer and a snap canidate implementation. Optionally provide an entity mask to completely ignore some entity types */
	FSequencerSnapField(const ISequencer& InSequencer, ISequencerSnapCandidate& Candidate, uint32 EntityMask = ESequencerEntity::Everything);

	/** Move construction / assignment */
	FSequencerSnapField(FSequencerSnapField&& In) : SortedSnaps(MoveTemp(In.SortedSnaps)) {}
	FSequencerSnapField& operator=(FSequencerSnapField&& In) { SortedSnaps = MoveTemp(In.SortedSnaps); return *this; }
	
	/** Snap the specified time to this field with the given threshold */
	TOptional<float> Snap(float InTime, float Threshold) const;

	/** Snap the specified times to this field with the given threshold. Will return the closest snap value of the entire intersection. */
	TOptional<FSnapResult> Snap(const TArray<float>& InTimes, float Threshold) const;

private:
	/** Array of snap points, approximately grouped, and sorted in ascending order by time */
	TArray<FSequencerSnapPoint> SortedSnaps;
};

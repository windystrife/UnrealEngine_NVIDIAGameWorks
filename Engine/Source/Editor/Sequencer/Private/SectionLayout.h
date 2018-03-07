// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "DisplayNodes/SequencerSectionKeyAreaNode.h"
#include "DisplayNodes/SequencerTrackNode.h"

class FGroupedKeyArea;

/** A layout element specifying the geometry required to render a key area */
struct FSectionLayoutElement
{
	/** Whether this layout element pertains to one or multiple key areas */
	enum EType { Single, Group };

	/** Construct this element from a grouped key area */
	static FSectionLayoutElement FromGroup(const TSharedRef<FSequencerDisplayNode>& InNode, const TSharedRef<FGroupedKeyArea>& InKeyAreaGroup, float InOffset);

	/** Construct this element from a single Key area node */
	static FSectionLayoutElement FromKeyAreaNode(const TSharedRef<FSequencerSectionKeyAreaNode>& InKeyAreaNode, UMovieSceneSection* InSection, float InOffset);

	/** Construct this element from a single Key area node */
	static FSectionLayoutElement FromTrack(const TSharedRef<FSequencerTrackNode>& InTrackNode, UMovieSceneSection* InSection, float InOffset);

	/** Construct this element from a single Key area node */
	static FSectionLayoutElement EmptySpace(const TSharedRef<FSequencerDisplayNode>& InNode, float InOffset);

	/** Retrieve the type of this layout element */
	EType GetType() const;

	/** Retrieve vertical offset from the top of this element's parent */
	float GetOffset() const;

	/** Retrieve the desired height of this element based on the specified parent geometry */
	float GetHeight() const;

	/** Access the key area that this layout element was generated for */
	TSharedPtr<IKeyArea> GetKeyArea() const;

	/** Access the display node that this layout element was generated for */
	TSharedPtr<FSequencerDisplayNode> GetDisplayNode() const;

private:

	/** Pointer to the key area that we were generated from */
	TSharedPtr<IKeyArea> KeyArea;

	/** The specific node that this key area relates to */
	TSharedPtr<FSequencerDisplayNode> DisplayNode;

	/** The type of this layout element */
	EType Type;

	/** The vertical offset from the top of the element's parent */
	float LocalOffset;

	/** Explicit height of the layout element */
	float Height;
};

/** Class used for generating, and caching the layout geometry for a given display node's key areas */
class FSectionLayout
{
public:

	/** Constructor that takes a display node, and the index of the section to layout */
	FSectionLayout(FSequencerTrackNode& TrackNode, int32 SectionIndex);

	/** Get all layout elements that we generated */
	const TArray<FSectionLayoutElement>& GetElements() const;
	
	/** Get the desired total height of this layout */
	float GetTotalHeight() const;

private:
	/** Array of layout elements that we generated */
	TArray<FSectionLayoutElement> Elements;
};

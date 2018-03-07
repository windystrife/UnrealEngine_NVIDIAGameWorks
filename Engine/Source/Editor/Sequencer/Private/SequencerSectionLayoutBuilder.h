// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "IKeyArea.h"
#include "DisplayNodes/SequencerTrackNode.h"
#include "ISectionLayoutBuilder.h"

class FSequencerSectionLayoutBuilder
	: public ISectionLayoutBuilder
{
public:
	FSequencerSectionLayoutBuilder( TSharedRef<FSequencerTrackNode> InRootNode );

public:

	// ISectionLayoutBuilder interface

	virtual void PushCategory( FName CategoryName, const FText& DisplayLabel ) override;
	virtual void SetSectionAsKeyArea( TSharedRef<IKeyArea> KeyArea ) override;
	virtual void AddKeyArea( FName KeyAreaName, const FText& DisplayName, TSharedRef<IKeyArea> KeyArea ) override;
	virtual void PopCategory() override;

	/** Check whether this section layout builder has been given any layout or not */
	bool HasAnyLayout() const
	{
		return bHasAnyLayout;
	}

private:

	/** Root node of the tree */
	TSharedRef<FSequencerTrackNode> RootNode;

	/** The current node that other nodes are added to */
	TSharedRef<FSequencerDisplayNode> CurrentNode;

	/** Boolean indicating whether this section layout builder has been given any layout or not */
	bool bHasAnyLayout;
};

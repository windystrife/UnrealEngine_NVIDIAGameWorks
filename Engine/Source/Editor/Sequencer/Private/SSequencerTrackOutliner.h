// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SBoxPanel.h"

/**
 * The area where outliner nodes for each track is stored
 */
class SSequencerTrackOutliner : public SVerticalBox
{
public:

	void Construct( const FArguments& InArgs );
};

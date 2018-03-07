// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

UENUM()
enum class EMovieSceneKeyInterpolation : uint8
{
	/** Auto. */
	Auto UMETA(DisplayName="Auto"),

	/** User. */
	User UMETA(DisplayName="User"),

	/** Break. */
	Break UMETA(DisplayName="Break"),

	/** Linear. */
	Linear UMETA(DisplayName="Linear"),

	/** Constant. */
	Constant UMETA(DisplayName="Constant"),
};

/** Defines different modes for adding keyframes in sequencer. */
enum class ESequencerKeyMode
{
	/** Keyframes are being generated automatically by the user interacting with object properties or transforms. */
	AutoKey,
	/** The user has requested to manually add a keyframe.  Keys will be added for unchanged and empty components of multi-component tracks depending on current settings. */
	ManualKey,
	/** The user has requested to manually add a keyframe.  Keys will be added for unchanged and empty components of multi-component tracks regardless of the current settings. */
	ManualKeyForced
};

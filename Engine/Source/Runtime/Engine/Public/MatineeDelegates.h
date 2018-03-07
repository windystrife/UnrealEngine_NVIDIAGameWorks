// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** 
 * FMatineeDelegates
 * Delegates used by matinee.
 **/
class ENGINE_API FMatineeDelegates
{
public:
	/** Return a single FMatineeDelegates singleton */
	static FMatineeDelegates& Get();

	/**
	 * Fired whenever a track event keyframe is added
	 *
	 * @param	AMatineeActor	The actor the keyframe belongs to
	 * @param	FName			The name of the keyframe that has been added
	 * @param	int32			The index of the keyframe
	 */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnEventKeyframeAdded, const class AMatineeActor*, const FName&, int32);
	FOnEventKeyframeAdded OnEventKeyframeAdded;

	/**
	 * Fired whenever a track event keyframe is renamed
	 *
	 * @param	AMatineeActor	The actor the keyframe belongs to
	 * @param	FName			The old name of the keyframe
	 * @param	FName			The new name of the keyframe
	 */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnEventKeyframeRenamed, const class AMatineeActor*, const FName&, const FName&);
	FOnEventKeyframeRenamed OnEventKeyframeRenamed;

	/**
	 * Fired whenever a track event keyframe is removed
	 *
	 * @param	AMatineeActor	The actor the keyframe belongs to
	 * @param	FName			The names of the keyframes that have been removed
	 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEventKeyframeRemoved, const class AMatineeActor*, const TArray<FName>&);
	FOnEventKeyframeRemoved OnEventKeyframeRemoved;
};

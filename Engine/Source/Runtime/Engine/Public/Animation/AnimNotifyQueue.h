// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

class USkeletalMeshComponent;
struct FAnimInstanceProxy;
struct FAnimNotifyEvent;

struct FAnimNotifyQueue
{
	FAnimNotifyQueue()
		: PredictedLODLevel(-1)
	{
		RandomStream.Initialize(0x05629063);
	}

	/** Should the notifies current filtering mode stop it from triggering */
	bool PassesFiltering(const FAnimNotifyEvent* Notify) const;

	/** Work out whether this notify should be triggered based on its chance of triggering value */
	bool PassesChanceOfTriggering(const FAnimNotifyEvent* Event) const;

	/** Add anim notifies **/
	void AddAnimNotifies(const TArray<const FAnimNotifyEvent*>& NewNotifies, const float InstanceWeight);

	/** Add anim notifies from montage**/
	void AddAnimNotifies(const TMap<FName, TArray<const FAnimNotifyEvent*>>& NewNotifies, const float InstanceWeight);

	/** Reset queue & update LOD level */
	void Reset(USkeletalMeshComponent* Component);

	/** Append one queue to another */
	void Append(const FAnimNotifyQueue& Queue);
	
	/** 
	 *	Best LOD that was 'predicted' by UpdateSkelPose. Copied form USkeletalMeshComponent.
	 *	This is what bones were updated based on, so we do not allow rendering at a better LOD than this. 
	 */
	int32 PredictedLODLevel;

	/** Internal random stream */
	FRandomStream RandomStream;

	/** Animation Notifies that has been triggered in the latest tick **/
	TArray<const struct FAnimNotifyEvent *> AnimNotifies;

	/** Animation Notifies from montages that still need to be filtered by slot weight*/
	TMap<FName, TArray<const FAnimNotifyEvent*>> UnfilteredMontageAnimNotifies;

	/** Takes the cached notifies from playing montages and adds them if they pass a slot weight check */
	void ApplyMontageNotifies(const FAnimInstanceProxy& Proxy);
private:
	/** Implementation for adding notifies*/
	void AddAnimNotifiesToDest(const TArray<const FAnimNotifyEvent*>& NewNotifies, TArray<const FAnimNotifyEvent*>& DestArray, const float InstanceWeight);
};

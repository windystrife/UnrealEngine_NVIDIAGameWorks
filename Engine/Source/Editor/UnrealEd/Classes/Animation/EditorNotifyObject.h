// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//////////////////////////////////////////////////////////////////////////
// Proxy object for displaying notifies in the details panel with
// event data included alongside UAnimNotify 
//////////////////////////////////////////////////////////////////////////

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimTypes.h"
#include "Animation/EditorAnimBaseObj.h"
#include "EditorNotifyObject.generated.h"

UCLASS(MinimalAPI)
class UEditorNotifyObject : public UEditorAnimBaseObj
{
	GENERATED_UCLASS_BODY()

	/** Set up the editor object
	 *	@param TrackIdx Index of the track the notify belongs to
	 *	@param NotifyIndex index of the notify within the track
	 */
	virtual void InitialiseNotify(int32 TrackIdx, int32 NotifyIndex);
	
	/** Copy changes made to the event object back to the montage asset */
	virtual bool ApplyChangesToMontage() override;

	/** Index of the notify within it's track */
	int32 NotifyIndex;

	/** Index of the track the notify is in */
	int32 TrackIndex;

	/** The notify event to modify */
	UPROPERTY(EditAnywhere, Category=Event)
	FAnimNotifyEvent Event;
};

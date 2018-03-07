// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/EditorNotifyObject.h"
#include "Animation/AnimSequenceBase.h"

UEditorNotifyObject::UEditorNotifyObject(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

bool UEditorNotifyObject::ApplyChangesToMontage()
{
	if(AnimObject)
	{
		check(AnimObject->AnimNotifyTracks.IsValidIndex(TrackIndex));
		FAnimNotifyTrack* Track = &AnimObject->AnimNotifyTracks[TrackIndex];
		check(Track->Notifies.IsValidIndex(NotifyIndex));
		FAnimNotifyEvent* ActualNotify = Track->Notifies[NotifyIndex];
		Event.OnChanged(Event.GetTime());

		// If we have a duration this is a state notify
		if(Event.GetDuration() > 0.0f)
		{
			Event.EndLink.OnChanged(Event.EndLink.GetTime());

			// Always keep link methods in sync between notifies and duration links
			if(Event.GetLinkMethod() != Event.EndLink.GetLinkMethod())
			{
				Event.EndLink.ChangeLinkMethod(Event.GetLinkMethod());
			}
		}
		*ActualNotify = Event;
	}

	return true;
}

void UEditorNotifyObject::InitialiseNotify(int32 InTrackIdx, int32 InNotifyIndex)
{
	if(AnimObject)
	{
		Event = *AnimObject->AnimNotifyTracks[InTrackIdx].Notifies[InNotifyIndex];
		NotifyIndex = InNotifyIndex;
		TrackIndex = InTrackIdx;
	}
}

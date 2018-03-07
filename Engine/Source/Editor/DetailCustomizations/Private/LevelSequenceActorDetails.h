// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Input/Reply.h"
#include "IDetailCustomization.h"

class ALevelSequenceActor;
class IDetailLayoutBuilder;

class FLevelSequenceActorDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();
private:
	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;

	bool CanOpenLevelSequenceForActor() const;
	FReply OnOpenLevelSequenceForActor();

private:
	/** The selected level sequence actor */
	TWeakObjectPtr<ALevelSequenceActor> LevelSequenceActor;
};

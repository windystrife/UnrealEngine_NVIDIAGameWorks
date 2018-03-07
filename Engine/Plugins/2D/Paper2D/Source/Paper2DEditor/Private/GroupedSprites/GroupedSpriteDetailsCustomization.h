// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;

//////////////////////////////////////////////////////////////////////////
// FGroupedSpriteComponentDetailsCustomization

class FGroupedSpriteComponentDetailsCustomization : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	// End of IDetailCustomization interface

protected:
	FGroupedSpriteComponentDetailsCustomization();

	FReply SplitSprites();
	FReply SortSprites();

protected:
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
};

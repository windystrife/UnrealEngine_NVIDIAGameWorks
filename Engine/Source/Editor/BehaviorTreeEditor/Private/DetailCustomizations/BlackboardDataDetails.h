// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;

/** Delegate used to retrieve current blackboard selection */
DECLARE_DELEGATE_RetVal_OneParam(int32, FOnGetSelectedBlackboardItemIndex, bool& /* bIsInherited */);

class FBlackboardDataDetails : public IDetailCustomization
{
public:
	FBlackboardDataDetails(FOnGetSelectedBlackboardItemIndex InOnGetSelectedBlackboardItemIndex)
		: OnGetSelectedBlackboardItemIndex(InOnGetSelectedBlackboardItemIndex)
	{}

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance(FOnGetSelectedBlackboardItemIndex InOnGetSelectedBlackboardItemIndex);

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;

private:
	/** Delegate used to retrieve current blackboard selection */
	FOnGetSelectedBlackboardItemIndex OnGetSelectedBlackboardItemIndex;
};

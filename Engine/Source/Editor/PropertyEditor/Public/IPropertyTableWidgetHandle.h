// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class IPropertyTableWidgetHandle
{
public: 

	virtual void RequestRefresh() = 0;
	virtual TSharedRef<class SWidget> GetWidget() = 0;

};

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class IDetailLayoutBuilder;

/** 
 * Interface for any class that lays out details for a specific class
 */
class IDetailCustomization : public TSharedFromThis<IDetailCustomization>
{
public:
	virtual ~IDetailCustomization() {}

	/** Called when details should be customized */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) = 0;
};

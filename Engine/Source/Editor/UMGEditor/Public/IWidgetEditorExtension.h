// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "Features/IModularFeatures.h"

/**
 * Provides studios a means to better extend and control what happens when widgets are compiled.
 * Generally this is used to run behind designers and vet their creations with scripts that can check
 * additional things during compiling and verify that something is or isn't being done that is expected
 * to be used or not.
 */
class IWidgetEditorExtension : public IModularFeature
{
public:
	static const FName ServiceFeatureName;

	/**
	 * Virtual destructor
	 */
	virtual ~IWidgetEditorExtension() {}




};

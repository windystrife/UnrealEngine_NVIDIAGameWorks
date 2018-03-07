// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "PropertyHandle.h"

class IDetailLayoutBuilder;
class USoundSourceBus;

class FSoundSourceBusDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

private:
	//~ Begin IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	//~ End IDetailCustomization interface */

	void CustomizeCurveDetails(IDetailLayoutBuilder& DetailBuilder);
};

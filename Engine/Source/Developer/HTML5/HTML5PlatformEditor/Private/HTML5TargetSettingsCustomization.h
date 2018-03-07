// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Misc/Attribute.h"
#include "Input/Reply.h"
#include "IDetailCustomization.h"
#include "TargetPlatformAudioCustomization.h"

class IPropertyHandle;
class SEditableTextBox;
class SErrorText;


//////////////////////////////////////////////////////////////////////////
// FHTML5TargetSettingsCustomization

class FHTML5TargetSettingsCustomization : public IDetailCustomization
{
public:
	~FHTML5TargetSettingsCustomization();

	// Makes a new instance of this detail layout class for a specific detail view requesting it
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	// End of IDetailCustomization interface

private:
	FHTML5TargetSettingsCustomization();

	//Audio plugin widget builder:
	FAudioPluginWidgetManager AudioPluginWidgetManager;
};
